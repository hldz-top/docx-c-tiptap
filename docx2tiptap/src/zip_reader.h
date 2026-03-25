#ifndef ZIP_READER_H
#define ZIP_READER_H

#include "arena.h"

#include <miniz.h>

#include <stddef.h>
#include <stdint.h>

typedef struct ZipBuf {
    mz_zip_archive arch;
    int opened;
} ZipBuf;

int zip_open_memory(ZipBuf *z, const uint8_t *data, size_t len);
void zip_close(ZipBuf *z);

/**
 * Extract ZIP member into arena (NUL-terminated). Returns NULL if missing.
 * For binary members, out_len excludes the added NUL.
 */
char *zip_extract_string(ZipBuf *z, Arena *a, const char *path, size_t *out_len);

/** Binary extract into arena (no extra NUL). */
uint8_t *zip_extract_bytes(ZipBuf *z, Arena *a, const char *path, size_t *out_len);

/** Case-insensitive path locate (for OOXML). */
int zip_locate_file_ci(ZipBuf *z, const char *path);

#endif
