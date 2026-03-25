#include "styles_reader.h"

#include "xml_util.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <ctype.h>
#include <string.h>

typedef struct StyleRow {
    char *id;
    char *name_l;
    char *based_on;
    StyleSemantic sem;
} StyleRow;

static void lower_inplace(char *s) {
    if (!s)
        return;
    for (; *s; ++s)
        *s = (char)tolower((unsigned char)*s);
}

static StyleSemantic semantic_from_name(const char *nl) {
    if (!nl || !*nl)
        return STYLE_UNKNOWN;
    if (strcmp(nl, "heading 1") == 0 || strcmp(nl, "标题 1") == 0 || strcmp(nl, "标题1") == 0)
        return STYLE_HEADING_1;
    if (strcmp(nl, "heading 2") == 0 || strcmp(nl, "标题 2") == 0 || strcmp(nl, "标题2") == 0)
        return STYLE_HEADING_2;
    if (strcmp(nl, "heading 3") == 0 || strcmp(nl, "标题 3") == 0 || strcmp(nl, "标题3") == 0)
        return STYLE_HEADING_3;
    if (strcmp(nl, "heading 4") == 0 || strcmp(nl, "标题 4") == 0 || strcmp(nl, "标题4") == 0)
        return STYLE_HEADING_4;
    if (strcmp(nl, "abstract") == 0 || strcmp(nl, "英文摘要") == 0)
        return STYLE_ABSTRACT_EN;
    if (strcmp(nl, "摘要") == 0 || strcmp(nl, "中文摘要") == 0)
        return STYLE_ABSTRACT_ZH;
    if (strcmp(nl, "keywords") == 0 || strcmp(nl, "英文关键词") == 0 || strcmp(nl, "keyword") == 0)
        return STYLE_KEYWORDS_EN;
    if (strcmp(nl, "关键词") == 0 || strcmp(nl, "中文关键词") == 0)
        return STYLE_KEYWORDS_ZH;
    if (strstr(nl, "caption") && strstr(nl, "figure"))
        return STYLE_CAPTION_FIGURE;
    if (strstr(nl, "caption") && strstr(nl, "table"))
        return STYLE_CAPTION_TABLE;
    if (strstr(nl, "图题") || strcmp(nl, "figure caption") == 0)
        return STYLE_CAPTION_FIGURE;
    if (strstr(nl, "表题") || strcmp(nl, "table caption") == 0)
        return STYLE_CAPTION_TABLE;
    if (strcmp(nl, "bibliography") == 0 || strcmp(nl, "参考文献") == 0)
        return STYLE_BODY_TEXT;
    if (strcmp(nl, "normal") == 0 || strcmp(nl, "正文") == 0 || strcmp(nl, "body text") == 0)
        return STYLE_BODY_TEXT;
    return STYLE_UNKNOWN;
}

static const xmlChar *first_val_child(xmlNodePtr parent, const char *local) {
    xmlNodePtr ch = xml_child_local(parent, local);
    if (!ch)
        return NULL;
    return xml_attr_local(ch, "val");
}

static StyleSemantic resolve_row(StyleRow *rows, size_t n, StyleRow *row, int depth) {
    if (depth > 4)
        return STYLE_UNKNOWN;
    if (row->sem != STYLE_UNKNOWN)
        return row->sem;
    StyleSemantic from_name = semantic_from_name(row->name_l);
    if (from_name != STYLE_UNKNOWN) {
        row->sem = from_name;
        return from_name;
    }
    if (!row->based_on || !row->based_on[0]) {
        row->sem = STYLE_BODY_TEXT;
        return STYLE_BODY_TEXT;
    }
    for (size_t i = 0; i < n; i++) {
        if (rows[i].id && strcmp(rows[i].id, row->based_on) == 0) {
            StyleSemantic s = resolve_row(rows, n, &rows[i], depth + 1);
            row->sem = s;
            return s;
        }
    }
    row->sem = STYLE_BODY_TEXT;
    return STYLE_BODY_TEXT;
}

int styles_parse(Arena *a, const char *xml, StyleMap *out) {
    if (!a || !xml || !out)
        return -1;
    memset(out, 0, sizeof(*out));
    xmlDocPtr doc = xmlReadMemory(xml, (int)strlen(xml), "styles.xml", NULL, 0);
    if (!doc)
        return -1;
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) {
        xmlFreeDoc(doc);
        return -1;
    }

    size_t cap = 64;
    StyleRow *rows = arena_alloc(a, cap * sizeof(StyleRow), sizeof(void *));
    if (!rows) {
        xmlFreeDoc(doc);
        return -1;
    }
    size_t n = 0;

    for (xmlNodePtr st = root->children; st; st = st->next) {
        if (st->type != XML_ELEMENT_NODE || !xml_local_eq(st, "style"))
            continue;
        const xmlChar *sid = xml_attr_local(st, "styleId");
        if (!sid)
            continue;
        if (n >= cap) {
            cap *= 2;
            StyleRow *nr = arena_alloc(a, cap * sizeof(StyleRow), sizeof(void *));
            if (!nr) {
                xmlFreeDoc(doc);
                return -1;
            }
            memcpy(nr, rows, n * sizeof(StyleRow));
            rows = nr;
        }
        rows[n].id = arena_strdup(a, (const char *)sid);
        const xmlChar *nm = first_val_child(st, "name");
        char *name_buf = NULL;
        if (nm) {
            name_buf = arena_strdup(a, (const char *)nm);
            lower_inplace(name_buf);
        }
        rows[n].name_l = name_buf;
        const xmlChar *bo = first_val_child(st, "basedOn");
        rows[n].based_on = bo ? arena_strdup(a, (const char *)bo) : NULL;
        rows[n].sem = STYLE_UNKNOWN;
        n++;
    }

    for (size_t i = 0; i < n; i++)
        resolve_row(rows, n, &rows[i], 0);

    StyleEntry *items = arena_alloc(a, n * sizeof(StyleEntry), sizeof(void *));
    if (!items) {
        xmlFreeDoc(doc);
        return -1;
    }
    for (size_t i = 0; i < n; i++) {
        items[i].style_id = rows[i].id;
        items[i].semantic = rows[i].sem;
        items[i].display_name = rows[i].name_l;
    }

    out->items = items;
    out->count = n;
    xmlFreeDoc(doc);
    return 0;
}
