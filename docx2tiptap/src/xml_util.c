#include "xml_util.h"

#include <string.h>

int xml_local_eq(const xmlNode *n, const char *local) {
    if (!n || n->type != XML_ELEMENT_NODE || !n->name || !local)
        return 0;
    const char *nm = (const char *)n->name;
    const char *c = strrchr(nm, ':');
    const char *loc = c ? c + 1 : nm;
    return xmlStrcmp(BAD_CAST loc, BAD_CAST local) == 0;
}

xmlNodePtr xml_child_local(xmlNodePtr parent, const char *local) {
    if (!parent)
        return NULL;
    for (xmlNodePtr c = parent->children; c; c = c->next) {
        if (c->type == XML_ELEMENT_NODE && xml_local_eq(c, local))
            return c;
    }
    return NULL;
}

void xml_collect_text(xmlNodePtr node, char *buf, size_t buf_sz, size_t *out_len) {
    size_t len = 0;
    if (buf && buf_sz)
        buf[0] = '\0';
    if (!node || !buf || buf_sz == 0) {
        if (out_len)
            *out_len = 0;
        return;
    }
    for (xmlNodePtr cur = node; cur; cur = cur->next) {
        if (cur->type == XML_TEXT_NODE || cur->type == XML_CDATA_SECTION_NODE) {
            const char *t = (const char *)cur->content;
            if (!t)
                continue;
            size_t tl = strlen(t);
            if (len + tl >= buf_sz)
                tl = buf_sz - 1 - len;
            if (tl == 0)
                break;
            memcpy(buf + len, t, tl);
            len += tl;
            buf[len] = '\0';
        } else if (cur->type == XML_ELEMENT_NODE) {
            size_t sub = 0;
            xml_collect_text(cur->children, buf + len, buf_sz - len, &sub);
            len += sub;
        }
    }
    if (out_len)
        *out_len = len;
}

const xmlChar *xml_attr_local(xmlNodePtr n, const char *local) {
    if (!n || !local)
        return NULL;
    for (xmlAttrPtr a = n->properties; a; a = a->next) {
        if (!a->name)
            continue;
        const char *nm = (const char *)a->name;
        const char *c = strrchr(nm, ':');
        const char *loc = c ? c + 1 : nm;
        if (strcmp(loc, local) == 0 && a->children && a->children->content)
            return a->children->content;
    }
    return NULL;
}
