/**
 * @file vertex_layout.h
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 */
#ifndef __GL_VERTEX_LAYOUT
#define __GL_VERTEX_LAYOUT

#include "magma.h"
#include "../utils.h"

#define MAX_VERTEX_ATTRIBUTE_COUNT  4

typedef struct
{
    mg_vertex_attribute_t attributes[MAX_VERTEX_ATTRIBUTE_COUNT];
    mg_vertex_layout_t vertex_layout;
    uint32_t hash;
} vertex_layout_t;

#ifdef __cplusplus
extern "C" {
#endif

void vertex_layout_init(vertex_layout_t *vl);
void vertex_layout_add(vertex_layout_t *vl, uint32_t input, uint32_t offset, uint32_t size);

inline void vertex_layout_append(vertex_layout_t *vl, uint32_t input, uint32_t size)
{
    vertex_layout_add(vl, input, vl->vertex_layout.stride, size);
}

inline void vertex_layout_set_stride(vertex_layout_t *vl, uint32_t stride)
{
    vl->vertex_layout.stride = stride;
}

void vertex_layout_finalize(vertex_layout_t *vl);

inline uint32_t vertex_layout_get_hash(const vertex_layout_t *vl)
{
    return vl->hash;
}

#ifdef __cplusplus
}
#endif

#endif
