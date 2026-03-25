#ifndef XML_UTIL_H
#define XML_UTIL_H

#include <libxml/tree.h>

/** Compare element local name (handles any namespace prefix). */
int xml_local_eq(const xmlNode *n, const char *local);

/** First element child with given local name. */
xmlNodePtr xml_child_local(xmlNodePtr parent, const char *local);

/** Deep text concatenation for w:t etc. (skips non-text). */
void xml_collect_text(xmlNodePtr node, char *buf, size_t buf_sz, size_t *out_len);

/** Attribute value by local name (any prefix). */
const xmlChar *xml_attr_local(xmlNodePtr n, const char *local);

#endif
