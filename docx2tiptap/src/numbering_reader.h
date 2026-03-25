#ifndef NUMBERING_READER_H
#define NUMBERING_READER_H

#include "parse_context.h"

int numbering_parse(Arena *a, const char *xml, NumMap *out);

#endif
