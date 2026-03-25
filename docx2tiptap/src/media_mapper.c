#include "media_mapper.h"

#include "arena.h"
#include "parse_context.h"
#include "zip_reader.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define d2t_casecmp _stricmp
#else
#include <strings.h>
#define d2t_casecmp strcasecmp
#endif

static void rel_target_to_zip_path(const char *target, char *buf, size_t sz) {
    if (!target || !buf || sz == 0) {
        if (buf && sz)
            buf[0] = '\0';
        return;
    }
    if (strncmp(target, "word/", 5) == 0) {
        snprintf(buf, (int)sz, "%s", target);
        return;
    }
    if (strncmp(target, "../", 3) == 0)
        snprintf(buf, (int)sz, "word/%s", target + 3);
    else if (target[0] == '/')
        snprintf(buf, (int)sz, "%s", target + 1);
    else
        snprintf(buf, (int)sz, "word/%s", target);
}

static const char *guess_mime(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot)
        return "application/octet-stream";
    if (d2t_casecmp(dot, ".png") == 0)
        return "image/png";
    if (d2t_casecmp(dot, ".jpg") == 0 || d2t_casecmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (d2t_casecmp(dot, ".gif") == 0)
        return "image/gif";
    if (d2t_casecmp(dot, ".webp") == 0)
        return "image/webp";
    return "application/octet-stream";
}

static const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *base64_alloc(Arena *a, const uint8_t *data, size_t len) {
    size_t out_len = (len + 2) / 3 * 4;
    char *out = arena_alloc(a, out_len + 1, sizeof(char));
    if (!out)
        return NULL;
    size_t i, j = 0;
    for (i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        int has1 = i + 1 < len;
        int has2 = i + 2 < len;
        if (has1)
            n |= (uint32_t)data[i + 1] << 8;
        if (has2)
            n |= (uint32_t)data[i + 2];
        out[j++] = b64[(n >> 18) & 63];
        out[j++] = b64[(n >> 12) & 63];
        out[j++] = has1 ? b64[(n >> 6) & 63] : '=';
        out[j++] = has2 ? b64[n & 63] : '=';
    }
    out[j] = '\0';
    return out;
}

char *media_build_image_src(ParseContext *ctx, const char *embed_rid, int is_float) {
    (void)is_float;
    if (!ctx || !embed_rid || !ctx->zip)
        return NULL;
    const char *target = rel_target_for_id(&ctx->rels, embed_rid);
    if (!target)
        return NULL;
    char path[512];
    rel_target_to_zip_path(target, path, sizeof path);

    if (ctx->opts.image_strategy == DOCX_IMAGE_SKIP)
        return arena_strdup(ctx->arena, "");

    if (ctx->opts.image_strategy == DOCX_IMAGE_PLACEHOLDER_URL) {
        char ph[640];
        snprintf(ph, sizeof ph, "docx-media:%s", path);
        return arena_strdup(ctx->arena, ph);
    }

    size_t bin_len = 0;
    uint8_t *bytes = zip_extract_bytes(ctx->zip, ctx->arena, path, &bin_len);
    if (!bytes || bin_len == 0)
        return NULL;
    const char *mime = guess_mime(path);
    char *b64s = base64_alloc(ctx->arena, bytes, bin_len);
    if (!b64s)
        return NULL;
    size_t prefix_len = strlen("data:") + strlen(mime) + strlen(";base64,") + strlen(b64s) + 1;
    char *data_uri = arena_alloc(ctx->arena, prefix_len, sizeof(char));
    if (!data_uri)
        return NULL;
    snprintf(data_uri, prefix_len, "data:%s;base64,%s", mime, b64s);
    return data_uri;
}
