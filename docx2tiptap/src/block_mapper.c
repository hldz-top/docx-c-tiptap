#include "block_mapper.h"

#include "arena.h"
#include "inline_mapper.h"
#include "parse_context.h"
#include "table_mapper.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

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

static void tt_add_int_attr(Arena *a, TtNode *n, const char *k, int v) {
    if (!n || n->n_attr >= TT_MAX_ATTR)
        return;
    n->attr_keys[n->n_attr] = arena_strdup(a, k);
    n->attr_int[n->n_attr] = v;
    n->attr_is_int[n->n_attr] = 1;
    n->n_attr++;
}

static void tt_add_str_attr(Arena *a, TtNode *n, const char *k, const char *v) {
    if (!n || n->n_attr >= TT_MAX_ATTR)
        return;
    n->attr_keys[n->n_attr] = arena_strdup(a, k);
    n->attr_str[n->n_attr] = arena_strdup(a, v);
    n->attr_is_int[n->n_attr] = 0;
    n->n_attr++;
}

static void tt_apply_para_format(ParseContext *ctx, TtNode *n, OoxmlPara *para) {
    if (!ctx || !n || !para || !ctx->opts.include_paragraph_format)
        return;
    Arena *a = ctx->arena;
    if (para->text_align && para->text_align[0])
        tt_add_str_attr(a, n, "textAlign", para->text_align);
    if (para->first_line_indent && para->first_line_indent[0])
        tt_add_str_attr(a, n, "firstLineIndent", para->first_line_indent);
    if (para->line_height && para->line_height[0])
        tt_add_str_attr(a, n, "lineHeight", para->line_height);
    if (para->margin_top && para->margin_top[0])
        tt_add_str_attr(a, n, "marginTop", para->margin_top);
    if (para->margin_bottom && para->margin_bottom[0])
        tt_add_str_attr(a, n, "marginBottom", para->margin_bottom);
}

static void para_concat_text(OoxmlPara *p, char *buf, size_t sz) {
    if (!buf || sz == 0)
        return;
    buf[0] = '\0';
    size_t len = 0;
    for (size_t i = 0; i < p->run_count; i++) {
        if (p->runs[i].kind != OOXML_RUN_TEXT && p->runs[i].kind != OOXML_RUN_TAB)
            continue;
        const char *t = p->runs[i].text;
        if (!t)
            continue;
        size_t tl = strlen(t);
        if (len + tl >= sz)
            tl = sz - 1 - len;
        if (tl == 0)
            break;
        memcpy(buf + len, t, tl);
        len += tl;
        buf[len] = '\0';
    }
}

static int looks_hand_numbered(OoxmlPara *p) {
    char b[512];
    para_concat_text(p, b, sizeof b);
    const char *s = b;
    while (*s == ' ' || *s == '\t')
        s++;
    if (!isdigit((unsigned char)s[0]))
        return 0;
    size_t i = 0;
    while (isdigit((unsigned char)s[i]))
        i++;
    return s[i] == '.';
}

static TtNode *make_paragraph_with_runs(ParseContext *ctx, OoxmlDoc *doc, OoxmlPara *para) {
    Arena *a = ctx->arena;
    TtNode *p = tt_new(a, "paragraph");
    if (!p)
        return NULL;
    tt_append_runs(ctx, doc, para->runs, para->run_count, p);
    tt_apply_para_format(ctx, p, para);
    return p;
}

TtNode *tt_map_paragraph_block(ParseContext *ctx, OoxmlDoc *doc, OoxmlPara *para) {
    if (!ctx || !para)
        return NULL;
    Arena *a = ctx->arena;

    if (para->num_id >= 0)
        return make_paragraph_with_runs(ctx, doc, para);

    if (para->semantic == STYLE_UNKNOWN && para->style_id && ctx->opts.include_warnings &&
        ctx->opts.unknown_style_mode == DOCX_UNKNOWN_STYLE_WARNING) {
        char w[256];
        snprintf(w, sizeof w, "unknown style: %s", para->style_id);
        parse_ctx_add_warning(ctx, w);
    }

    if (para->num_id < 0 && looks_hand_numbered(para) && ctx->opts.include_warnings)
        parse_ctx_add_warning(ctx, "possible hand-numbered paragraph (no w:numPr)");

    switch (para->semantic) {
    case STYLE_HEADING_1:
    case STYLE_HEADING_2:
    case STYLE_HEADING_3:
    case STYLE_HEADING_4: {
        TtNode *h = tt_new(a, "heading");
        int lvl = (int)(para->semantic - STYLE_HEADING_1) + 1;
        tt_add_int_attr(a, h, "level", lvl);
        tt_append_runs(ctx, doc, para->runs, para->run_count, h);
        tt_apply_para_format(ctx, h, para);
        return h;
    }
    case STYLE_ABSTRACT_ZH: {
        TtNode *n = tt_new(a, "abstractBlock");
        tt_add_str_attr(a, n, "lang", "zh");
        tt_append_runs(ctx, doc, para->runs, para->run_count, n);
        return n;
    }
    case STYLE_ABSTRACT_EN: {
        TtNode *n = tt_new(a, "abstractBlock");
        tt_add_str_attr(a, n, "lang", "en");
        tt_append_runs(ctx, doc, para->runs, para->run_count, n);
        return n;
    }
    case STYLE_KEYWORDS_ZH: {
        TtNode *n = tt_new(a, "keywordsBlock");
        tt_append_runs(ctx, doc, para->runs, para->run_count, n);
        return n;
    }
    case STYLE_KEYWORDS_EN: {
        TtNode *n = tt_new(a, "keywordsBlock");
        tt_append_runs(ctx, doc, para->runs, para->run_count, n);
        return n;
    }
    case STYLE_REFERENCE_ITEM: {
        TtNode *n = tt_new(a, "referenceItem");
        tt_append_runs(ctx, doc, para->runs, para->run_count, n);
        return n;
    }
    case STYLE_CAPTION_FIGURE:
    case STYLE_CAPTION_TABLE:
        /* Standalone caption without paired figure/table stays as paragraph. */
        return make_paragraph_with_runs(ctx, doc, para);
    default:
        return make_paragraph_with_runs(ctx, doc, para);
    }
}

TtNode *tt_build_doc(ParseContext *ctx, OoxmlDoc *doc) {
    if (!ctx || !doc)
        return NULL;
    Arena *a = ctx->arena;
    TtNode *root = tt_new(a, "doc");
    if (!root)
        return NULL;

    for (size_t i = 0; i < doc->block_count; i++) {
        OoxmlBlock *b = &doc->blocks[i];
        if (b->kind == OOXML_BLOCK_PARA) {
            TtNode *n = tt_map_paragraph_block(ctx, doc, b->u.para);
            if (n)
                tt_add_child(root, n);
        } else if (b->kind == OOXML_BLOCK_TABLE) {
            TtNode *n = tt_map_table(ctx, doc, b->u.table);
            if (n)
                tt_add_child(root, n);
        } else if (b->kind == OOXML_BLOCK_FIGURE) {
            OoxmlFigure *fg = b->u.figure;
            if (fg && fg->image_para) {
                TtNode *fig = tt_new(a, "figure");
                if (fig) {
                    tt_append_runs(ctx, doc, fg->image_para->runs, fg->image_para->run_count, fig);
                    if (fg->caption) {
                        TtNode *cap = tt_new(a, "paragraph");
                        if (cap) {
                            tt_append_runs(ctx, doc, fg->caption->runs, fg->caption->run_count, cap);
                            tt_apply_para_format(ctx, cap, fg->caption);
                        }
                        if (cap && cap->n_children > 0)
                            tt_add_child(fig, cap);
                    }
                    tt_add_child(root, fig);
                }
            }
        } else if (b->kind == OOXML_BLOCK_LIST) {
            OoxmlList *L = b->u.list;
            int bullet = (L->fmt == NUM_FMT_BULLET);
            TtNode *list = tt_new(a, bullet ? "bulletList" : "orderedList");
            if (!list)
                continue;
            for (size_t k = 0; k < L->item_count; k++) {
                TtNode *li = tt_new(a, "listItem");
                if (!li)
                    continue;
                TtNode *inner = tt_map_paragraph_block(ctx, doc, L->items[k]);
                if (!inner) {
                    inner = tt_new(a, "paragraph");
                }
                tt_add_child(li, inner);
                tt_add_child(list, li);
            }
            tt_add_child(root, list);
        }
    }

    return root;
}

TtNode *tt_build_doc_fragment(ParseContext *ctx, OoxmlDoc *doc, OoxmlBlock *blocks, size_t block_count) {
    if (!ctx || !doc || !blocks || block_count == 0)
        return NULL;
    Arena *a = ctx->arena;
    TtNode *root = tt_new(a, "_frag");
    if (!root)
        return NULL;

    for (size_t i = 0; i < block_count; i++) {
        OoxmlBlock *b = &blocks[i];
        TtNode *ch = NULL;
        if (b->kind == OOXML_BLOCK_PARA)
            ch = tt_map_paragraph_block(ctx, doc, b->u.para);
        else if (b->kind == OOXML_BLOCK_TABLE)
            ch = tt_map_table(ctx, doc, b->u.table);
        else if (b->kind == OOXML_BLOCK_FIGURE) {
            OoxmlFigure *fg = b->u.figure;
            if (fg && fg->image_para) {
                ch = tt_new(a, "figure");
                if (ch) {
                    tt_append_runs(ctx, doc, fg->image_para->runs, fg->image_para->run_count, ch);
                    if (fg->caption) {
                        TtNode *cap = tt_new(a, "paragraph");
                        if (cap) {
                            tt_append_runs(ctx, doc, fg->caption->runs, fg->caption->run_count, cap);
                            tt_apply_para_format(ctx, cap, fg->caption);
                        }
                        if (cap && cap->n_children > 0)
                            tt_add_child(ch, cap);
                    }
                }
            }
        } else if (b->kind == OOXML_BLOCK_LIST) {
            OoxmlList *L = b->u.list;
            int bullet = (L->fmt == NUM_FMT_BULLET);
            ch = tt_new(a, bullet ? "bulletList" : "orderedList");
            if (ch) {
                for (size_t k = 0; k < L->item_count; k++) {
                    TtNode *li = tt_new(a, "listItem");
                    if (!li)
                        continue;
                    TtNode *inner = tt_map_paragraph_block(ctx, doc, L->items[k]);
                    if (!inner)
                        inner = tt_new(a, "paragraph");
                    tt_add_child(li, inner);
                    tt_add_child(ch, li);
                }
            }
        }
        if (ch)
            tt_add_child(root, ch);
    }

    return root;
}
