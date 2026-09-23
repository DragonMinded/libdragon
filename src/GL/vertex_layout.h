/**
 * @file vertex_layout.h
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 */
#ifndef __GL_VERTEX_LAYOUT
#define __GL_VERTEX_LAYOUT

#include "magma.h"
#include "../utils.h"

#define MAX_VERTEX_ATTRIBUTE_COUNT  4

/** @brief Helper-struct for constructing #mg_vertex_layout_t. */
typedef struct vertex_layout_s
{
    mg_vertex_attribute_t attributes[MAX_VERTEX_ATTRIBUTE_COUNT];
    mg_vertex_layout_t vertex_layout;
    uint32_t hash;
} vertex_layout_t;

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Resets the layout back to an "empty" state. */
void vertex_layout_init(vertex_layout_t *vl);

/** 
 * @brief Adds an attribute to the layout.
 * 
 * The added attribute is now assumed to be the "last" one, so the stride is set to just behind this attribute.
 */
void vertex_layout_add(vertex_layout_t *vl, uint32_t input, uint32_t offset, uint32_t size);

/**
 * @brief Like #vertex_layout_add, but tightly packs the new attribute at the end of the current layout.
 */
inline void vertex_layout_append(vertex_layout_t *vl, uint32_t input, uint32_t size)
{
    vertex_layout_add(vl, input, vl->vertex_layout.stride, size);
}

/**
 * @brief Directly sets the stride of the layout.
 * 
 * This can be useful to override the assumptions made by #vertex_layout_add or #vertex_layout_append
 */
inline void vertex_layout_set_stride(vertex_layout_t *vl, uint32_t stride)
{
    vl->vertex_layout.stride = stride;
}

/** @brief Marks this layout as complete. */
void vertex_layout_finalize(vertex_layout_t *vl);

inline uint32_t vertex_layout_get_hash(const vertex_layout_t *vl)
{
    return vl->hash;
}

#ifdef __cplusplus
}
#endif

#endif
