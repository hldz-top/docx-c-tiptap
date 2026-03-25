#include "arena.h"
#include "zip_reader.h"

#include <miniz.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    mz_zip_archive za;
    memset(&za, 0, sizeof(za));
    assert(mz_zip_writer_init_heap(&za, 0, 64 * 1024));
    assert(mz_zip_writer_add_mem(&za, "hello.txt", "world", 5, MZ_BEST_COMPRESSION));

    void *pbuf = NULL;
    size_t sz = 0;
    assert(mz_zip_writer_finalize_heap_archive(&za, &pbuf, &sz));
    mz_zip_writer_end(&za);

    ZipBuf z;
    assert(zip_open_memory(&z, pbuf, sz) == 0);
    Arena a;
    assert(arena_init(&a, 4096) == 0);
    size_t elen = 0;
    char *txt = zip_extract_string(&z, &a, "hello.txt", &elen);
    assert(txt && elen == 5 && memcmp(txt, "world", 5) == 0);
    zip_close(&z);
    arena_free(&a);
    mz_free(pbuf);
    return 0;
}
