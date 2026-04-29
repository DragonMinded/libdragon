#include "data_cache.h"
#include "data_source.h"
#include "rspq.h"

void data_cache_init(data_cache_t *cache, data_source_t *source)
{
    *cache = (data_cache_t){0};
    cache->source = source;
    data_cache_set_data_dirty(cache);
}

static void free_buffer(data_cache_t *cache)
{
    if (cache->buffer != NULL) {
        rspq_call_deferred(free_uncached, cache->buffer);
    }
}

void data_cache_destroy(data_cache_t *cache)
{
    free_buffer(cache);
}

void data_cache_set_data_dirty(data_cache_t *cache)
{
    cache->is_data_dirty = true;
}

static bool are_cached_bounds_too_small(data_cache_t *cache, index_bounds_t bounds) {
    return !are_bounds_included(cache->bounds, bounds);
}

static bool is_pull_required_for_bounds(data_cache_t *cache, index_bounds_t bounds)
{
    return cache->is_data_dirty ||
        are_cached_bounds_too_small(cache, bounds) ||
        !data_source_is_fully_internal(cache->source);
}

static void set_bounds(data_cache_t *cache, index_bounds_t bounds)
{
    cache->bounds = bounds;
}

static void update_stride_from_data_source(data_cache_t *cache)
{
    cache->stride = data_source_get_stride(cache->source);
}

static void reallocate_buffer(data_cache_t *cache, uint32_t new_size)
{
    free_buffer(cache);
    // TODO: use allocator abstraction?
    cache->buffer = malloc_uncached(new_size);
}

static void prepare_buffer(data_cache_t *cache)
{
    uint32_t new_size = cache->bounds.count * cache->stride;
    reallocate_buffer(cache, new_size);
}

static void pull_data_into_buffer(data_cache_t *cache, index_bounds_t bounds)
{
    set_bounds(cache, bounds);
    update_stride_from_data_source(cache);
    prepare_buffer(cache);
    data_source_pull(cache->source, cache->buffer, cache->bounds);

    cache->is_data_dirty = false;
}

static data_view_t get_data_view(data_cache_t *cache, index_bounds_t bounds)
{
    uint32_t offset_from_cached = bounds.first - cache->bounds.first;
    uint8_t *buffer = cache->buffer;

    return (data_view_t) {
        .pointer = buffer + offset_from_cached * cache->stride,
        .stride = cache->stride
    };
}

data_view_t data_cache_prepare_at_bounds(data_cache_t *cache, index_bounds_t bounds)
{
    if (is_pull_required_for_bounds(cache, bounds)) {
        pull_data_into_buffer(cache, bounds);
    }

    return get_data_view(cache, bounds);
}
