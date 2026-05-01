#ifndef __VERTEX_LAYOUT_CACHE_H
#define __VERTEX_LAYOUT_CACHE_H

#include "vertex_layout.h"
#include "array.h"

typedef struct vertex_layout_cache_s {
    vertex_layout_t layout;
    bool dirty;
} vertex_layout_cache_t;

#ifdef __cplusplus
extern "C" {
#endif

void vertex_layout_cache_init(vertex_layout_cache_t *cache);
void vertex_layout_cache_set_dirty(vertex_layout_cache_t *cache);
void vertex_layout_cache_update(vertex_layout_cache_t *cache, const array_t *arrays);

inline const vertex_layout_t *vertex_layout_cache_get_layout(const vertex_layout_cache_t *cache)
{
    assertf(!cache->dirty, "Vertex layout cache is dirty!");
    return &cache->layout;
}

#ifdef __cplusplus
}
#endif

#endif
