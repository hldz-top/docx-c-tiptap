#include "arena.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    Arena a;
    assert(arena_init(&a, 1024) == 0);
    void *p1 = arena_alloc(&a, 32, 8);
    void *p2 = arena_alloc(&a, 32, 8);
    assert(p1 && p2);
    assert((uintptr_t)p1 % 8 == 0);
    char *s = arena_strdup(&a, "hello");
    assert(s && strcmp(s, "hello") == 0);
    arena_free(&a);
    return 0;
}
