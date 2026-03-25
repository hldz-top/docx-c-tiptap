#ifndef JSON_WRITER_H
#define JSON_WRITER_H

#include "ooxml_ir.h"
#include "parse_context.h"
#include "tiptap_ir.h"

/** Returns malloc-allocated JSON string (pretty-printed) or NULL. */
char *json_write_doc(const TtNode *root, const ParseContext *ctx, const OoxmlDoc *odoc);

#endif
