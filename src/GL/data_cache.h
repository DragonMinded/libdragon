#ifndef __DATA_CACHE_H
#define __DATA_CACHE_H

#include <stdint.h>
#include "indices.h"
#include "data_view.h"

typedef struct data_source_s data_source_t; 

typedef struct data_cache_s {
    data_source_t *source;
    void *buffer;
    uint32_t stride;
    index_bounds_t bounds;
    bool is_data_dirty;
} data_cache_t;

#ifdef __cplusplus
extern "C" {
#endif

void data_cache_init(data_cache_t *cache, data_source_t *source);
void data_cache_destroy(data_cache_t *cache);
void data_cache_set_data_dirty(data_cache_t *cache);
data_view_t data_cache_prepare_at_bounds(data_cache_t *cache, index_bounds_t bounds);

#ifdef __cplusplus
}
#endif

#endif
