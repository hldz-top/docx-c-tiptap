#include "doc_reader.h"

#include "zip_reader.h"
#include "xml_util.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define d2t_casecmp _stricmp
#else
#include <strings.h>
#define d2t_casecmp strcasecmp
#endif

static void rel_target_to_zip_path_doc(const char *target, char *buf, size_t sz) {
    if (!target || !buf || sz == 0) {
        if (buf && sz)
            buf[0] = '\0';
        return;
    }
    if (strncmp(target, "word/", 5) == 0) {
        snprintf(buf, (int)sz, "%s", target);
        return;
    }
    if (strncmp(target, "../", 3) == 0)
        snprintf(buf, (int)sz, "word/%s", target + 3);
    else if (target[0] == '/')
        snprintf(buf, (int)sz, "%s", target + 1);
    else
        snprintf(buf, (int)sz, "word/%s", target);
}

static const char *highlight_word_to_hex(const char *w) {
    if (!w)
        return NULL;
    static const struct {
        const char *n;
        const char *hx;
    } map[] = {
        {"yellow", "#ffff00"},   {"green", "#00ff00"},    {"cyan", "#00ffff"},
        {"magenta", "#ff00ff"},  {"blue", "#0000ff"},     {"red", "#ff0000"},
        {"darkBlue", "#00008b"}, {"darkCyan", "#008b8b"},  {"darkGreen", "#006400"},
        {"darkMagenta", "#8b008b"}, {"darkRed", "#8b0000"}, {"darkYellow", "#bdb76b"},
        {"darkGray", "#a9a9a9"}, {"lightGray", "#d3d3d3"}, {"black", "#000000"},
        {"white", "#ffffff"},    {"none", NULL},
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (d2t_casecmp(w, map[i].n) == 0)
            return map[i].hx;
    }
    return NULL;
}

static char *twips_to_pt_str(Arena *a, int twips) {
    if (twips <= 0)
        return NULL;
    char buf[48];
    if (twips % 20 == 0)
        snprintf(buf, sizeof buf, "%dpt", twips / 20);
    else
        snprintf(buf, sizeof buf, "%.1fpt", (double)twips / 20.0);
    return arena_strdup(a, buf);
}

static xmlNodePtr first_desc_local(xmlNodePtr n, const char *local) {
    if (!n)
        return NULL;
    if (n->type == XML_ELEMENT_NODE && xml_local_eq(n, local))
        return n;
    for (xmlNodePtr c = n->children; c; c = c->next) {
        xmlNodePtr f = first_desc_local(c, local);
        if (f)
            return f;
    }
    return NULL;
}

static size_t count_runs_in_p(xmlNodePtr p) {
    size_t n = 0;
    for (xmlNodePtr c = p->children; c; c = c->next) {
        if (c->type != XML_ELEMENT_NODE)
            continue;
        if (xml_local_eq(c, "r"))
            n++;
        else if (xml_local_eq(c, "hyperlink")) {
            for (xmlNodePtr h = c->children; h; h = h->next)
                if (h->type == XML_ELEMENT_NODE && xml_local_eq(h, "r"))
                    n++;
        } else if (xml_local_eq(c, "fldSimple")) {
            for (xmlNodePtr h = c->children; h; h = h->next)
                if (h->type == XML_ELEMENT_NODE && xml_local_eq(h, "r"))
                    n++;
        }
    }
    return n;
}

static const char *hyperlink_rid(xmlNodePtr hyperlink) {
    for (xmlAttrPtr a = hyperlink->properties; a; a = a->next) {
        if (!a->name || !a->children || !a->children->content)
            continue;
        const char *nm = (const char *)a->name;
        const char *c = strrchr(nm, ':');
        const char *loc = c ? c + 1 : nm;
        if (strcmp(loc, "id") == 0)
            return (const char *)a->children->content;
    }
    return NULL;
}

typedef struct FieldCtx {
    int depth;
    int saw_separate;
    int suppress_result;
} FieldCtx;

static int fld_char_kind(xmlNodePtr r) {
    xmlNodePtr fc = xml_child_local(r, "fldChar");
    if (!fc)
        return -1;
    const xmlChar *t = xml_attr_local(fc, "fldCharType");
    if (!t)
        return -1;
    if (xmlStrcmp(t, BAD_CAST "begin") == 0)
        return 0;
    if (xmlStrcmp(t, BAD_CAST "separate") == 0)
        return 1;
    if (xmlStrcmp(t, BAD_CAST "end") == 0)
        return 2;
    return -1;
}

static void instr_to_upper_buf(const char *s, char *dst, size_t dstsz) {
    size_t j = 0;
    if (!s || !dst || dstsz == 0)
        return;
    for (size_t i = 0; s[i] && j + 1 < dstsz; i++)
        dst[j++] = (char)toupper((unsigned char)s[i]);
    dst[j] = '\0';
}

static int instr_suppresses_visible_result(const char *instr) {
    char u[256];
    instr_to_upper_buf(instr, u, sizeof u);
    if (strstr(u, "PAGEREF"))
        return 0;
    if (strstr(u, "NUMPAGES"))
        return 1;
    if (strstr(u, "PAGE"))
        return 1;
    if (strstr(u, "DATE") || strstr(u, "TIME"))
        return 1;
    return 0;
}

/**
 * Returns 0 if this w:r should not produce an OOXML run (field scaffolding / suppressed result).
 */
static int xml_run_should_emit(FieldCtx *fc, xmlNodePtr r) {
    int fk = fld_char_kind(r);
    if (fk == 0) {
        fc->depth++;
        if (fc->depth == 1) {
            fc->saw_separate = 0;
            fc->suppress_result = 0;
        }
        return 0;
    }
    if (fk == 1) {
        if (fc->depth == 1)
            fc->saw_separate = 1;
        return 0;
    }
    if (fk == 2) {
        if (fc->depth > 0)
            fc->depth--;
        if (fc->depth == 0) {
            fc->saw_separate = 0;
            fc->suppress_result = 0;
        }
        return 0;
    }

    xmlNodePtr instr = first_desc_local(r, "instrText");
    if (instr && instr->children && instr->children->content && fc->depth > 0 && !fc->saw_separate) {
        const char *s = (const char *)instr->children->content;
        if (instr_suppresses_visible_result(s))
            fc->suppress_result = 1;
        return 0;
    }

    if (fc->depth > 0 && fc->saw_separate && fc->suppress_result)
        return 0;

    return 1;
}

static void read_rpr(ParseContext *ctx, xmlNodePtr r, OoxmlRun *out) {
    xmlNodePtr rpr = xml_child_local(r, "rPr");
    if (!rpr)
        return;
    if (xml_child_local(rpr, "b"))
        out->bold = 1;
    if (xml_child_local(rpr, "i"))
        out->italic = 1;
    if (xml_child_local(rpr, "u"))
        out->underline = 1;
    if (xml_child_local(rpr, "strike") || xml_child_local(rpr, "dstrike"))
        out->strike = 1;

    if (!ctx->opts.include_run_style)
        return;

    xmlNodePtr rf = xml_child_local(rpr, "rFonts");
    if (rf) {
        const xmlChar *ea = xml_attr_local(rf, "eastAsia");
        const xmlChar *asc = xml_attr_local(rf, "ascii");
        const xmlChar *ha = xml_attr_local(rf, "hAnsi");
        const char *pick = NULL;
        if (ea && ea[0])
            pick = (const char *)ea;
        else if (asc && asc[0])
            pick = (const char *)asc;
        else if (ha && ha[0])
            pick = (const char *)ha;
        if (pick)
            out->font_family = arena_strdup(ctx->arena, pick);
    }

    int half = 0;
    xmlNodePtr szcs = xml_child_local(rpr, "szCs");
    xmlNodePtr sz = xml_child_local(rpr, "sz");
    if (szcs) {
        const xmlChar *v = xml_attr_local(szcs, "val");
        if (v)
            half = atoi((const char *)v);
    }
    if (half <= 0 && sz) {
        const xmlChar *v = xml_attr_local(sz, "val");
        if (v)
            half = atoi((const char *)v);
    }
    if (half > 0) {
        char buf[32];
        if (half % 2 == 0)
            snprintf(buf, sizeof buf, "%dpt", half / 2);
        else
            snprintf(buf, sizeof buf, "%.1fpt", (double)half / 2.0);
        out->font_size_pt = arena_strdup(ctx->arena, buf);
    }

    xmlNodePtr col = xml_child_local(rpr, "color");
    if (col) {
        const xmlChar *v = xml_attr_local(col, "val");
        if (v && xmlStrcmp(v, BAD_CAST "auto") != 0 && xmlStrlen(v) >= 6) {
            char hx[16];
            snprintf(hx, sizeof hx, "#%.6s", (const char *)v);
            out->color_hex = arena_strdup(ctx->arena, hx);
        }
    }

    xmlNodePtr hi = xml_child_local(rpr, "highlight");
    if (hi) {
        const xmlChar *v = xml_attr_local(hi, "val");
        if (v && xmlStrcmp(v, BAD_CAST "clear") != 0) {
            const char *hx = highlight_word_to_hex((const char *)v);
            if (hx)
                out->highlight_color = arena_strdup(ctx->arena, hx);
        }
    }

    xmlNodePtr va = xml_child_local(rpr, "vertAlign");
    if (va) {
        const xmlChar *v = xml_attr_local(va, "val");
        if (v) {
            if (xmlStrcmp(v, BAD_CAST "superscript") == 0)
                out->superscript = 1;
            if (xmlStrcmp(v, BAD_CAST "subscript") == 0)
                out->subscript = 1;
        }
    }
}

static void parse_w_r(ParseContext *ctx, xmlNodePtr r, OoxmlRun *out, const char *href) {
    memset(out, 0, sizeof(*out));
    out->kind = OOXML_RUN_TEXT;
    read_rpr(ctx, r, out);
    if (href)
        out->href = arena_strdup(ctx->arena, href);

    xmlNodePtr en = xml_child_local(r, "endnoteReference");
    if (en) {
        const xmlChar *eid = xml_attr_local(en, "id");
        out->kind = OOXML_RUN_ENDNOTE_REF;
        out->endnote_id = eid ? atoi((const char *)eid) : -1;
        return;
    }

    xmlNodePtr fn = xml_child_local(r, "footnoteReference");
    if (fn) {
        const xmlChar *fid = xml_attr_local(fn, "id");
        out->kind = OOXML_RUN_FOOTNOTE_REF;
        out->footnote_id = fid ? atoi((const char *)fid) : -1;
        return;
    }

    xmlNodePtr dr = xml_child_local(r, "drawing");
    if (!dr)
        dr = first_desc_local(r, "drawing");
    if (dr) {
        xmlNodePtr blip = first_desc_local(dr, "blip");
        if (blip) {
            const xmlChar *emb = xml_attr_local(blip, "embed");
            if (emb) {
                out->kind = OOXML_RUN_IMAGE;
                out->embed_rid = arena_strdup(ctx->arena, (const char *)emb);
                xmlNodePtr inlineN = first_desc_local(dr, "inline");
                xmlNodePtr anchorN = first_desc_local(dr, "anchor");
                out->image_float = anchorN && !inlineN ? 1 : 0;
                return;
            }
        }
    }

    if (xml_child_local(r, "tab")) {
        out->kind = OOXML_RUN_TAB;
        out->text = arena_strdup(ctx->arena, "\t");
        return;
    }

    if (xml_child_local(r, "br")) {
        out->text = arena_strdup(ctx->arena, "\n");
        return;
    }

    char buf[16384];
    buf[0] = '\0';
    size_t len = 0;
    for (xmlNodePtr c = r->children; c; c = c->next) {
        if (c->type == XML_ELEMENT_NODE && xml_local_eq(c, "t")) {
            const xmlChar *sp = xml_attr_local(c, "space");
            const char *content = c->children && c->children->content ? (const char *)c->children->content : "";
            size_t cl = strlen(content);
            if (len + cl >= sizeof(buf))
                cl = sizeof(buf) - 1 - len;
            if (cl == 0)
                continue;
            memcpy(buf + len, content, cl);
            len += cl;
            buf[len] = '\0';
        }
    }
    if (len > 0)
        out->text = arena_strndup(ctx->arena, buf, len);
}

static void fill_para_meta(ParseContext *ctx, xmlNodePtr p, OoxmlPara *para) {
    Arena *a = ctx->arena;
    para->num_id = -1;
    para->ilvl = 0;
    para->style_id = NULL;
    para->semantic = STYLE_BODY_TEXT;
    para->text_align = NULL;
    para->first_line_indent = NULL;
    para->line_height = NULL;
    para->margin_top = NULL;
    para->margin_bottom = NULL;

    xmlNodePtr ppr = xml_child_local(p, "pPr");
    if (ppr) {
        xmlNodePtr ps = xml_child_local(ppr, "pStyle");
        if (ps) {
            const xmlChar *val = xml_attr_local(ps, "val");
            if (val) {
                para->style_id = arena_strdup(a, (const char *)val);
                para->semantic = style_map_lookup(&ctx->styles, (const char *)val);
            }
        }
        xmlNodePtr np = xml_child_local(ppr, "numPr");
        if (np) {
            xmlNodePtr nid = xml_child_local(np, "numId");
            xmlNodePtr il = xml_child_local(np, "ilvl");
            if (nid) {
                const xmlChar *v = xml_attr_local(nid, "val");
                if (v)
                    para->num_id = atoi((const char *)v);
            }
            if (il) {
                const xmlChar *v = xml_attr_local(il, "val");
                if (v)
                    para->ilvl = atoi((const char *)v);
            }
        }

        if (ctx->opts.include_paragraph_format) {
            xmlNodePtr jc = xml_child_local(ppr, "jc");
            if (jc) {
                const xmlChar *jcv = xml_attr_local(jc, "val");
                if (jcv) {
                    if (xmlStrcmp(jcv, BAD_CAST "center") == 0)
                        para->text_align = arena_strdup(a, "center");
                    else if (xmlStrcmp(jcv, BAD_CAST "right") == 0 || xmlStrcmp(jcv, BAD_CAST "end") == 0)
                        para->text_align = arena_strdup(a, "right");
                    else if (xmlStrcmp(jcv, BAD_CAST "both") == 0 || xmlStrcmp(jcv, BAD_CAST "justify") == 0 ||
                             xmlStrcmp(jcv, BAD_CAST "distribute") == 0)
                        para->text_align = arena_strdup(a, "justify");
                    else
                        para->text_align = arena_strdup(a, "left"); /* start, left, etc. */
                }
            }

            xmlNodePtr ind = xml_child_local(ppr, "ind");
            if (ind) {
                const xmlChar *fl = xml_attr_local(ind, "firstLine");
                if (fl)
                    para->first_line_indent = twips_to_pt_str(a, atoi((const char *)fl));
                else if (xml_attr_local(ind, "firstLineChars") && ctx->opts.include_warnings)
                    parse_ctx_add_warning(ctx, "firstLineChars without firstLine not converted");
            }

            xmlNodePtr spc = xml_child_local(ppr, "spacing");
            if (spc) {
                const xmlChar *before = xml_attr_local(spc, "before");
                const xmlChar *after = xml_attr_local(spc, "after");
                if (before)
                    para->margin_top = twips_to_pt_str(a, atoi((const char *)before));
                if (after)
                    para->margin_bottom = twips_to_pt_str(a, atoi((const char *)after));

                const xmlChar *line = xml_attr_local(spc, "line");
                if (line) {
                    int lv = atoi((const char *)line);
                    const xmlChar *lr = xml_attr_local(spc, "lineRule");
                    char buf[48];
                    if (lr && xmlStrcmp(lr, BAD_CAST "auto") == 0 && lv > 0) {
                        double mult = (double)lv / 240.0;
                        snprintf(buf, sizeof buf, "%g", mult);
                        para->line_height = arena_strdup(a, buf);
                    } else if (lv > 0) {
                        para->line_height = twips_to_pt_str(a, lv);
                    }
                }
            }
        }
    }
}

static OoxmlPara *parse_paragraph(ParseContext *ctx, xmlNodePtr p) {
    Arena *a = ctx->arena;
    OoxmlPara *para = arena_alloc(a, sizeof(OoxmlPara), sizeof(void *));
    if (!para)
        return NULL;
    memset(para, 0, sizeof(*para));
    fill_para_meta(ctx, p, para);

    size_t nrun = count_runs_in_p(p);
    if (nrun == 0) {
        para->runs = NULL;
        para->run_count = 0;
        return para;
    }
    OoxmlRun *runs = arena_alloc(a, nrun * sizeof(OoxmlRun), sizeof(void *));
    if (!runs)
        return para;
    FieldCtx fc;
    memset(&fc, 0, sizeof(fc));
    size_t wi = 0;
    for (xmlNodePtr c = p->children; c; c = c->next) {
        if (c->type != XML_ELEMENT_NODE)
            continue;
        if (xml_local_eq(c, "r")) {
            if (!xml_run_should_emit(&fc, c))
                continue;
            parse_w_r(ctx, c, &runs[wi++], NULL);
        } else if (xml_local_eq(c, "hyperlink")) {
            const char *rid = hyperlink_rid(c);
            const char *href = NULL;
            if (rid) {
                const char *tgt = rel_target_for_id(&ctx->rels, rid);
                const char *typ = rel_type_for_id(&ctx->rels, rid);
                if (tgt && typ && strstr(typ, "hyperlink"))
                    href = arena_strdup(ctx->arena, tgt);
            }
            for (xmlNodePtr h = c->children; h; h = h->next) {
                if (h->type == XML_ELEMENT_NODE && xml_local_eq(h, "r")) {
                    if (!xml_run_should_emit(&fc, h))
                        continue;
                    parse_w_r(ctx, h, &runs[wi++], href);
                }
            }
        } else if (xml_local_eq(c, "fldSimple")) {
            const xmlChar *ins = xml_attr_local(c, "instr");
            if (ins && instr_suppresses_visible_result((const char *)ins))
                continue;
            for (xmlNodePtr h = c->children; h; h = h->next) {
                if (h->type == XML_ELEMENT_NODE && xml_local_eq(h, "r")) {
                    if (!xml_run_should_emit(&fc, h))
                        continue;
                    parse_w_r(ctx, h, &runs[wi++], NULL);
                }
            }
        }
    }
    para->runs = runs;
    para->run_count = wi;
    return para;
}

static OoxmlTableCell *table_find_vm_continue(OoxmlTableRow *row, int g0, int cs);
static void table_apply_vmerge(OoxmlTable *t);

static OoxmlTable *parse_table(ParseContext *ctx, xmlNodePtr tbl) {
    Arena *a = ctx->arena;
    size_t nrows = 0;
    for (xmlNodePtr c = tbl->children; c; c = c->next)
        if (c->type == XML_ELEMENT_NODE && xml_local_eq(c, "tr"))
            nrows++;
    OoxmlTableRow *rows = arena_alloc(a, nrows * sizeof(OoxmlTableRow), sizeof(void *));
    if (!rows)
        return NULL;
    OoxmlTable *t = arena_alloc(a, sizeof(OoxmlTable), sizeof(void *));
    if (!t)
        return NULL;
    memset(t, 0, sizeof(*t));
    t->rows = rows;
    t->row_count = nrows;
    size_t ri = 0;
    for (xmlNodePtr tr = tbl->children; tr; tr = tr->next) {
        if (tr->type != XML_ELEMENT_NODE || !xml_local_eq(tr, "tr"))
            continue;
        size_t ncells = 0;
        for (xmlNodePtr tc = tr->children; tc; tc = tc->next)
            if (tc->type == XML_ELEMENT_NODE && xml_local_eq(tc, "tc"))
                ncells++;
        OoxmlTableCell *cells = arena_alloc(a, ncells * sizeof(OoxmlTableCell), sizeof(void *));
        if (!cells)
            continue;
        memset(cells, 0, ncells * sizeof(OoxmlTableCell));
        rows[ri].cells = cells;
        rows[ri].cell_count = ncells;
        size_t ci = 0;
        int grid_cursor = 0;
        for (xmlNodePtr tc = tr->children; tc; tc = tc->next) {
            if (tc->type != XML_ELEMENT_NODE || !xml_local_eq(tc, "tc"))
                continue;
            if (ci >= ncells)
                break;
            OoxmlTableCell *cell = &cells[ci];
            cell->grid_col = grid_cursor;
            xmlNodePtr tcp = xml_child_local(tc, "tcPr");
            if (tcp) {
                xmlNodePtr gs = xml_child_local(tcp, "gridSpan");
                if (gs) {
                    const xmlChar *v = xml_attr_local(gs, "val");
                    cell->colspan = v ? atoi((const char *)v) : 1;
                } else
                    cell->colspan = 1;
                xmlNodePtr vm = xml_child_local(tcp, "vMerge");
                if (vm) {
                    const xmlChar *v = xml_attr_local(vm, "val");
                    if (v && xmlStrcmp(v, BAD_CAST "restart") == 0)
                        cell->vmerge_restart = 1;
                    else
                        cell->vmerge_continue = 1;
                }
            } else
                cell->colspan = 1;
            if (cell->colspan < 1)
                cell->colspan = 1;
            grid_cursor += cell->colspan;
            size_t npara = 0;
            for (xmlNodePtr x = tc->children; x; x = x->next)
                if (x->type == XML_ELEMENT_NODE && xml_local_eq(x, "p"))
                    npara++;
            cell->paras = arena_alloc(a, npara * sizeof(OoxmlPara *), sizeof(void *));
            cell->para_count = npara;
            size_t pi = 0;
            for (xmlNodePtr x = tc->children; x; x = x->next) {
                if (x->type == XML_ELEMENT_NODE && xml_local_eq(x, "p")) {
                    OoxmlPara *pp = parse_paragraph(ctx, x);
                    if (pp)
                        cell->paras[pi++] = pp;
                }
            }
            cell->para_count = pi;
            ci++;
        }
        rows[ri].cell_count = ci;
        ri++;
    }
    t->row_count = ri;
    table_apply_vmerge(t);
    return t;
}

static OoxmlTableCell *table_find_vm_continue(OoxmlTableRow *row, int g0, int cs) {
    for (size_t i = 0; i < row->cell_count; i++) {
        OoxmlTableCell *c = &row->cells[i];
        if (c->vmerge_continue && c->grid_col == g0 && c->colspan == cs)
            return c;
    }
    return NULL;
}

static void table_apply_vmerge(OoxmlTable *t) {
    if (!t || t->row_count == 0)
        return;
    for (size_t ri = 0; ri < t->row_count; ri++) {
        OoxmlTableRow *row = &t->rows[ri];
        for (size_t ci = 0; ci < row->cell_count; ci++) {
            OoxmlTableCell *c = &row->cells[ci];
            c->rowspan = 1;
            c->skip_emit = 0;
        }
    }
    for (size_t ri = 0; ri < t->row_count; ri++) {
        OoxmlTableRow *row = &t->rows[ri];
        for (size_t ci = 0; ci < row->cell_count; ci++) {
            OoxmlTableCell *c = &row->cells[ci];
            if (c->vmerge_continue) {
                c->skip_emit = 1;
                c->rowspan = 0;
            }
        }
    }
    for (size_t ri = 0; ri < t->row_count; ri++) {
        OoxmlTableRow *row = &t->rows[ri];
        for (size_t ci = 0; ci < row->cell_count; ci++) {
            OoxmlTableCell *c = &row->cells[ci];
            if (c->vmerge_continue)
                continue;
            int rs = 1;
            for (size_t r2 = ri + 1; r2 < t->row_count; r2++) {
                if (!table_find_vm_continue(&t->rows[r2], c->grid_col, c->colspan))
                    break;
                rs++;
            }
            c->rowspan = rs;
        }
    }
}

static xmlNodePtr last_sect_pr_in_document(xmlNodePtr body) {
    xmlNodePtr last = NULL;
    if (!body)
        return NULL;
    for (xmlNodePtr c = body->children; c; c = c->next) {
        if (c->type != XML_ELEMENT_NODE)
            continue;
        if (xml_local_eq(c, "sectPr"))
            last = c;
        else if (xml_local_eq(c, "p")) {
            xmlNodePtr ppr = xml_child_local(c, "pPr");
            if (ppr) {
                xmlNodePtr sp = xml_child_local(ppr, "sectPr");
                if (sp)
                    last = sp;
            }
        }
    }
    return last;
}

static void sect_collect_hf_refs(ParseContext *ctx, xmlNodePtr sect, OoxmlDoc *doc) {
    if (!sect || !doc || !ctx->opts.include_header_footer)
        return;
    for (xmlNodePtr ch = sect->children; ch; ch = ch->next) {
        if (ch->type != XML_ELEMENT_NODE)
            continue;
        if (xml_local_eq(ch, "headerReference")) {
            const xmlChar *ty = xml_attr_local(ch, "type");
            const xmlChar *rid = xml_attr_local(ch, "id");
            if (!rid)
                continue;
            if (!ty || xmlStrcmp(ty, BAD_CAST "default") == 0)
                doc->header_rid = arena_strdup(ctx->arena, (const char *)rid);
        } else if (xml_local_eq(ch, "footerReference")) {
            const xmlChar *ty = xml_attr_local(ch, "type");
            const xmlChar *rid = xml_attr_local(ch, "id");
            if (!rid)
                continue;
            if (!ty || xmlStrcmp(ty, BAD_CAST "default") == 0)
                doc->footer_rid = arena_strdup(ctx->arena, (const char *)rid);
        }
    }
}

static void parse_sect_pr(ParseContext *ctx, xmlNodePtr sect, PageSetupVals *pg) {
    if (!sect || !pg || !ctx->opts.include_page_setup)
        return;
    xmlNodePtr pgSz = xml_child_local(sect, "pgSz");
    xmlNodePtr pgMar = xml_child_local(sect, "pgMar");
    if (pgSz) {
        const xmlChar *w = xml_attr_local(pgSz, "w");
        const xmlChar *h = xml_attr_local(pgSz, "h");
        const xmlChar *orient = xml_attr_local(pgSz, "orient");
        if (w)
            pg->page_width = atoi((const char *)w);
        if (h)
            pg->page_height = atoi((const char *)h);
        if (orient && xmlStrcmp(orient, BAD_CAST "landscape") == 0)
            pg->landscape = 1;
    }
    if (pgMar) {
        const xmlChar *t = xml_attr_local(pgMar, "top");
        const xmlChar *b = xml_attr_local(pgMar, "bottom");
        const xmlChar *l = xml_attr_local(pgMar, "left");
        const xmlChar *r = xml_attr_local(pgMar, "right");
        if (t)
            pg->margin_top = atoi((const char *)t);
        if (b)
            pg->margin_bottom = atoi((const char *)b);
        if (l)
            pg->margin_left = atoi((const char *)l);
        if (r)
            pg->margin_right = atoi((const char *)r);
    }
    pg->has_values = 1;
}

int doc_read_body(ParseContext *ctx, const char *xml, OoxmlDoc *doc) {
    if (!ctx || !xml || !doc)
        return -1;
    xmlDocPtr d = xmlReadMemory(xml, (int)strlen(xml), "document.xml", NULL, 0);
    if (!d)
        return -1;
    xmlNodePtr root = xmlDocGetRootElement(d);
    xmlNodePtr body = root ? xml_child_local(root, "body") : NULL;
    if (!body) {
        xmlFreeDoc(d);
        return -1;
    }

    size_t nb = 0;
    for (xmlNodePtr c = body->children; c; c = c->next) {
        if (c->type != XML_ELEMENT_NODE)
            continue;
        if (xml_local_eq(c, "p") || xml_local_eq(c, "tbl"))
            nb++;
    }

    OoxmlBlock *blocks = arena_alloc(ctx->arena, nb * sizeof(OoxmlBlock), sizeof(void *));
    if (!blocks) {
        xmlFreeDoc(d);
        return -1;
    }
    size_t w = 0;
    for (xmlNodePtr c = body->children; c; c = c->next) {
        if (c->type != XML_ELEMENT_NODE)
            continue;
        if (xml_local_eq(c, "p")) {
            OoxmlPara *p = parse_paragraph(ctx, c);
            if (p) {
                blocks[w].kind = OOXML_BLOCK_PARA;
                blocks[w].u.para = p;
                w++;
            }
        } else if (xml_local_eq(c, "tbl")) {
            OoxmlTable *t = parse_table(ctx, c);
            if (t) {
                blocks[w].kind = OOXML_BLOCK_TABLE;
                blocks[w].u.table = t;
                w++;
            }
        }
    }
    doc->blocks = blocks;
    doc->block_count = w;

    xmlNodePtr sect = last_sect_pr_in_document(body);
    if (sect) {
        parse_sect_pr(ctx, sect, &doc->page);
        sect_collect_hf_refs(ctx, sect, doc);
    }

    xmlFreeDoc(d);
    return 0;
}

void doc_coalesce_lists(ParseContext *ctx, OoxmlDoc *doc) {
    if (!ctx || !doc || doc->block_count == 0)
        return;
    size_t cap = doc->block_count + 4;
    OoxmlBlock *nb = arena_alloc(ctx->arena, cap * sizeof(OoxmlBlock), sizeof(void *));
    if (!nb)
        return;
    size_t out = 0;
    for (size_t i = 0; i < doc->block_count;) {
        OoxmlBlock *b = &doc->blocks[i];
        if (b->kind != OOXML_BLOCK_PARA || b->u.para->num_id < 0) {
            nb[out++] = *b;
            i++;
            continue;
        }
        int nid = b->u.para->num_id;
        int ilvl = b->u.para->ilvl;
        NumFmtKind fmt = num_map_fmt_for_para(&ctx->nums, nid, ilvl);
        size_t j = i + 1;
        while (j < doc->block_count) {
            OoxmlBlock *bj = &doc->blocks[j];
            if (bj->kind != OOXML_BLOCK_PARA || bj->u.para->num_id != nid || bj->u.para->ilvl != ilvl)
                break;
            j++;
        }
        OoxmlList *L = arena_alloc(ctx->arena, sizeof(OoxmlList), sizeof(void *));
        if (!L) {
            i = j;
            continue;
        }
        L->num_id = nid;
        L->ilvl = ilvl;
        L->fmt = fmt;
        L->item_count = j - i;
        L->items = arena_alloc(ctx->arena, L->item_count * sizeof(OoxmlPara *), sizeof(void *));
        if (!L->items) {
            i = j;
            continue;
        }
        for (size_t k = 0; k < L->item_count; k++)
            L->items[k] = doc->blocks[i + k].u.para;
        nb[out].kind = OOXML_BLOCK_LIST;
        nb[out].u.list = L;
        out++;
        i = j;
    }
    doc->blocks = nb;
    doc->block_count = out;
}

static int only_whitespace_text(const char *s) {
    if (!s)
        return 1;
    for (; *s; s++) {
        if (!isspace((unsigned char)*s))
            return 0;
    }
    return 1;
}

static int para_is_image_only_block(OoxmlPara *p) {
    if (!p)
        return 0;
    int imgs = 0;
    for (size_t i = 0; i < p->run_count; i++) {
        OoxmlRun *r = &p->runs[i];
        if (r->kind == OOXML_RUN_IMAGE)
            imgs++;
        else if (r->kind == OOXML_RUN_FOOTNOTE_REF || r->kind == OOXML_RUN_ENDNOTE_REF)
            return 0;
        else if (r->kind == OOXML_RUN_TEXT || r->kind == OOXML_RUN_TAB) {
            if (!only_whitespace_text(r->text))
                return 0;
        }
    }
    return imgs > 0;
}

void doc_merge_figures(ParseContext *ctx, OoxmlDoc *doc) {
    if (!ctx || !doc || doc->block_count == 0)
        return;
    size_t cap = doc->block_count + 2;
    OoxmlBlock *nb = arena_alloc(ctx->arena, cap * sizeof(OoxmlBlock), sizeof(void *));
    if (!nb)
        return;
    size_t w = 0;
    for (size_t i = 0; i < doc->block_count; i++) {
        if (i + 1 < doc->block_count && doc->blocks[i].kind == OOXML_BLOCK_PARA &&
            doc->blocks[i + 1].kind == OOXML_BLOCK_PARA) {
            OoxmlPara *a = doc->blocks[i].u.para;
            OoxmlPara *b = doc->blocks[i + 1].u.para;
            if (para_is_image_only_block(a) && b->semantic == STYLE_CAPTION_FIGURE) {
                OoxmlFigure *fig = arena_alloc(ctx->arena, sizeof(OoxmlFigure), sizeof(void *));
                if (fig) {
                    fig->image_para = a;
                    fig->caption = b;
                    nb[w].kind = OOXML_BLOCK_FIGURE;
                    nb[w].u.figure = fig;
                    w++;
                    i++;
                    continue;
                }
            }
        }
        nb[w++] = doc->blocks[i];
    }
    doc->blocks = nb;
    doc->block_count = w;
}

static int doc_read_xml_part_blocks(ParseContext *ctx, const char *xml, const char *root_local,
                                    OoxmlBlock **out_blocks, size_t *out_count) {
    if (!ctx || !xml || !root_local || !out_blocks || !out_count)
        return -1;
    *out_blocks = NULL;
    *out_count = 0;
    xmlDocPtr d = xmlReadMemory(xml, (int)strlen(xml), "part.xml", NULL, 0);
    if (!d)
        return -1;
    xmlNodePtr root = xmlDocGetRootElement(d);
    if (!root || !xml_local_eq(root, root_local)) {
        xmlFreeDoc(d);
        return -1;
    }

    size_t nb = 0;
    for (xmlNodePtr c = root->children; c; c = c->next) {
        if (c->type != XML_ELEMENT_NODE)
            continue;
        if (xml_local_eq(c, "p") || xml_local_eq(c, "tbl"))
            nb++;
    }

    OoxmlBlock *blocks = arena_alloc(ctx->arena, nb * sizeof(OoxmlBlock), sizeof(void *));
    if (!blocks) {
        xmlFreeDoc(d);
        return -1;
    }
    size_t w = 0;
    for (xmlNodePtr c = root->children; c; c = c->next) {
        if (c->type != XML_ELEMENT_NODE)
            continue;
        if (xml_local_eq(c, "p")) {
            OoxmlPara *p = parse_paragraph(ctx, c);
            if (p) {
                blocks[w].kind = OOXML_BLOCK_PARA;
                blocks[w].u.para = p;
                w++;
            }
        } else if (xml_local_eq(c, "tbl")) {
            OoxmlTable *t = parse_table(ctx, c);
            if (t) {
                blocks[w].kind = OOXML_BLOCK_TABLE;
                blocks[w].u.table = t;
                w++;
            }
        }
    }
    xmlFreeDoc(d);
    *out_blocks = blocks;
    *out_count = w;
    return 0;
}

int doc_read_header_footer(ParseContext *ctx, struct ZipBuf *zip, OoxmlDoc *doc) {
    if (!ctx || !zip || !doc || !ctx->opts.include_header_footer)
        return 0;

    if (doc->header_rid) {
        const char *tgt = rel_target_for_id(&ctx->rels, doc->header_rid);
        if (tgt && tgt[0]) {
            char path[512];
            rel_target_to_zip_path_doc(tgt, path, sizeof path);
            char *hx = zip_extract_string(zip, ctx->arena, path, NULL);
            if (hx) {
                OoxmlBlock *bl = NULL;
                size_t bn = 0;
                if (doc_read_xml_part_blocks(ctx, hx, "hdr", &bl, &bn) == 0) {
                    OoxmlDoc tmp;
                    memset(&tmp, 0, sizeof(tmp));
                    tmp.blocks = bl;
                    tmp.block_count = bn;
                    doc_coalesce_lists(ctx, &tmp);
                    doc->header_blocks = tmp.blocks;
                    doc->header_block_count = tmp.block_count;
                } else
                    parse_ctx_add_warning(ctx, "failed to parse header part");
            } else
                parse_ctx_add_warning(ctx, "missing header xml in package");
        }
    }

    if (doc->footer_rid) {
        const char *tgt = rel_target_for_id(&ctx->rels, doc->footer_rid);
        if (tgt && tgt[0]) {
            char path[512];
            rel_target_to_zip_path_doc(tgt, path, sizeof path);
            char *fx = zip_extract_string(zip, ctx->arena, path, NULL);
            if (fx) {
                OoxmlBlock *bl = NULL;
                size_t bn = 0;
                if (doc_read_xml_part_blocks(ctx, fx, "ftr", &bl, &bn) == 0) {
                    OoxmlDoc tmp;
                    memset(&tmp, 0, sizeof(tmp));
                    tmp.blocks = bl;
                    tmp.block_count = bn;
                    doc_coalesce_lists(ctx, &tmp);
                    doc->footer_blocks = tmp.blocks;
                    doc->footer_block_count = tmp.block_count;
                } else
                    parse_ctx_add_warning(ctx, "failed to parse footer part");
            } else
                parse_ctx_add_warning(ctx, "missing footer xml in package");
        }
    }

    return 0;
}

static int footnote_id_ok(int id) { return id > 0; }

int doc_read_footnotes(ParseContext *ctx, const char *xml, OoxmlDoc *doc) {
    if (!ctx || !xml || !doc)
        return -1;
    xmlDocPtr d = xmlReadMemory(xml, (int)strlen(xml), "footnotes.xml", NULL, 0);
    if (!d)
        return -1;
    xmlNodePtr root = xmlDocGetRootElement(d);
    if (!root) {
        xmlFreeDoc(d);
        return -1;
    }

    size_t nf = 0;
    for (xmlNodePtr c = root->children; c; c = c->next)
        if (c->type == XML_ELEMENT_NODE && xml_local_eq(c, "footnote"))
            nf++;

    FootnoteBody *fbs = arena_alloc(ctx->arena, nf * sizeof(FootnoteBody), sizeof(void *));
    if (!fbs) {
        xmlFreeDoc(d);
        return -1;
    }
    size_t w = 0;
    for (xmlNodePtr fn = root->children; fn; fn = fn->next) {
        if (fn->type != XML_ELEMENT_NODE || !xml_local_eq(fn, "footnote"))
            continue;
        const xmlChar *idv = xml_attr_local(fn, "id");
        int fid = idv ? atoi((const char *)idv) : -1;
        if (!footnote_id_ok(fid))
            continue;
        size_t np = 0;
        for (xmlNodePtr c = fn->children; c; c = c->next)
            if (c->type == XML_ELEMENT_NODE && xml_local_eq(c, "p"))
                np++;
        OoxmlPara **paras = arena_alloc(ctx->arena, np * sizeof(OoxmlPara *), sizeof(void *));
        if (!paras)
            continue;
        size_t pi = 0;
        for (xmlNodePtr c = fn->children; c; c = c->next) {
            if (c->type == XML_ELEMENT_NODE && xml_local_eq(c, "p")) {
                OoxmlPara *p = parse_paragraph(ctx, c);
                if (p)
                    paras[pi++] = p;
            }
        }
        fbs[w].id = fid;
        fbs[w].paras = paras;
        fbs[w].para_count = pi;
        w++;
    }
    doc->footnotes = fbs;
    doc->footnote_count = w;
    xmlFreeDoc(d);
    return 0;
}

int doc_read_endnotes(ParseContext *ctx, const char *xml, OoxmlDoc *doc) {
    if (!ctx || !xml || !doc)
        return -1;
    xmlDocPtr d = xmlReadMemory(xml, (int)strlen(xml), "endnotes.xml", NULL, 0);
    if (!d)
        return -1;
    xmlNodePtr root = xmlDocGetRootElement(d);
    if (!root) {
        xmlFreeDoc(d);
        return -1;
    }

    size_t ne = 0;
    for (xmlNodePtr c = root->children; c; c = c->next)
        if (c->type == XML_ELEMENT_NODE && xml_local_eq(c, "endnote"))
            ne++;

    EndnoteBody *ebs = arena_alloc(ctx->arena, ne * sizeof(EndnoteBody), sizeof(void *));
    if (!ebs) {
        xmlFreeDoc(d);
        return -1;
    }
    size_t w = 0;
    for (xmlNodePtr en = root->children; en; en = en->next) {
        if (en->type != XML_ELEMENT_NODE || !xml_local_eq(en, "endnote"))
            continue;
        const xmlChar *idv = xml_attr_local(en, "id");
        int eid = idv ? atoi((const char *)idv) : -1;
        if (!footnote_id_ok(eid))
            continue;
        size_t np = 0;
        for (xmlNodePtr c = en->children; c; c = c->next)
            if (c->type == XML_ELEMENT_NODE && xml_local_eq(c, "p"))
                np++;
        OoxmlPara **paras = arena_alloc(ctx->arena, np * sizeof(OoxmlPara *), sizeof(void *));
        if (!paras)
            continue;
        size_t pi = 0;
        for (xmlNodePtr c = en->children; c; c = c->next) {
            if (c->type == XML_ELEMENT_NODE && xml_local_eq(c, "p")) {
                OoxmlPara *p = parse_paragraph(ctx, c);
                if (p)
                    paras[pi++] = p;
            }
        }
        ebs[w].id = eid;
        ebs[w].paras = paras;
        ebs[w].para_count = pi;
        w++;
    }
    doc->endnotes = ebs;
    doc->endnote_count = w;
    xmlFreeDoc(d);
    return 0;
}
