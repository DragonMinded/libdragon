#ifndef __DRAW_CALL_CACHE_H
#define __DRAW_CALL_CACHE_H

#include <stdint.h>
#include "rspq.h"
#include "GL/gl.h"
#include "hashtable_internal.h"
#include "indices.h"

#define MAX_CACHED_DRAW_CALLS   8

typedef struct draw_call_parms_s {
    uint32_t offset;
    uint32_t count;
    GLenum mode;
} draw_call_parms_t;

typedef struct cached_draw_call_s {
    draw_call_parms_t parms;
    index_bounds_t index_range;
    rspq_block_t *block;
    bool is_data_dirty;
} cached_draw_call_t;

typedef struct draw_call_cache_s {
    hashtable_t cached_draw_calls;
} draw_call_cache_t;

typedef struct gl_array_object_s gl_array_object_t;

#ifdef __cplusplus
extern "C" {
#endif

draw_call_cache_t *draw_call_cache_create();
void draw_call_cache_free(draw_call_cache_t *cache);

void draw_call_cache_set_data_dirty(draw_call_cache_t *cache);

cached_draw_call_t *draw_call_cache_get_or_create(draw_call_cache_t *cache, const draw_call_parms_t *parms);

bool draw_call_needs_update(cached_draw_call_t *draw_call);
void draw_call_update(cached_draw_call_t *draw_call, const void *index_data, gl_array_object_t *array_object);

void draw_call_run(cached_draw_call_t *draw_call);

#ifdef __cplusplus
}
#endif

#endif
