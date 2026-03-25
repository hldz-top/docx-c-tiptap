#ifndef TABLE_MAPPER_H
#define TABLE_MAPPER_H

#include "ooxml_ir.h"
#include "parse_context.h"
#include "tiptap_ir.h"

TtNode *tt_map_table(ParseContext *ctx, OoxmlDoc *doc, OoxmlTable *tbl);

#endif
