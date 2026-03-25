#include "arena.h"

#include <stdlib.h>
#include <string.h>

enum { kDefaultCap = 4u * 1024u * 1024u };

static size_t align_up(size_t n, size_t align) {
    return (n + align - 1) & ~(align - 1);
}

int arena_init(Arena *a, size_t initial_cap) {
    if (!a)
        return -1;
    size_t cap = initial_cap ? initial_cap : kDefaultCap;
    a->base = (uint8_t *)malloc(cap);
    if (!a->base)
        return -1;
    a->cap = cap;
    a->used = 0;
    a->owns_buffer = 1;
    return 0;
}

void arena_free(Arena *a) {
    if (!a || !a->base)
        return;
    if (a->owns_buffer)
        free(a->base);
    a->base = NULL;
    a->cap = a->used = 0;
}

void *arena_alloc(Arena *a, size_t size, size_t align) {
    if (!a || !a->base || align == 0 || (align & (align - 1)) != 0)
        return NULL;
    size_t start = align_up(a->used, align);
    if (start > a->cap || size > a->cap - start)
        return NULL;
    void *p = a->base + start;
    a->used = start + size;
    return p;
}

char *arena_strdup(Arena *a, const char *s) {
    if (!s)
        return NULL;
    size_t n = strlen(s) + 1;
    char *d = arena_alloc(a, n, sizeof(char));
    if (!d)
        return NULL;
    memcpy(d, s, n);
    return d;
}

char *arena_strndup(Arena *a, const char *s, size_t n) {
    if (!s)
        return NULL;
    char *d = arena_alloc(a, n + 1, sizeof(char));
    if (!d)
        return NULL;
    memcpy(d, s, n);
    d[n] = '\0';
    return d;
}
