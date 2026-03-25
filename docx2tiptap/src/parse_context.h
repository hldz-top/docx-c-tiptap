#ifndef PARSE_CONTEXT_H
#define PARSE_CONTEXT_H

#include "docx2tiptap.h"

#include "arena.h"
#include "zip_reader.h"

#include <stddef.h>

typedef struct RelEntry {
    char *id;
    char *type;
    char *target;
} RelEntry;

typedef struct RelMap {
    RelEntry *items;
    size_t count;
} RelMap;

typedef enum StyleSemantic {
    STYLE_BODY_TEXT = 0,
    STYLE_HEADING_1,
    STYLE_HEADING_2,
    STYLE_HEADING_3,
    STYLE_HEADING_4,
    STYLE_ABSTRACT_ZH,
    STYLE_ABSTRACT_EN,
    STYLE_KEYWORDS_ZH,
    STYLE_KEYWORDS_EN,
    STYLE_REFERENCE_ITEM,
    STYLE_CAPTION_FIGURE,
    STYLE_CAPTION_TABLE,
    STYLE_UNKNOWN,
} StyleSemantic;

typedef struct StyleEntry {
    char *style_id;
    StyleSemantic semantic;
    char *display_name; /* lowercased w:name for matching */
} StyleEntry;

typedef struct StyleMap {
    StyleEntry *items;
    size_t count;
} StyleMap;

typedef enum NumFmtKind {
    NUM_FMT_BULLET = 0,
    NUM_FMT_DECIMAL,
    NUM_FMT_LOWER_LETTER,
    NUM_FMT_UPPER_LETTER,
    NUM_FMT_LOWER_ROMAN,
    NUM_FMT_UPPER_ROMAN,
    NUM_FMT_OTHER,
} NumFmtKind;

typedef struct AbstractLevelFmt {
    NumFmtKind fmt;
} AbstractLevelFmt;

typedef struct AbstractNumDef {
    int abstract_id;
    AbstractLevelFmt *levels;
    int level_count;
} AbstractNumDef;

typedef struct NumInstance {
    int num_id;
    int abstract_id;
} NumInstance;

typedef struct NumMap {
    AbstractNumDef *abstracts;
    size_t abstract_count;
    NumInstance *instances;
    size_t instance_count;
} NumMap;

#define DOCX_MAX_WARNINGS 64

typedef struct ParseContext {
    DocxOptions opts;
    Arena *arena;
    ZipBuf *zip;
    RelMap rels;
    StyleMap styles;
    NumMap nums;
    char *warning_msgs[DOCX_MAX_WARNINGS];
    int warning_count;
} ParseContext;

void parse_ctx_add_warning(ParseContext *ctx, const char *msg);

const char *rel_target_for_id(const RelMap *m, const char *r_id);
const char *rel_type_for_id(const RelMap *m, const char *r_id);

StyleSemantic style_map_lookup(const StyleMap *m, const char *style_id);

/** num_id -> ordered vs bullet for level 0 (simplified). */
NumFmtKind num_map_fmt_for_para(const NumMap *m, int num_id, int ilvl);

#endif
