#include "rels_reader.h"

#include "xml_util.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <string.h>

int rels_parse(Arena *a, const char *xml, RelMap *out) {
    if (!a || !xml || !out)
        return -1;
    memset(out, 0, sizeof(*out));
    xmlDocPtr doc = xmlReadMemory(xml, (int)strlen(xml), "rels.xml", NULL, 0);
    if (!doc)
        return -1;
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) {
        xmlFreeDoc(doc);
        return -1;
    }

    size_t cap = 32;
    RelEntry *items = arena_alloc(a, cap * sizeof(RelEntry), sizeof(void *));
    if (!items) {
        xmlFreeDoc(doc);
        return -1;
    }
    size_t n = 0;

    for (xmlNodePtr c = root->children; c; c = c->next) {
        if (c->type != XML_ELEMENT_NODE || !xml_local_eq(c, "Relationship"))
            continue;
        const xmlChar *id = xml_attr_local(c, "Id");
        const xmlChar *type = xml_attr_local(c, "Type");
        const xmlChar *target = xml_attr_local(c, "Target");
        if (!id)
            continue;
        if (n >= cap) {
            size_t ncap = cap * 2;
            RelEntry *ni = arena_alloc(a, ncap * sizeof(RelEntry), sizeof(void *));
            if (!ni) {
                xmlFreeDoc(doc);
                return -1;
            }
            memcpy(ni, items, n * sizeof(RelEntry));
            items = ni;
            cap = ncap;
        }
        items[n].id = arena_strdup(a, (const char *)id);
        items[n].type = type ? arena_strdup(a, (const char *)type) : NULL;
        items[n].target = target ? arena_strdup(a, (const char *)target) : NULL;
        n++;
    }

    out->items = items;
    out->count = n;
    xmlFreeDoc(doc);
    return 0;
}
