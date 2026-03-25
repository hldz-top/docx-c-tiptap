#include "docx2tiptap.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    const char *path = "docx2tiptap/tests/fixtures/minimal.docx";
    if (argc > 1)
        path = argv[1];

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "skip: open %s\n", path);
        return 77;
    }
    size_t len = 0;
    unsigned char *data = read_all(f, &len);
    fclose(f);
    if (!data)
        return 1;

    DocxOptions opt = DOCX_OPTIONS_DEFAULT_INITIALIZER;
    char *json = docx_parse(data, len, &opt);
    free(data);

    assert(json);
    assert(strstr(json, "\"type\": \"doc\""));
    assert(strstr(json, "Hello"));
    docx_free(json);

    /* Regression: stripped options should not emit style keys on minimal fixture */
    FILE *f2 = fopen(path, "rb");
    if (f2) {
        size_t len2 = 0;
        unsigned char *data2 = read_all(f2, &len2);
        fclose(f2);
        if (data2) {
            DocxOptions slim = DOCX_OPTIONS_DEFAULT_INITIALIZER;
            slim.include_run_style = 0;
            slim.include_paragraph_format = 0;
            slim.include_header_footer = 0;
            char *j2 = docx_parse(data2, len2, &slim);
            free(data2);
            assert(j2);
            assert(!strstr(j2, "\"textStyle\""));
            assert(!strstr(j2, "\"header\""));
            docx_free(j2);
        }
    }

    return 0;
}
