#include "table_mapper.h"

#include "arena.h"
#include "block_mapper.h"
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

TtNode *tt_map_table(ParseContext *ctx, OoxmlDoc *doc, OoxmlTable *tbl) {
    if (!ctx || !tbl)
        return NULL;
    Arena *a = ctx->arena;
    TtNode *table = tt_new(a, "table");
    if (!table)
        return NULL;
    for (size_t ri = 0; ri < tbl->row_count; ri++) {
        OoxmlTableRow *row = &tbl->rows[ri];
        TtNode *tr = tt_new(a, "tableRow");
        if (!tr)
            continue;
        for (size_t ci = 0; ci < row->cell_count; ci++) {
            OoxmlTableCell *cell = &row->cells[ci];
            if (cell->skip_emit)
                continue;
            TtNode *tc = tt_new(a, "tableCell");
            if (!tc)
                continue;
            if (cell->colspan > 1)
                tt_add_int_attr(a, tc, "colspan", cell->colspan);
            if (cell->rowspan > 1)
                tt_add_int_attr(a, tc, "rowspan", cell->rowspan);
            for (size_t pi = 0; pi < cell->para_count; pi++) {
                TtNode *blk = tt_map_paragraph_block(ctx, doc, cell->paras[pi]);
                if (blk)
                    tt_add_child(tc, blk);
            }
            tt_add_child(tr, tc);
        }
        tt_add_child(table, tr);
    }
    return table;
}
