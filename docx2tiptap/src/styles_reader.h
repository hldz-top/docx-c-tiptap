#ifndef STYLES_READER_H
#define STYLES_READER_H

#include "parse_context.h"

int styles_parse(Arena *a, const char *xml, StyleMap *out);

#endif
