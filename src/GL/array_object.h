/**
 * @file array_object.h
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 */
#ifndef __ARRAY_OBJECT_H
#define __ARRAY_OBJECT_H

#include "array.h"
#include "data_source.h"
#include "data_cache.h"
#include "vertex_layout_cache.h"

typedef struct gl_buffer_object_s gl_buffer_object_t;
typedef struct draw_call_cache_s draw_call_cache_t;

/** 
 * @brief Represents an array object, or "VAO", created using #glGenVertexArrays
 * 
 * The only actual state this object holds is the collection of #arrays and the currently bound #element_array_buffer.
 * A lot of effort is made to cache data to avoid recomputing it on every draw call, which is the purpose of all other members.
 * If the array states stay the same across draw calls and the source vertex data is not changed, this data can simply be reused
 * instead of recomputing it every time. The main part that benefits from this is vertex data, which can be provided through the
 * GL API in a wide range of different source formats, but always needs to be submitted to the RSP in the same target format.
 * 
 * Note that vertex data caching only works if it is provided through VBOs. That's because modifying VBO data from the outside is only
 * possible through GL APIs, which can notify this implementation about changes.
 * When using straight pointers without VBOs on the other hand, the implementation has no way to monitor changes whatsoever because it
 * can point to arbitrary locations in memory managed by the user.
 */
typedef struct gl_array_object_s {
    array_t arrays[ARRAY_COUNT];                ///< The arrays. See #array_type_t for their order.
    gl_buffer_object_t *element_array_buffer;   ///< Pointer to the VBO that is currently bound to #GL_ELEMENT_ARRAY_BUFFER_ARB (NULL if none is bound).

    vertex_layout_cache_t layout_cache;         ///< Cache for the vertex layout computed from #arrays.

    /** 
     * @brief Data cache for regular vertex data.
     * 
     * This cache is invalidated whenever any array state or data inside any bound VBOs changes (except for matrix indices, see below).
     * It contains the RSP-compatible vertex data that is later submitted for rendering.
     */
    data_cache_t vertex_data_cache;

    /**
     * @brief Data cache for matrix indices.
     * 
     * This cache is invalidated whenever the matrix index array state, or the data inside the VBO bound to it changes.
     * It contains the matrix indices that are used for skinning (see also #draw_call_cache).
     * It is only filled when the matrix index array is actually enabled.
     */
    data_cache_t mtx_index_data_cache;

    /**
     * @brief Data source for #vertex_data_cache.
     * 
     * This wraps all arrays except matrix indices and is used by the vertex data cache for conversion to RSP-compatible format.
     */
    data_source_t vertex_data_source;

    /** 
     * @brief Data source for #mtx_index_data_cache.
     * 
     * This wraps the matrix indices array. Even though matrix indices are never directly submitted to the RSP, the magma API expects them
     * in a fixed format. This is used to convert the matrix indices into that format.
     */
    data_source_t mtx_index_data_source;

    /**
     * @brief Cache for draw calls with #GL_MATRIX_PALETTE_ARB enabled.
     * 
     * Since they depend on matrix indices (which are recorded into the rspq block),
     * which are part of the array state, these draw calls are cached here instead of the element array buffer.
     * This is lazily initialized, so may be NULL before being used for the first time.
     * See also \ref gl_buffer_object.element_cache.
     */
    draw_call_cache_t *draw_call_cache;
} gl_array_object_t;

#ifdef __cplusplus
extern "C" {
#endif

void array_object_init(gl_array_object_t *array_object);
void array_object_destroy(gl_array_object_t *array_object);

void array_object_set_array_enabled(gl_array_object_t *array_object, array_type_t array_type, bool enabled);
void array_object_set_array_params(gl_array_object_t *array_object, array_type_t array_type, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);

void array_object_set_buffer_binding(gl_array_object_t *array_object, array_type_t array_type, gl_buffer_object_t *buffer);
void array_object_set_element_buffer_binding(gl_array_object_t *array_object, gl_buffer_object_t *buffer);
void array_object_invalidate_buffer_data(gl_array_object_t *array_object, gl_buffer_object_t *buffer);
void array_object_validate_drawing(gl_array_object_t *array_object, bool indexed);

void array_object_update_pointers(gl_array_object_t *array_object);

draw_call_cache_t *array_object_get_draw_call_cache(gl_array_object_t *array_object);
mg_input_assembly_parms_t array_object_get_input_assembly_parms(gl_array_object_t *array_object, GLenum mode, index_bounds_t bounds);

#ifdef __cplusplus
}
#endif

#endif
