#include "draw_call_cache.h"
#include "gl_internal.h"
#include "array_object.h"
#include "fnv1a.h"

#define MAX_CACHED_DRAW_CALL_COUNT  8

extern gl_state_t *state;

draw_call_cache_t *draw_call_cache_create()
{
    draw_call_cache_t *cache = malloc(sizeof(draw_call_cache_t));
    hashtable_init(&cache->cached_draw_calls, 4, NULL);
    return cache;
}

static void free_block(cached_draw_call_t *draw_call)
{
    if (draw_call->block != NULL) {
        rspq_call_deferred((void(*)(void*))rspq_block_free, draw_call->block);
        draw_call->block = NULL;
    }
}

static void free_draw_call(cached_draw_call_t *draw_call)
{
    free_block(draw_call);
    free(draw_call);
}

static void free_visitor(uint32_t key, void *value, int refcount)
{
    cached_draw_call_t *draw_call = value;
    free_draw_call(draw_call);
}

static void free_all(draw_call_cache_t *cache)
{
    hashtable_visit(&cache->cached_draw_calls, free_visitor);
}

void draw_call_cache_free(draw_call_cache_t *cache)
{
    free_all(cache);
    hashtable_free(&cache->cached_draw_calls);
    free(cache);
}

void draw_call_cache_invalidate_indices(draw_call_cache_t *cache)
{
    // All cached data is now obsolete -> free everything
    free_all(cache);
    hashtable_clear(&cache->cached_draw_calls);
}

static void free_block_visitor(uint32_t key, void *value, int refcount)
{
    cached_draw_call_t *draw_call = value;
    free_block(draw_call);
}

void draw_call_cache_invalidate_mtx_indices(draw_call_cache_t *cache)
{
    // Index ranges are still valid, but blocks are obsolete
    hashtable_visit(&cache->cached_draw_calls, free_block_visitor);
}

static uint32_t get_draw_call_key(const draw_call_parms_t *parms)
{
    uint32_t key = fnv1a_init();
    fnv1a_step(&key, parms->offset);
    fnv1a_step(&key, parms->count);
    fnv1a_step(&key, parms->mode);
    return key;
}

static cached_draw_call_t *create_draw_call(draw_call_cache_t *cache, const draw_call_parms_t *parms, const void *index_data)
{
    cached_draw_call_t *draw_call = malloc(sizeof(cached_draw_call_t));
    draw_call->index_range = find_index_bounds(index_data, parms->count);
    draw_call->block = NULL;
    return draw_call;
}

static rspq_block_t *create_block(const draw_call_parms_t *parms, const void *index_data, gl_array_object_t *array_object, index_bounds_t index_range)
{
    mg_input_assembly_parms_t input_assembly_parms = array_object_get_input_assembly_parms(array_object, parms->mode, index_range);
    rspq_block_begin();
    mg_draw_indexed(&input_assembly_parms, index_data, parms->count, -index_range.first);
    return rspq_block_end();
}

static void update_draw_call(cached_draw_call_t *draw_call, const draw_call_parms_t *parms, const void *index_data, gl_array_object_t *array_object)
{
    if (draw_call->block == NULL) {
        draw_call->block = create_block(parms, index_data, array_object, draw_call->index_range);
    }
}

cached_draw_call_t *draw_call_cache_get_or_create(draw_call_cache_t *cache, const draw_call_parms_t *parms, const void *index_data, gl_array_object_t *array_object)
{
    uint32_t key = get_draw_call_key(parms);

    cached_draw_call_t *draw_call = hashtable_lookup(&cache->cached_draw_calls, key);
    if (draw_call == NULL) {
        draw_call = create_draw_call(cache, parms, index_data);
        hashtable_insert(&cache->cached_draw_calls, key, draw_call);
    }

    update_draw_call(draw_call, parms, index_data, array_object);

    return draw_call;
}

void draw_call_run(cached_draw_call_t *draw_call)
{
    assertf(draw_call->block != NULL, "Tried to run a draw call with missing block");

    rspq_block_run(draw_call->block);
}
