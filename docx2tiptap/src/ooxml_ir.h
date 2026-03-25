#ifndef OOXML_IR_H
#define OOXML_IR_H

#include "parse_context.h"

#include <stddef.h>

typedef enum OoxmlRunKind {
    OOXML_RUN_TEXT = 0,
    OOXML_RUN_IMAGE,
    OOXML_RUN_FOOTNOTE_REF,
    OOXML_RUN_ENDNOTE_REF,
    OOXML_RUN_TAB,
} OoxmlRunKind;

typedef struct OoxmlRun {
    OoxmlRunKind kind;
    char *text;
    int bold;
    int italic;
    int underline;
    int strike;
    char *href;
    char *embed_rid;
    int image_float;
    int footnote_id;
    int endnote_id;
    char *font_family;
    char *font_size_pt;
    char *color_hex;
    char *highlight_color;
    int superscript;
    int subscript;
} OoxmlRun;

typedef struct OoxmlPara {
    char *style_id;
    StyleSemantic semantic;
    int num_id;
    int ilvl;
    OoxmlRun *runs;
    size_t run_count;
    char *text_align;
    char *first_line_indent;
    char *line_height;
    char *margin_top;
    char *margin_bottom;
} OoxmlPara;

typedef struct OoxmlList {
    int num_id;
    int ilvl;
    NumFmtKind fmt;
    OoxmlPara **items;
    size_t item_count;
} OoxmlList;

typedef struct OoxmlTableCell {
    OoxmlPara **paras;
    size_t para_count;
    int colspan;
    int vmerge_restart;
    int vmerge_continue;
    int grid_col;
    int rowspan;
    int skip_emit;
} OoxmlTableCell;

typedef struct OoxmlTableRow {
    OoxmlTableCell *cells;
    size_t cell_count;
} OoxmlTableRow;

typedef struct OoxmlTable {
    OoxmlTableRow *rows;
    size_t row_count;
} OoxmlTable;

typedef struct OoxmlFigure {
    OoxmlPara *image_para;
    OoxmlPara *caption;
} OoxmlFigure;

typedef enum OoxmlBlockKind {
    OOXML_BLOCK_PARA = 0,
    OOXML_BLOCK_LIST,
    OOXML_BLOCK_TABLE,
    OOXML_BLOCK_FIGURE,
} OoxmlBlockKind;

typedef struct OoxmlBlock {
    OoxmlBlockKind kind;
    union {
        OoxmlPara *para;
        OoxmlList *list;
        OoxmlTable *table;
        OoxmlFigure *figure;
    } u;
} OoxmlBlock;

typedef struct PageSetupVals {
    int has_values;
    int margin_top;
    int margin_bottom;
    int margin_left;
    int margin_right;
    int page_width;
    int page_height;
    int landscape;
} PageSetupVals;

typedef struct FootnoteBody {
    int id;
    OoxmlPara **paras;
    size_t para_count;
} FootnoteBody;

typedef struct EndnoteBody {
    int id;
    OoxmlPara **paras;
    size_t para_count;
} EndnoteBody;

/** Incomplete; see tiptap_ir.h */
struct TtNode;

typedef struct OoxmlDoc {
    OoxmlBlock *blocks;
    size_t block_count;
    PageSetupVals page;
    FootnoteBody *footnotes;
    size_t footnote_count;
    EndnoteBody *endnotes;
    size_t endnote_count;
    char *header_rid;
    char *footer_rid;
    OoxmlBlock *header_blocks;
    size_t header_block_count;
    OoxmlBlock *footer_blocks;
    size_t footer_block_count;
    struct TtNode *header_tt;
    struct TtNode *footer_tt;
} OoxmlDoc;

#endif
