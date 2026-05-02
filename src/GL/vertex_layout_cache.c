#include "vertex_layout_cache.h"
#include "gl_constants.h"

void vertex_layout_cache_init(vertex_layout_cache_t *cache)
{
    vertex_layout_cache_invalidate(cache);
}

void vertex_layout_cache_invalidate(vertex_layout_cache_t *cache)
{
    cache->dirty = true;
}

void vertex_layout_cache_update(vertex_layout_cache_t *cache, const array_t *arrays)
{
    if (!cache->dirty) {
        return;
    }

    assertf(arrays[ARRAY_VERTEX].enabled, "The vertex array must be enabled!");

    vertex_layout_t *layout = &cache->layout;

    vertex_layout_init(layout);
    vertex_layout_append(layout, GLP_ATTRIBUTE_POS_NORM, array_type_get_stride(ARRAY_VERTEX));

    if (arrays[ARRAY_COLOR].enabled) {
        vertex_layout_append(layout, GLP_ATTRIBUTE_COLOR, array_type_get_stride(ARRAY_COLOR));
    }

    if (arrays[ARRAY_TEXCOORD].enabled) {
        vertex_layout_append(layout, GLP_ATTRIBUTE_TEXCOORD, array_type_get_stride(ARRAY_TEXCOORD));
    }

    vertex_layout_finalize(layout);

    cache->dirty = false;
}