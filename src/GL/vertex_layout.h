/**
 * @file vertex_layout.h
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 */
#ifndef __GL_VERTEX_LAYOUT
#define __GL_VERTEX_LAYOUT

#include "magma.h"
#include "../utils.h"

#define MAX_VERTEX_ATTRIBUTE_COUNT  3

typedef struct
{
    mg_vertex_attribute_t attributes[MAX_VERTEX_ATTRIBUTE_COUNT];
    mg_vertex_layout_t vertex_layout;
} vertex_layout;


#ifdef __cplusplus
extern "C" {
#endif

inline void vertex_layout_init(vertex_layout *vl)
{
    vl->vertex_layout.attributes = vl->attributes;
    vl->vertex_layout.attribute_count = 0;
    vl->vertex_layout.stride = 0;
}

inline void vertex_layout_add(vertex_layout *vl, uint32_t input, uint32_t offset, uint32_t size)
{
    mg_vertex_attribute_t *attr = &vl->attributes[vl->vertex_layout.attribute_count++];
    attr->input = input;
    attr->offset = offset;
    vl->vertex_layout.stride = offset + size;
}

inline void vertex_layout_append(vertex_layout *vl, uint32_t input, uint32_t size)
{
    vertex_layout_add(vl, input, vl->vertex_layout.stride, size);
}

inline void vertex_layout_copy_without(vertex_layout *vl, const vertex_layout *src, uint32_t input)
{
    vertex_layout_init(vl);
    for (size_t i = 0; i < src->vertex_layout.attribute_count; i++)
    {
        const mg_vertex_attribute_t *attr = &src->attributes[i];
        if (attr->input != input) {
            vertex_layout_add(vl, attr->input, attr->offset, 0);
        }
    }
    vl->vertex_layout.stride = src->vertex_layout.stride;
}

#ifdef __cplusplus
}
#endif

#endif
