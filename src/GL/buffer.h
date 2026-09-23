/**
 * @file buffer.h
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 */
#ifndef __BUFFER_H
#define __BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include "GL/gl.h"

typedef struct {
    GLvoid *data;
    uint32_t size;
} gl_storage_t;

typedef struct gl_array_object_s gl_array_object_t;
typedef struct gl_array_object_ref_s gl_array_object_ref_t;
typedef struct draw_call_cache_s draw_call_cache_t;

typedef struct gl_array_object_ref_s {
    gl_array_object_t *array_object;
    gl_array_object_ref_t *next;
    uint32_t refcount;
} gl_array_object_ref_t;

/**
 * @brief Represents a buffer object, or "VBO", created using #glGenBuffersARB
 */
typedef struct gl_buffer_object_s {
    GLenum usage;
    GLenum access;
    GLvoid *pointer;
    gl_storage_t storage;
    bool mapped;
    gl_array_object_ref_t *array_obj_ref;

    /**
     * @brief Caches draw calls to be quickly reused without re-computing them.
     * 
     * When submitting geometry to be rendered, magma internally tries to optimize vertex cache usage.
     * This can be costly, since indices may be in any arbitrary order. This is why GL tries to cache
     * draw calls by recording them into rspq blocks, so this work doesn't need to be repeated.
     * Since the RSP commands only depend on the indices and not on vertex data (except matrix indices, see below),
     * the cached draw calls can be reused if for example the same indices are used every frame.
     * This only works if the indices are stored in a VBO (Just like with vertex data, see #gl_array_object_t),
     * which is why this cache is placed in this struct.
     * 
     * This field is initially NULL. The cache is only created and initialized when this VBO is used as the source for 
     * index data for the first time. For this to happen, it needs to be bound to #GL_ELEMENT_ARRAY_BUFFER_ARB while
     * #glDrawElements is called (see the exception below).
     * 
     * If #GL_MATRIX_PALETTE_ARB is enabled (skinning), the RSP commands are additionally dependent on matrix indices.
     * In that case, the draw call cache inside \ref gl_array_object_t.draw_call_cache is used instead.
     * 
     * This cache is invalidated whenever the data inside this VBO is modified.
     */
    draw_call_cache_t *element_cache;

    uint32_t ref_count;
} gl_buffer_object_t;

#ifdef __cplusplus
extern "C" {
#endif

bool gl_storage_alloc(gl_storage_t *storage, uint32_t size);
void gl_storage_free(gl_storage_t *storage);
bool gl_storage_resize(gl_storage_t *storage, uint32_t new_size);

void gl_buffer_add_array_ref(gl_buffer_object_t *buffer, gl_array_object_t *array);
void gl_buffer_remove_array_ref(gl_buffer_object_t *buffer, gl_array_object_t *array);
void buffer_object_validate_not_mapped(gl_buffer_object_t *obj);

#ifdef __cplusplus
}
#endif

#endif
