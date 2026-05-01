#ifndef __ARRAY_OBJECT_H
#define __ARRAY_OBJECT_H

#include "array.h"
#include "data_source.h"
#include "data_cache.h"
#include "vertex_layout_cache.h"

typedef struct gl_buffer_object_s gl_buffer_object_t;
typedef struct draw_call_cache_s draw_call_cache_t;

typedef struct gl_array_object_s {
    array_t arrays[ARRAY_COUNT];
    gl_buffer_object_t *element_array_buffer;
    vertex_layout_cache_t layout_cache;
    data_source_t vertex_data_source, mtx_index_data_source;
    data_cache_t vertex_data_cache, mtx_index_data_cache;
    draw_call_cache_t *draw_call_cache;
} gl_array_object_t;

#ifdef __cplusplus
extern "C" {
#endif

void array_object_init(gl_array_object_t *obj);
void array_object_destroy(gl_array_object_t *obj);

void array_object_set_array_enabled(gl_array_object_t *obj, array_type_t array_type, bool enabled);
void array_object_set_array_params(gl_array_object_t *obj, array_type_t array_type, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);

void array_object_set_buffer_binding(gl_array_object_t *obj, array_type_t array_type, gl_buffer_object_t *buffer);
void array_object_set_buffer_dirty(gl_array_object_t *obj, gl_buffer_object_t *buffer);
void array_object_validate_drawing(gl_array_object_t *array_object, bool indexed);

void array_object_update_pointers(gl_array_object_t *obj);

draw_call_cache_t *array_object_get_draw_call_cache(gl_array_object_t *array_object);
mg_input_assembly_parms_t array_object_get_input_assembly_parms(gl_array_object_t *array_object, GLenum mode, index_bounds_t bounds);

#ifdef __cplusplus
}
#endif

#endif
