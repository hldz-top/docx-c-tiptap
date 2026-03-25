#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct Arena {
    uint8_t *base;
    size_t cap;
    size_t used;
    int owns_buffer;
} Arena;

int arena_init(Arena *a, size_t initial_cap);
void arena_free(Arena *a);

void *arena_alloc(Arena *a, size_t size, size_t align);
char *arena_strdup(Arena *a, const char *s);
char *arena_strndup(Arena *a, const char *s, size_t n);

#endif
