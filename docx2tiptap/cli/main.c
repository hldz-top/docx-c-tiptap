#include "docx2tiptap.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#endif

static unsigned char *read_all(FILE *f, size_t *out_len) {
    unsigned char *buf = NULL;
    size_t cap = 0;
    size_t len = 0;
    for (;;) {
        if (len + 4096 > cap) {
            cap = cap ? cap * 2 : 65536;
            unsigned char *nb = (unsigned char *)realloc(buf, cap);
            if (!nb) {
                free(buf);
                return NULL;
            }
            buf = nb;
        }
        size_t n = fread(buf + len, 1, 4096, f);
        len += n;
        if (n < 4096)
            break;
    }
    *out_len = len;
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <input.docx>\n", argc > 0 ? argv[0] : "docx2tiptap");
        return 1;
    }

#if defined(_WIN32)
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror(argv[1]);
        return 1;
    }
    size_t len = 0;
    unsigned char *data = read_all(f, &len);
    fclose(f);
    if (!data) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    DocxOptions opt = DOCX_OPTIONS_DEFAULT_INITIALIZER;
    char *json = docx_parse(data, len, &opt);
    free(data);

    if (!json) {
        fprintf(stderr, "%s\n", docx_error_str());
        return 1;
    }

    fputs(json, stdout);
    docx_free(json);
    return 0;
}
