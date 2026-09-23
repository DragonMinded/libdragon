/**
 * @file vertex_layout.c
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 */
#include "vertex_layout.h"
#include "fnv1a.h"

void vertex_layout_init(vertex_layout_t *vl)
{
    vl->vertex_layout.attributes = vl->attributes;
    vl->vertex_layout.attribute_count = 0;
    vl->vertex_layout.stride = 0;
}

void vertex_layout_add(vertex_layout_t *vl, uint32_t input, uint32_t offset, uint32_t size)
{
    assertf(vl->vertex_layout.attribute_count < MAX_VERTEX_ATTRIBUTE_COUNT, "Vertex layout already contains the maximum number of attributes (%d)!", MAX_VERTEX_ATTRIBUTE_COUNT);
    mg_vertex_attribute_t *attr = &vl->attributes[vl->vertex_layout.attribute_count++];
    attr->input = input;
    attr->offset = offset;
    vl->vertex_layout.stride = offset + size;
}

static uint32_t get_hash(const mg_vertex_layout_t *layout)
{
    uint32_t key = fnv1a_init();
    for (size_t i = 0; i < layout->attribute_count; i++)
    {
        fnv1a_step(&key, layout->attributes[i].input);
        fnv1a_step(&key, layout->attributes[i].offset);
    }
    fnv1a_step(&key, layout->stride);
    return key;
}

void vertex_layout_finalize(vertex_layout_t *vl)
{
    vl->hash = get_hash(&vl->vertex_layout);
}

extern inline void vertex_layout_append(vertex_layout_t *vl, uint32_t input, uint32_t size);
extern inline void vertex_layout_set_stride(vertex_layout_t *vl, uint32_t stride);
extern inline uint32_t vertex_layout_get_hash(const vertex_layout_t *vl);
