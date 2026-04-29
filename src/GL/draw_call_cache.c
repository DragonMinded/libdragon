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

static cached_draw_call_t *cached_draw_call_create(const draw_call_parms_t *parms)
{
    cached_draw_call_t *draw_call = malloc(sizeof(cached_draw_call_t));
    draw_call->parms = *parms;
    draw_call->index_range = (index_bounds_t){0};
    draw_call->block = NULL;
    draw_call->is_data_dirty = true;
    
    return draw_call;
}

static void free_block(cached_draw_call_t *draw_call)
{
    if (draw_call->block != NULL) {
        rspq_call_deferred((void(*)(void*))rspq_block_free, draw_call->block);
    }
}

static void cached_draw_call_free(cached_draw_call_t *draw_call)
{
    free_block(draw_call);
    free(draw_call);
}

static void free_visitor(uint32_t key, void *value, int refcount)
{
    cached_draw_call_t *draw_call = value;
    cached_draw_call_free(draw_call);
}

void draw_call_cache_free(draw_call_cache_t *cache)
{
    hashtable_visit(&cache->cached_draw_calls, free_visitor);
    hashtable_free(&cache->cached_draw_calls);
    free(cache);
}

static void set_dirty_visitor(uint32_t key, void *value, int refcount)
{
    cached_draw_call_t *draw_call = value;
    draw_call->is_data_dirty = true;
}

void draw_call_cache_set_data_dirty(draw_call_cache_t *cache)
{
    hashtable_visit(&cache->cached_draw_calls, set_dirty_visitor);
}

static uint32_t get_draw_call_key(const draw_call_parms_t *parms)
{
    uint32_t key = fnv1a_init();
    fnv1a_step(&key, parms->offset);
    fnv1a_step(&key, parms->count);
    fnv1a_step(&key, parms->mode);
    return key;
}

cached_draw_call_t *draw_call_cache_get_or_create(draw_call_cache_t *cache, const draw_call_parms_t *parms)
{
    uint32_t key = get_draw_call_key(parms);

    cached_draw_call_t *draw_call = hashtable_lookup(&cache->cached_draw_calls, key);
    if (draw_call == NULL) {
        draw_call = cached_draw_call_create(parms);
        hashtable_insert(&cache->cached_draw_calls, key, draw_call);
    }

    return draw_call;
}

bool draw_call_needs_update(cached_draw_call_t *draw_call)
{
    return draw_call->is_data_dirty || draw_call->block == NULL;
}

static void update_cached_index_range(cached_draw_call_t *draw_call, const void *index_data)
{
    draw_call->index_range = find_index_bounds(index_data, draw_call->parms.count);
}

static uint32_t get_vertex_offset(cached_draw_call_t *draw_call)
{
    return -draw_call->index_range.first;
}

static void draw_call_set_block(cached_draw_call_t *draw_call, rspq_block_t *block)
{
    free_block(draw_call);
    
    draw_call->block = block;
}

static void update_cached_block(cached_draw_call_t *draw_call, const void *index_data, gl_array_object_t *array_object)
{
    mg_input_assembly_parms_t input_assembly_parms = array_object_get_input_assembly_parms(array_object, draw_call->parms.mode, draw_call->index_range);
    uint32_t vertex_offset = get_vertex_offset(draw_call);

    rspq_block_begin();
    mg_draw_indexed(&input_assembly_parms, index_data, draw_call->parms.count, vertex_offset);
    rspq_block_t *block = rspq_block_end();

    draw_call_set_block(draw_call, block);
}

void draw_call_update(cached_draw_call_t *draw_call, const void *index_data, gl_array_object_t *array_object)
{
    update_cached_index_range(draw_call, index_data);

    update_cached_block(draw_call, index_data, array_object);

    draw_call->is_data_dirty = false;
}

void draw_call_run(cached_draw_call_t *draw_call)
{
    assertf(draw_call->block != NULL, "Tried to run a draw call with missing block");

    rspq_block_run(draw_call->block);
}
