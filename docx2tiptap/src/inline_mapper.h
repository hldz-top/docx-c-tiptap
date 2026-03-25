#ifndef INLINE_MAPPER_H
#define INLINE_MAPPER_H

#include "ooxml_ir.h"
#include "parse_context.h"
#include "tiptap_ir.h"

int tt_append_runs(ParseContext *ctx, OoxmlDoc *doc, OoxmlRun *runs, size_t run_count, TtNode *parent);

#endif
