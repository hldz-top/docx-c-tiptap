#include "numbering_reader.h"

#include "xml_util.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <stdlib.h>
#include <string.h>

static NumFmtKind fmt_from_val(const char *v) {
    if (!v)
        return NUM_FMT_OTHER;
    if (strcmp(v, "bullet") == 0)
        return NUM_FMT_BULLET;
    if (strcmp(v, "decimal") == 0)
        return NUM_FMT_DECIMAL;
    if (strcmp(v, "lowerLetter") == 0)
        return NUM_FMT_LOWER_LETTER;
    if (strcmp(v, "upperLetter") == 0)
        return NUM_FMT_UPPER_LETTER;
    if (strcmp(v, "lowerRoman") == 0)
        return NUM_FMT_LOWER_ROMAN;
    if (strcmp(v, "upperRoman") == 0)
        return NUM_FMT_UPPER_ROMAN;
    return NUM_FMT_OTHER;
}

static int parse_int_attr(xmlNodePtr n, const char *local) {
    const xmlChar *v = xml_attr_local(n, local);
    if (!v)
        return -1;
    return atoi((const char *)v);
}

int numbering_parse(Arena *a, const char *xml, NumMap *out) {
    if (!a || !xml || !out)
        return -1;
    memset(out, 0, sizeof(*out));
    xmlDocPtr doc = xmlReadMemory(xml, (int)strlen(xml), "numbering.xml", NULL, 0);
    if (!doc)
        return -1;
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) {
        xmlFreeDoc(doc);
        return -1;
    }

    size_t abs_cap = 16;
    AbstractNumDef *abs_arr = arena_alloc(a, abs_cap * sizeof(AbstractNumDef), sizeof(void *));
    if (!abs_arr) {
        xmlFreeDoc(doc);
        return -1;
    }
    size_t abs_n = 0;

    size_t inst_cap = 32;
    NumInstance *inst = arena_alloc(a, inst_cap * sizeof(NumInstance), sizeof(void *));
    if (!inst) {
        xmlFreeDoc(doc);
        return -1;
    }
    size_t inst_n = 0;

    for (xmlNodePtr ch = root->children; ch; ch = ch->next) {
        if (ch->type != XML_ELEMENT_NODE)
            continue;
        if (xml_local_eq(ch, "abstractNum")) {
            int aid = parse_int_attr(ch, "abstractNumId");
            if (aid < 0)
                continue;
            int max_lvl = -1;
            for (xmlNodePtr x = ch->children; x; x = x->next) {
                if (x->type == XML_ELEMENT_NODE && xml_local_eq(x, "lvl")) {
                    int il = parse_int_attr(x, "ilvl");
                    if (il > max_lvl)
                        max_lvl = il;
                }
            }
            int nlv = (max_lvl >= 0) ? (max_lvl + 1) : 0;
            AbstractLevelFmt *levels = NULL;
            if (nlv > 0) {
                levels = arena_alloc(a, (size_t)nlv * sizeof(AbstractLevelFmt), sizeof(void *));
                if (!levels) {
                    xmlFreeDoc(doc);
                    return -1;
                }
                for (int i = 0; i < nlv; i++)
                    levels[i].fmt = NUM_FMT_DECIMAL;
            }
            for (xmlNodePtr x = ch->children; x; x = x->next) {
                if (x->type != XML_ELEMENT_NODE || !xml_local_eq(x, "lvl"))
                    continue;
                int il = parse_int_attr(x, "ilvl");
                if (il < 0 || il >= nlv || !levels)
                    continue;
                xmlNodePtr nf = xml_child_local(x, "numFmt");
                const xmlChar *val = nf ? xml_attr_local(nf, "val") : NULL;
                levels[il].fmt = fmt_from_val(val ? (const char *)val : "decimal");
            }
            if (abs_n >= abs_cap) {
                abs_cap *= 2;
                AbstractNumDef *na = arena_alloc(a, abs_cap * sizeof(AbstractNumDef), sizeof(void *));
                if (!na) {
                    xmlFreeDoc(doc);
                    return -1;
                }
                memcpy(na, abs_arr, abs_n * sizeof(AbstractNumDef));
                abs_arr = na;
            }
            abs_arr[abs_n].abstract_id = aid;
            abs_arr[abs_n].levels = levels;
            abs_arr[abs_n].level_count = nlv;
            abs_n++;
        } else if (xml_local_eq(ch, "num")) {
            int nid = parse_int_attr(ch, "numId");
            if (nid < 0)
                continue;
            xmlNodePtr an = xml_child_local(ch, "abstractNumId");
            const xmlChar *aval = an ? xml_attr_local(an, "val") : NULL;
            if (!aval)
                continue;
            int abid = atoi((const char *)aval);
            if (inst_n >= inst_cap) {
                inst_cap *= 2;
                NumInstance *ni = arena_alloc(a, inst_cap * sizeof(NumInstance), sizeof(void *));
                if (!ni) {
                    xmlFreeDoc(doc);
                    return -1;
                }
                memcpy(ni, inst, inst_n * sizeof(NumInstance));
                inst = ni;
            }
            inst[inst_n].num_id = nid;
            inst[inst_n].abstract_id = abid;
            inst_n++;
        }
    }

    out->abstracts = abs_arr;
    out->abstract_count = abs_n;
    out->instances = inst;
    out->instance_count = inst_n;
    xmlFreeDoc(doc);
    return 0;
}
