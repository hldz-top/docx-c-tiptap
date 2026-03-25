#ifndef MEDIA_MAPPER_H
#define MEDIA_MAPPER_H

#include "parse_context.h"

#include <stddef.h>

/**
 * Build image src string per options: data URI, empty skip, or placeholder.
 * Returns arena-allocated string or NULL.
 */
char *media_build_image_src(ParseContext *ctx, const char *embed_rid, int is_float);

#endif
