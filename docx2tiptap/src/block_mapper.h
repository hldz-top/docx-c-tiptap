#ifndef BLOCK_MAPPER_H
#define BLOCK_MAPPER_H

#include "ooxml_ir.h"
#include "parse_context.h"
#include "tiptap_ir.h"

TtNode *tt_build_doc(ParseContext *ctx, OoxmlDoc *doc);

/** Map a single paragraph to a block-level Tiptap node (paragraph, heading, etc.). */
TtNode *tt_map_paragraph_block(ParseContext *ctx, OoxmlDoc *doc, OoxmlPara *para);

/** Synthetic root type `_frag` with mapped block children (for header/footer JSON). */
TtNode *tt_build_doc_fragment(ParseContext *ctx, OoxmlDoc *doc, OoxmlBlock *blocks, size_t block_count);

#endif
