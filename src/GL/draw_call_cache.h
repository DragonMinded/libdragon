/**
 * @file draw_call_cache.h
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 */
#ifndef __DRAW_CALL_CACHE_H
#define __DRAW_CALL_CACHE_H

#include <stdint.h>
#include "rspq.h"
#include "GL/gl.h"
#include "hashtable_internal.h"
#include "indices.h"

/** @brief Represents the parameters of glDrawElements to describe a cachable draw call */
typedef struct draw_call_parms_s {
    uint32_t offset;    ///< The byte offset into the VBO bound to #GL_ELEMENT_ARRAY_BUFFER_ARB
    uint32_t count;     ///< The index count
    GLenum mode;        ///< The primitive mode
} draw_call_parms_t;

/** @brief Cached data of a draw call (glDrawElements) */
typedef struct cached_draw_call_s {
    /** 
     * @brief The range of indices that occur in this draw call.
     * 
     * This information is also being cached because the indices need to be linearly scanned to determine it.
     * The range is used to pre-warm the vertex data for the draw call.
     */
    index_bounds_t index_range;
    rspq_block_t *block; ///< The draw call recorded into a block.
} cached_draw_call_t;

/** 
 * @brief Caches draw calls so they can be quickly re-issued across frames.
 * 
 * Draw calls are keyed by the hash of their parameters (see #draw_call_parms_t).
 * The cache can hold multiple entries to allow for multiple draw calls from the same index buffer.
 * The entire cache is automatically invalidated when the index buffer data changes.
 * Note that matrix indices are also recorded into draw calls and therefore changes to them also invalidate the cache.
 */
typedef struct draw_call_cache_s {
    hashtable_t cached_draw_calls;
} draw_call_cache_t;

typedef struct gl_array_object_s gl_array_object_t;

#ifdef __cplusplus
extern "C" {
#endif

draw_call_cache_t *draw_call_cache_create();
void draw_call_cache_free(draw_call_cache_t *cache);

void draw_call_cache_invalidate_indices(draw_call_cache_t *cache);
void draw_call_cache_invalidate_mtx_indices(draw_call_cache_t *cache);

cached_draw_call_t *draw_call_cache_get_or_create(draw_call_cache_t *cache, const draw_call_parms_t *parms, const void *index_data, gl_array_object_t *array_object);

void draw_call_run(cached_draw_call_t *draw_call);

#ifdef __cplusplus
}
#endif

#endif
