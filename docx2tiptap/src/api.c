#include "docx2tiptap.h"

#include "arena.h"
#include "block_mapper.h"
#include "doc_reader.h"
#include "json_writer.h"
#include "numbering_reader.h"
#include "parse_context.h"
#include "rels_reader.h"
#include "styles_reader.h"
#include "zip_reader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#define d2t_tls __declspec(thread)
#else
#define d2t_tls _Thread_local
#endif

static d2t_tls char g_tls_err[512];

static void set_err(const char *msg) {
    if (!msg)
        g_tls_err[0] = '\0';
    else {
        snprintf(g_tls_err, sizeof g_tls_err, "%s", msg);
        g_tls_err[sizeof g_tls_err - 1] = '\0';
    }
}

char *docx_parse(const uint8_t *buf, size_t len, const DocxOptions *opts) {
    set_err("");
    if (!buf || len == 0) {
        set_err("empty input");
        return NULL;
    }

    static const DocxOptions k_default = DOCX_OPTIONS_DEFAULT_INITIALIZER;
    const DocxOptions *op = opts ? opts : &k_default;

    Arena arena;
    if (arena_init(&arena, 0) != 0) {
        set_err("arena allocation failed");
        return NULL;
    }

    ZipBuf zip;
    memset(&zip, 0, sizeof(zip));
    if (zip_open_memory(&zip, buf, len) != 0) {
        set_err("invalid ZIP / not a DOCX");
        arena_free(&arena);
        return NULL;
    }

    ParseContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.opts = *op;
    ctx.arena = &arena;
    ctx.zip = &zip;

    char *rels_xml = zip_extract_string(&zip, &arena, "word/_rels/document.xml.rels", NULL);
    if (rels_xml) {
        if (rels_parse(&arena, rels_xml, &ctx.rels) != 0)
            parse_ctx_add_warning(&ctx, "failed to parse document.xml.rels");
    }

    char *styles_xml = zip_extract_string(&zip, &arena, "word/styles.xml", NULL);
    if (styles_xml)
        styles_parse(&arena, styles_xml, &ctx.styles);

    char *num_xml = zip_extract_string(&zip, &arena, "word/numbering.xml", NULL);
    if (num_xml)
        numbering_parse(&arena, num_xml, &ctx.nums);

    OoxmlDoc odoc;
    memset(&odoc, 0, sizeof(odoc));

    char *footnotes_xml = zip_extract_string(&zip, &arena, "word/footnotes.xml", NULL);
    if (footnotes_xml)
        doc_read_footnotes(&ctx, footnotes_xml, &odoc);

    char *endnotes_xml = zip_extract_string(&zip, &arena, "word/endnotes.xml", NULL);
    if (endnotes_xml)
        doc_read_endnotes(&ctx, endnotes_xml, &odoc);

    char *document_xml = zip_extract_string(&zip, &arena, "word/document.xml", NULL);
    if (!document_xml) {
        set_err("missing word/document.xml");
        zip_close(&zip);
        arena_free(&arena);
        return NULL;
    }

    if (doc_read_body(&ctx, document_xml, &odoc) != 0) {
        set_err("failed to parse document body");
        zip_close(&zip);
        arena_free(&arena);
        return NULL;
    }

    doc_read_header_footer(&ctx, &zip, &odoc);

    doc_coalesce_lists(&ctx, &odoc);
    doc_merge_figures(&ctx, &odoc);

    odoc.header_tt = tt_build_doc_fragment(&ctx, &odoc, odoc.header_blocks, odoc.header_block_count);
    odoc.footer_tt = tt_build_doc_fragment(&ctx, &odoc, odoc.footer_blocks, odoc.footer_block_count);

    TtNode *root = tt_build_doc(&ctx, &odoc);
    if (!root) {
        set_err("failed to build Tiptap IR");
        zip_close(&zip);
        arena_free(&arena);
        return NULL;
    }

    char *json = json_write_doc(root, &ctx, &odoc);
    zip_close(&zip);
    arena_free(&arena);

    if (!json)
        set_err("failed to serialize JSON");
    return json;
}

void docx_free(char *json) { free(json); }

const char *docx_error_str(void) { return g_tls_err[0] ? g_tls_err : ""; }
