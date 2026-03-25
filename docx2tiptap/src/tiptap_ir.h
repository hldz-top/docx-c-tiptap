#ifndef TIPTAP_IR_H
#define TIPTAP_IR_H

#include <stddef.h>

#define TT_MAX_MARK_ATTR 8

typedef struct TtMark {
    const char *type;
    const char *href;
    const char *attr_keys[TT_MAX_MARK_ATTR];
    const char *attr_str[TT_MAX_MARK_ATTR];
    int n_attr;
} TtMark;

#define TT_MAX_ATTR 12
#define TT_MAX_CHILD 512

typedef struct TtNode {
    const char *type;
    const char *attr_keys[TT_MAX_ATTR];
    const char *attr_str[TT_MAX_ATTR];
    int attr_int[TT_MAX_ATTR];
    unsigned char attr_is_int[TT_MAX_ATTR];
    int n_attr;
    const char *leaf_text;
    TtMark leaf_marks[8];
    int n_leaf_marks;
    struct TtNode *children[TT_MAX_CHILD];
    int n_children;
} TtNode;

#endif
