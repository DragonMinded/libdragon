#ifndef __VERTEX_LAYOUT_CACHE_H
#define __VERTEX_LAYOUT_CACHE_H

#include "vertex_layout.h"
#include "array.h"

typedef struct vertex_layout_cache_s {
    vertex_layout layout;
    bool dirty;
} vertex_layout_cache_t;

#ifdef __cplusplus
extern "C" {
#endif

void vertex_layout_cache_init(vertex_layout_cache_t *cache);
void vertex_layout_cache_set_dirty(vertex_layout_cache_t *cache);
void vertex_layout_cache_update(vertex_layout_cache_t *cache, const array_t *arrays);

inline vertex_layout *vertex_layout_cache_get_layout(vertex_layout_cache_t *cache)
{
    return &cache->layout;
}

#ifdef __cplusplus
}
#endif

#endif
