#include "vertex_layout_cache.h"
#include "gl_constants.h"

void vertex_layout_cache_init(vertex_layout_cache_t *cache)
{
    vertex_layout_cache_set_dirty(cache);
    vertex_layout_init(&cache->layout);
}

void vertex_layout_cache_set_dirty(vertex_layout_cache_t *cache)
{
    cache->dirty = true;
}

void vertex_layout_cache_update(vertex_layout_cache_t *cache, const gl_array_t *arrays)
{
    if (!cache->dirty) {
        return;
    }

    assertf(arrays[ATTRIB_VERTEX].enabled, "The vertex array must be enabled!");

    vertex_layout *layout = &cache->layout;

    vertex_layout_init(layout);
    vertex_layout_append(layout, GLP_ATTRIBUTE_POS_NORM, array_type_get_stride(ATTRIB_VERTEX));

    if (arrays[ATTRIB_COLOR].enabled) {
        vertex_layout_append(layout, GLP_ATTRIBUTE_COLOR, array_type_get_stride(ATTRIB_COLOR));
    }

    if (arrays[ATTRIB_TEXCOORD].enabled) {
        vertex_layout_append(layout, GLP_ATTRIBUTE_TEXCOORD, array_type_get_stride(ATTRIB_TEXCOORD));
    }

    cache->dirty = false;
}