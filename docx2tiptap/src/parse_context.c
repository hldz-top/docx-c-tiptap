#include "parse_context.h"

#include <string.h>

void parse_ctx_add_warning(ParseContext *ctx, const char *msg) {
    if (!ctx || !msg || !ctx->arena)
        return;
    if (ctx->warning_count >= DOCX_MAX_WARNINGS)
        return;
    char *copy = arena_strdup(ctx->arena, msg);
    if (!copy)
        return;
    ctx->warning_msgs[ctx->warning_count++] = copy;
}

const char *rel_target_for_id(const RelMap *m, const char *r_id) {
    if (!m || !r_id)
        return NULL;
    for (size_t i = 0; i < m->count; i++) {
        if (m->items[i].id && strcmp(m->items[i].id, r_id) == 0)
            return m->items[i].target;
    }
    return NULL;
}

const char *rel_type_for_id(const RelMap *m, const char *r_id) {
    if (!m || !r_id)
        return NULL;
    for (size_t i = 0; i < m->count; i++) {
        if (m->items[i].id && strcmp(m->items[i].id, r_id) == 0)
            return m->items[i].type;
    }
    return NULL;
}

StyleSemantic style_map_lookup(const StyleMap *m, const char *style_id) {
    if (!m || !style_id)
        return STYLE_UNKNOWN;
    for (size_t i = 0; i < m->count; i++) {
        if (m->items[i].style_id && strcmp(m->items[i].style_id, style_id) == 0)
            return m->items[i].semantic;
    }
    return STYLE_UNKNOWN;
}

static const AbstractNumDef *find_abstract_const(const NumMap *map, int abstract_id) {
    for (size_t i = 0; i < map->abstract_count; i++) {
        if (map->abstracts[i].abstract_id == abstract_id)
            return &map->abstracts[i];
    }
    return NULL;
}

NumFmtKind num_map_fmt_for_para(const NumMap *m, int num_id, int ilvl) {
    if (!m || num_id < 0)
        return NUM_FMT_OTHER;
    int abstract_id = -1;
    for (size_t i = 0; i < m->instance_count; i++) {
        if (m->instances[i].num_id == num_id) {
            abstract_id = m->instances[i].abstract_id;
            break;
        }
    }
    if (abstract_id < 0)
        return NUM_FMT_OTHER;
    const AbstractNumDef *def = find_abstract_const(m, abstract_id);
    if (!def || ilvl < 0 || ilvl >= def->level_count)
        return NUM_FMT_OTHER;
    return def->levels[ilvl].fmt;
}
