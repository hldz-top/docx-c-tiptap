#ifndef RELS_READER_H
#define RELS_READER_H

#include "parse_context.h"

int rels_parse(Arena *a, const char *xml, RelMap *out);

#endif
