#ifndef __BUFFER_H
#define __BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include "GL/gl.h"
#include "draw_call_cache.h"

typedef struct {
    GLvoid *data;
    uint32_t size;
} gl_storage_t;

typedef struct gl_array_object_s gl_array_object_t;
typedef struct gl_array_object_ref_s gl_array_object_ref_t;

typedef struct gl_array_object_ref_s {
    gl_array_object_t *array_object;
    gl_array_object_ref_t *next;
} gl_array_object_ref_t;

typedef struct gl_buffer_object_s {
    GLenum usage;
    GLenum access;
    GLvoid *pointer;
    gl_storage_t storage;
    bool mapped;
    gl_array_object_ref_t *array_obj_ref;
    draw_call_cache_t *element_cache;
    uint32_t ref_count;
} gl_buffer_object_t;

#ifdef __cplusplus
extern "C" {
#endif

bool gl_storage_alloc(gl_storage_t *storage, uint32_t size);
void gl_storage_free(gl_storage_t *storage);
bool gl_storage_resize(gl_storage_t *storage, uint32_t new_size);

void gl_buffer_add_array_ref(gl_buffer_object_t *buffer, gl_array_object_t *array);
void gl_buffer_remove_array_ref(gl_buffer_object_t *buffer, gl_array_object_t *array);
void buffer_object_set_binding(gl_buffer_object_t *obj, gl_buffer_object_t **binding);
void buffer_object_validate_not_mapped(gl_buffer_object_t *obj);

#ifdef __cplusplus
}
#endif

#endif
