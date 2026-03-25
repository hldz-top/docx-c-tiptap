#include "inline_mapper.h"

#include "arena.h"
#include "media_mapper.h"

#include <string.h>

/* Defined in block_mapper.c — avoid including block_mapper.h (circular). */
TtNode *tt_map_paragraph_block(ParseContext *ctx, OoxmlDoc *doc, OoxmlPara *para);

static TtNode *tt_new(Arena *a, const char *type) {
    TtNode *n = arena_alloc(a, sizeof(TtNode), sizeof(void *));
    if (!n)
        return NULL;
    memset(n, 0, sizeof(*n));
    n->type = arena_strdup(a, type);
    return n;
}

static void tt_add_child(TtNode *p, TtNode *c) {
    if (!p || !c || p->n_children >= TT_MAX_CHILD)
        return;
    p->children[p->n_children++] = c;
}

static void tt_mark_add_kv(Arena *a, TtMark *m, const char *k, const char *v) {
    if (!m || m->n_attr >= TT_MAX_MARK_ATTR || !k || !v)
        return;
    m->attr_keys[m->n_attr] = arena_strdup(a, k);
    m->attr_str[m->n_attr] = arena_strdup(a, v);
    m->n_attr++;
}

static void tt_push_mark_simple(Arena *a, TtNode *n, const char *mtype) {
    if (!n || n->n_leaf_marks >= 8)
        return;
    TtMark *m = &n->leaf_marks[n->n_leaf_marks++];
    memset(m, 0, sizeof(*m));
    m->type = arena_strdup(a, mtype);
}

static void tt_push_mark_link(Arena *a, TtNode *n, const char *href) {
    if (!n || n->n_leaf_marks >= 8 || !href)
        return;
    TtMark *m = &n->leaf_marks[n->n_leaf_marks++];
    memset(m, 0, sizeof(*m));
    m->type = arena_strdup(a, "link");
    m->href = arena_strdup(a, href);
    tt_mark_add_kv(a, m, "href", href);
}

static FootnoteBody *find_footnote(OoxmlDoc *doc, int id) {
    if (!doc || !doc->footnotes)
        return NULL;
    for (size_t i = 0; i < doc->footnote_count; i++) {
        if (doc->footnotes[i].id == id)
            return &doc->footnotes[i];
    }
    return NULL;
}

static EndnoteBody *find_endnote(OoxmlDoc *doc, int id) {
    if (!doc || !doc->endnotes)
        return NULL;
    for (size_t i = 0; i < doc->endnote_count; i++) {
        if (doc->endnotes[i].id == id)
            return &doc->endnotes[i];
    }
    return NULL;
}

int tt_append_runs(ParseContext *ctx, OoxmlDoc *doc, OoxmlRun *runs, size_t run_count, TtNode *parent) {
    if (!ctx || !parent)
        return -1;
    Arena *a = ctx->arena;
    for (size_t r = 0; r < run_count; r++) {
        OoxmlRun *run = &runs[r];
        if (run->kind == OOXML_RUN_IMAGE) {
            TtNode *im = tt_new(a, "image");
            char *src = media_build_image_src(ctx, run->embed_rid, run->image_float);
            if (src && src[0] && im->n_attr < TT_MAX_ATTR) {
                im->attr_keys[im->n_attr] = arena_strdup(a, "src");
                im->attr_str[im->n_attr] = src;
                im->attr_is_int[im->n_attr] = 0;
                im->n_attr++;
            }
            if (run->image_float && im->n_attr < TT_MAX_ATTR) {
                im->attr_keys[im->n_attr] = arena_strdup(a, "layout");
                im->attr_str[im->n_attr] = arena_strdup(a, "float");
                im->attr_is_int[im->n_attr] = 0;
                im->n_attr++;
            }
            tt_add_child(parent, im);
            continue;
        }
        if (run->kind == OOXML_RUN_ENDNOTE_REF) {
            TtNode *en = tt_new(a, "endnote");
            if (en->n_attr < TT_MAX_ATTR) {
                en->attr_keys[en->n_attr] = arena_strdup(a, "id");
                en->attr_int[en->n_attr] = run->endnote_id;
                en->attr_is_int[en->n_attr] = 1;
                en->n_attr++;
            }
            EndnoteBody *eb = doc ? find_endnote(doc, run->endnote_id) : NULL;
            if (eb) {
                for (size_t pi = 0; pi < eb->para_count; pi++) {
                    TtNode *blk = tt_map_paragraph_block(ctx, doc, eb->paras[pi]);
                    if (blk)
                        tt_add_child(en, blk);
                }
            }
            tt_add_child(parent, en);
            continue;
        }
        if (run->kind == OOXML_RUN_FOOTNOTE_REF) {
            TtNode *fn = tt_new(a, "footnote");
            if (fn->n_attr < TT_MAX_ATTR) {
                fn->attr_keys[fn->n_attr] = arena_strdup(a, "id");
                fn->attr_int[fn->n_attr] = run->footnote_id;
                fn->attr_is_int[fn->n_attr] = 1;
                fn->n_attr++;
            }
            FootnoteBody *fb = doc ? find_footnote(doc, run->footnote_id) : NULL;
            if (fb) {
                for (size_t pi = 0; pi < fb->para_count; pi++) {
                    TtNode *blk = tt_map_paragraph_block(ctx, doc, fb->paras[pi]);
                    if (blk)
                        tt_add_child(fn, blk);
                }
            }
            tt_add_child(parent, fn);
            continue;
        }
        const char *txt = NULL;
        if (run->kind == OOXML_RUN_TAB)
            txt = "\t";
        else
            txt = run->text;
        if (!txt || !txt[0])
            continue;
        TtNode *tn = tt_new(a, "text");
        tn->leaf_text = arena_strdup(a, txt);
        if (run->bold)
            tt_push_mark_simple(a, tn, "bold");
        if (run->italic)
            tt_push_mark_simple(a, tn, "italic");
        if (run->underline)
            tt_push_mark_simple(a, tn, "underline");
        if (run->strike)
            tt_push_mark_simple(a, tn, "strike");
        if (ctx->opts.include_run_style) {
            int has_ts = (run->font_family && run->font_family[0]) || (run->font_size_pt && run->font_size_pt[0]) ||
                         (run->color_hex && run->color_hex[0]);
            if (has_ts) {
                if (tn->n_leaf_marks < 8) {
                    TtMark *m = &tn->leaf_marks[tn->n_leaf_marks++];
                    memset(m, 0, sizeof(*m));
                    m->type = arena_strdup(a, "textStyle");
                    if (run->font_family && run->font_family[0])
                        tt_mark_add_kv(a, m, "fontFamily", run->font_family);
                    if (run->font_size_pt && run->font_size_pt[0])
                        tt_mark_add_kv(a, m, "fontSize", run->font_size_pt);
                    if (run->color_hex && run->color_hex[0])
                        tt_mark_add_kv(a, m, "color", run->color_hex);
                }
            }
            if (run->highlight_color && run->highlight_color[0] && tn->n_leaf_marks < 8) {
                TtMark *m = &tn->leaf_marks[tn->n_leaf_marks++];
                memset(m, 0, sizeof(*m));
                m->type = arena_strdup(a, "highlight");
                tt_mark_add_kv(a, m, "color", run->highlight_color);
            }
            if (run->superscript)
                tt_push_mark_simple(a, tn, "superscript");
            if (run->subscript)
                tt_push_mark_simple(a, tn, "subscript");
        }
        if (run->href && run->href[0])
            tt_push_mark_link(a, tn, run->href);
        tt_add_child(parent, tn);
    }
    return 0;
}
