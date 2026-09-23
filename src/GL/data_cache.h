/**
 * @file data_cache.h
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 */
#ifndef __DATA_CACHE_H
#define __DATA_CACHE_H

#include <stdint.h>
#include "indices.h"
#include "data_view.h"

typedef struct data_source_s data_source_t; 

/** 
 * @brief Caches RSP-compatible vertex data so it doesn't need to be re-converted every frame.
 * 
 * The data is pulled from a #data_source_t and saved in uncached memory.
 */
typedef struct data_cache_s {
    data_source_t *source;  ///< The data source this cache pulls its data from.
    void *buffer;           ///< Pointer to the cached data.
    uint32_t stride;        ///< Stride of the cached vertices.
    index_bounds_t bounds;  ///< The index range of the VBO the vertex data was pulled from.
    bool is_data_dirty;     ///< Internal dirty flag that is set to true whenever any VBO data changes or the source arrays change.
} data_cache_t;

#ifdef __cplusplus
extern "C" {
#endif

void data_cache_init(data_cache_t *cache, data_source_t *source);
void data_cache_destroy(data_cache_t *cache);

/** @brief Marks the data in the cache as invalid. */
void data_cache_invalidate(data_cache_t *cache);

/** @brief Requests data at the specified index bounds. If the cached data fully encompasses those bounds and has not been invalidated, it is simply returned and not re-converted. */
data_view_t data_cache_prepare_at_bounds(data_cache_t *cache, index_bounds_t bounds);

#ifdef __cplusplus
}
#endif

#endif
