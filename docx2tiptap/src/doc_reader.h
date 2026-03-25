#ifndef DOC_READER_H
#define DOC_READER_H

#include "ooxml_ir.h"
#include "parse_context.h"

struct ZipBuf;

int doc_read_footnotes(ParseContext *ctx, const char *xml, OoxmlDoc *doc);
int doc_read_endnotes(ParseContext *ctx, const char *xml, OoxmlDoc *doc);
int doc_read_body(ParseContext *ctx, const char *xml, OoxmlDoc *doc);
/** Load default header/footer parts when include_header_footer is set and rIds are present. */
int doc_read_header_footer(ParseContext *ctx, struct ZipBuf *zip, OoxmlDoc *doc);
void doc_coalesce_lists(ParseContext *ctx, OoxmlDoc *doc);
void doc_merge_figures(ParseContext *ctx, OoxmlDoc *doc);

#endif
