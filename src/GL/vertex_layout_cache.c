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

static void layout_append_aligned(vertex_layout_t *layout, uint32_t input, array_type_t type)
{
    uint32_t aligned_stride = ROUND_UP(layout->vertex_layout.stride, array_type_get_alignment(type));
    vertex_layout_set_stride(layout, aligned_stride);
    vertex_layout_append(layout, input, array_type_get_stride(type));
}

void vertex_layout_cache_update(vertex_layout_cache_t *cache, const array_t *arrays)
{
    if (!cache->dirty) {
        return;
    }

    assertf(arrays[ARRAY_VERTEX].enabled, "The vertex array must be enabled!");

    vertex_layout_t *layout = &cache->layout;

    vertex_layout_init(layout);
    layout_append_aligned(layout, GLP_ATTRIBUTE_POSITION, ARRAY_VERTEX);

    if (arrays[ARRAY_NORMAL].enabled) {
        layout_append_aligned(layout, GLP_ATTRIBUTE_NORMAL, ARRAY_NORMAL);
    }

    if (arrays[ARRAY_COLOR].enabled) {
        layout_append_aligned(layout, GLP_ATTRIBUTE_COLOR, ARRAY_COLOR);
    }

    if (arrays[ARRAY_TEXCOORD].enabled) {
        layout_append_aligned(layout, GLP_ATTRIBUTE_TEXCOORD, ARRAY_TEXCOORD);
    }

    vertex_layout_finalize(layout);

    cache->dirty = false;
}