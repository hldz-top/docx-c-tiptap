/**
 * DOCX → Tiptap JSON (single public header).
 */
#ifndef DOCX2TIPTAP_H
#define DOCX2TIPTAP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** How to embed images in JSON. */
typedef enum DocxImageStrategy {
    DOCX_IMAGE_BASE64_INLINE = 0,
    DOCX_IMAGE_SKIP = 1,
    DOCX_IMAGE_PLACEHOLDER_URL = 2,
} DocxImageStrategy;

/** Unknown paragraph style handling. */
typedef enum DocxUnknownStyleMode {
    DOCX_UNKNOWN_STYLE_PARAGRAPH = 0,
    DOCX_UNKNOWN_STYLE_WARNING = 1,
} DocxUnknownStyleMode;

typedef struct DocxOptions {
    DocxImageStrategy image_strategy;
    DocxUnknownStyleMode unknown_style_mode;
    int include_page_setup;      /* bool */
    int include_warnings;        /* bool */
    int include_run_style;       /* bool: font, size, color, highlight, sub/sup */
    int include_paragraph_format; /* bool: align, indent, line spacing, margins */
    int include_header_footer;   /* bool */
} DocxOptions;

/** Default options: full feature set on. */
#define DOCX_OPTIONS_DEFAULT_INITIALIZER                                                         \
    {                                                                                            \
        DOCX_IMAGE_BASE64_INLINE, DOCX_UNKNOWN_STYLE_PARAGRAPH, 1, 1, 1, 1, 1                    \
    }

/**
 * Parse a DOCX file from memory. Returns a NUL-terminated JSON string allocated with malloc,
 * or NULL on failure. On success the caller must release with docx_free().
 */
char *docx_parse(const uint8_t *buf, size_t len, const DocxOptions *opts);

/** Free a string returned by docx_parse(). */
void docx_free(char *json);

/** Human-readable message for the last failure on this thread (empty string if none). */
const char *docx_error_str(void);

#ifdef __cplusplus
}
#endif

#endif /* DOCX2TIPTAP_H */
