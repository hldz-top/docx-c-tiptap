#include "zip_reader.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <string.h>
#define d2t_stricmp _stricmp
#else
#include <strings.h>
#define d2t_stricmp strcasecmp
#endif

int zip_open_memory(ZipBuf *z, const uint8_t *data, size_t len) {
    if (!z || !data || len == 0)
        return -1;
    memset(&z->arch, 0, sizeof(z->arch));
    if (!mz_zip_reader_init_mem(&z->arch, data, len, 0))
        return -1;
    z->opened = 1;
    return 0;
}

void zip_close(ZipBuf *z) {
    if (!z || !z->opened)
        return;
    mz_zip_reader_end(&z->arch);
    memset(&z->arch, 0, sizeof(z->arch));
    z->opened = 0;
}

int zip_locate_file_ci(ZipBuf *z, const char *path) {
    if (!z || !z->opened || !path)
        return -1;
    mz_uint n = mz_zip_reader_get_num_files(&z->arch);
    for (mz_uint i = 0; i < n; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&z->arch, i, &st))
            continue;
        if (d2t_stricmp(st.m_filename, path) == 0)
            return (int)i;
    }
    return -1;
}

static int extract_index(ZipBuf *z, Arena *a, int idx, int add_nul, uint8_t **out, size_t *out_len) {
    if (idx < 0 || !a || !out)
        return -1;
    size_t uncomp = 0;
    void *p = mz_zip_reader_extract_to_heap(&z->arch, (mz_uint)idx, &uncomp, 0);
    if (!p)
        return -1;
    size_t alloc = uncomp + (add_nul ? 1u : 0u);
    uint8_t *dst = arena_alloc(a, alloc, 1);
    if (!dst) {
        mz_free(p);
        return -1;
    }
    memcpy(dst, p, uncomp);
    if (add_nul)
        dst[uncomp] = '\0';
    mz_free(p);
    *out = dst;
    if (out_len)
        *out_len = uncomp;
    return 0;
}

char *zip_extract_string(ZipBuf *z, Arena *a, const char *path, size_t *out_len) {
    int idx = mz_zip_reader_locate_file(&z->arch, path, NULL, 0);
    if (idx < 0)
        idx = zip_locate_file_ci(z, path);
    if (idx < 0)
        return NULL;
    uint8_t *dst = NULL;
    size_t len = 0;
    if (extract_index(z, a, idx, 1, &dst, &len) != 0)
        return NULL;
    if (out_len)
        *out_len = len;
    return (char *)dst;
}

uint8_t *zip_extract_bytes(ZipBuf *z, Arena *a, const char *path, size_t *out_len) {
    int idx = mz_zip_reader_locate_file(&z->arch, path, NULL, 0);
    if (idx < 0)
        idx = zip_locate_file_ci(z, path);
    if (idx < 0)
        return NULL;
    uint8_t *dst = NULL;
    size_t len = 0;
    if (extract_index(z, a, idx, 0, &dst, &len) != 0)
        return NULL;
    if (out_len)
        *out_len = len;
    return dst;
}
