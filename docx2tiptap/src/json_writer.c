#include "json_writer.h"

#include <yyjson.h>

#include <stdlib.h>
#include <string.h>

static yyjson_mut_val *emit_text(yyjson_mut_doc *jd, const TtNode *n) {
    yyjson_mut_val *o = yyjson_mut_obj(jd);
    yyjson_mut_obj_add_str(jd, o, "type", "text");
    yyjson_mut_obj_add_str(jd, o, "text", n->leaf_text ? n->leaf_text : "");
    if (n->n_leaf_marks > 0) {
        yyjson_mut_val *marks = yyjson_mut_arr(jd);
        for (int i = 0; i < n->n_leaf_marks; i++) {
            const TtMark *m = &n->leaf_marks[i];
            yyjson_mut_val *mo = yyjson_mut_obj(jd);
            yyjson_mut_obj_add_str(jd, mo, "type", m->type ? m->type : "bold");
            if (m->n_attr > 0) {
                yyjson_mut_val *at = yyjson_mut_obj(jd);
                for (int j = 0; j < m->n_attr; j++) {
                    if (m->attr_keys[j] && m->attr_str[j])
                        yyjson_mut_obj_add_str(jd, at, m->attr_keys[j], m->attr_str[j]);
                }
                yyjson_mut_obj_add_val(jd, mo, "attrs", at);
            } else if (m->href && m->type && strcmp(m->type, "link") == 0) {
                yyjson_mut_val *at = yyjson_mut_obj(jd);
                yyjson_mut_obj_add_str(jd, at, "href", m->href);
                yyjson_mut_obj_add_val(jd, mo, "attrs", at);
            }
            yyjson_mut_arr_append(marks, mo);
        }
        yyjson_mut_obj_add_val(jd, o, "marks", marks);
    }
    return o;
}

static yyjson_mut_val *emit_node(yyjson_mut_doc *jd, const TtNode *n);

static void emit_attrs(yyjson_mut_doc *jd, yyjson_mut_val *o, const TtNode *n) {
    if (n->n_attr <= 0)
        return;
    yyjson_mut_val *at = yyjson_mut_obj(jd);
    for (int i = 0; i < n->n_attr; i++) {
        const char *k = n->attr_keys[i];
        if (!k)
            continue;
        if (n->attr_is_int[i])
            yyjson_mut_obj_add_int(jd, at, k, n->attr_int[i]);
        else if (n->attr_str[i])
            yyjson_mut_obj_add_str(jd, at, k, n->attr_str[i]);
    }
    yyjson_mut_obj_add_val(jd, o, "attrs", at);
}

static yyjson_mut_val *emit_node(yyjson_mut_doc *jd, const TtNode *n) {
    if (!n || !n->type)
        return NULL;
    if (strcmp(n->type, "text") == 0)
        return emit_text(jd, n);

    yyjson_mut_val *o = yyjson_mut_obj(jd);
    yyjson_mut_obj_add_str(jd, o, "type", n->type);
    emit_attrs(jd, o, n);
    int is_text_leaf = (strcmp(n->type, "text") == 0);
    int is_image_leaf = (strcmp(n->type, "image") == 0);
    if (!is_text_leaf && !is_image_leaf) {
        yyjson_mut_val *arr = yyjson_mut_arr(jd);
        for (int i = 0; i < n->n_children; i++) {
            yyjson_mut_val *ch = emit_node(jd, n->children[i]);
            if (ch)
                yyjson_mut_arr_append(arr, ch);
        }
        yyjson_mut_obj_add_val(jd, o, "content", arr);
    }
    return o;
}

static void add_hf_content(yyjson_mut_doc *jd, yyjson_mut_val *attrs, const char *key, const TtNode *frag) {
    if (!frag || frag->n_children <= 0)
        return;
    yyjson_mut_val *arr = yyjson_mut_arr(jd);
    for (int i = 0; i < frag->n_children; i++) {
        yyjson_mut_val *ch = emit_node(jd, frag->children[i]);
        if (ch)
            yyjson_mut_arr_append(arr, ch);
    }
    yyjson_mut_obj_add_val(jd, attrs, key, arr);
}

static void add_page_setup(yyjson_mut_doc *jd, yyjson_mut_val *attrs, const OoxmlDoc *odoc) {
    if (!odoc || !odoc->page.has_values)
        return;
    yyjson_mut_val *ps = yyjson_mut_obj(jd);
    yyjson_mut_obj_add_int(jd, ps, "marginTop", odoc->page.margin_top);
    yyjson_mut_obj_add_int(jd, ps, "marginBottom", odoc->page.margin_bottom);
    yyjson_mut_obj_add_int(jd, ps, "marginLeft", odoc->page.margin_left);
    yyjson_mut_obj_add_int(jd, ps, "marginRight", odoc->page.margin_right);
    yyjson_mut_obj_add_int(jd, ps, "pageWidth", odoc->page.page_width);
    yyjson_mut_obj_add_int(jd, ps, "pageHeight", odoc->page.page_height);
    yyjson_mut_obj_add_bool(jd, ps, "landscape", odoc->page.landscape != 0);
    yyjson_mut_obj_add_val(jd, attrs, "pageSetup", ps);
}

char *json_write_doc(const TtNode *root, const ParseContext *ctx, const OoxmlDoc *odoc) {
    if (!root)
        return NULL;
    yyjson_mut_doc *jd = yyjson_mut_doc_new(NULL);
    if (!jd)
        return NULL;

    yyjson_mut_val *doc = yyjson_mut_obj(jd);
    yyjson_mut_obj_add_str(jd, doc, "type", "doc");

    yyjson_mut_val *attrs = yyjson_mut_obj(jd);
    if (ctx && ctx->opts.include_page_setup && odoc)
        add_page_setup(jd, attrs, odoc);

    if (ctx && ctx->opts.include_header_footer && odoc) {
        add_hf_content(jd, attrs, "header", odoc->header_tt);
        add_hf_content(jd, attrs, "footer", odoc->footer_tt);
    }

    if (ctx && ctx->opts.include_warnings && ctx->warning_count > 0) {
        yyjson_mut_val *warr = yyjson_mut_arr(jd);
        for (int i = 0; i < ctx->warning_count; i++) {
            if (ctx->warning_msgs[i])
                yyjson_mut_arr_add_str(jd, warr, ctx->warning_msgs[i]);
        }
        yyjson_mut_obj_add_val(jd, attrs, "warnings", warr);
    } else {
        yyjson_mut_val *warr = yyjson_mut_arr(jd);
        yyjson_mut_obj_add_val(jd, attrs, "warnings", warr);
    }

    yyjson_mut_obj_add_val(jd, doc, "attrs", attrs);

    yyjson_mut_val *content = yyjson_mut_arr(jd);
    for (int i = 0; i < root->n_children; i++) {
        yyjson_mut_val *ch = emit_node(jd, root->children[i]);
        if (ch)
            yyjson_mut_arr_append(content, ch);
    }
    yyjson_mut_obj_add_val(jd, doc, "content", content);

    yyjson_mut_doc_set_root(jd, doc);

    char *json = yyjson_mut_write(jd, YYJSON_WRITE_PRETTY, NULL);
    yyjson_mut_doc_free(jd);
    return json;
}
