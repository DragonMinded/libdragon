/**
 * @file gl_internal.h
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 */
#ifndef __GL_INTERNAL
#define __GL_INTERNAL

#include "GL/gl.h"
#include "GL/gl_integration.h"
#include "gl_constants.h"
#include "magma.h"
#include "mgfx.h"
#include "rdpq.h"
#include "utils.h"

#define VERTEX_UNIT_COUNT     1
#define ATTRIB_TYPE_COUNT     9

#define MODELVIEW_STACK_SIZE  32
#define PROJECTION_STACK_SIZE 2
#define TEXTURE_STACK_SIZE    2
#define PALETTE_STACK_SIZE    1

#define MAX_PIPELINE_COUNT          (1<<4)
#define MAX_VERTEX_ATTRIBUTE_COUNT  3

#define CLAMP01(x) CLAMP((x), 0, 1)

#define CLAMPF_TO_BOOL(x)  ((x)!=0.0)

#define CLAMPF_TO_U8(x)  ((x)*0xFF)
#define CLAMPF_TO_I8(x)  ((x)*0x7F)
#define CLAMPF_TO_U16(x) ((x)*0xFFFF)
#define CLAMPF_TO_I16(x) ((x)*0x7FFF)
#define CLAMPF_TO_U32(x) ((x)*0xFFFFFFFF)
#define CLAMPF_TO_I32(x) ((x)*0x7FFFFFFF)

#define FLOAT_TO_U8(x)  (CLAMP((x), 0.f, 1.f)*0xFF)
#define FLOAT_TO_I8(x)  (CLAMP((x), -1.f, 1.f)*0x7F)
#define FLOAT_TO_I16(x)  (CLAMP((x), -1.f, 1.f)*0x7FFF)

#define U8_TO_FLOAT(x) ((x)/(float)(0xFF))
#define U16_TO_FLOAT(x) ((x)/(float)(0xFFFF))
#define U32_TO_FLOAT(x) ((x)/(float)(0xFFFFFFFF))
#define I8_TO_FLOAT(x) MAX((x)/(float)(0x7F),-1.f)
#define I16_TO_FLOAT(x) MAX((x)/(float)(0x7FFF),-1.f)
#define I32_TO_FLOAT(x) MAX((x)/(float)(0x7FFFFFFF),-1.f)

#define gl_set_error(error, message, ...)  ({ \
    state->current_error = error; \
    assertf(error == GL_NO_ERROR, "%s: " message, #error, ##__VA_ARGS__); \
})

#define gl_ensure_no_begin_end() ({ \
    assertf(state, "gl_init() not called"); \
    if (state->begin_end_active) { \
        gl_set_error(GL_INVALID_OPERATION, "%s is not allowed between glBegin/glEnd", __func__); \
    } \
    true; \
})

typedef int16_t int16u_t __attribute__((aligned(1)));
typedef uint16_t uint16u_t __attribute__((aligned(1)));
typedef int32_t int32u_t __attribute__((aligned(1)));
typedef uint32_t uint32u_t __attribute__((aligned(1)));
typedef float floatu __attribute__((aligned(1)));
typedef double doubleu __attribute__((aligned(1)));

typedef struct {
    GLfloat x, y, w, h, n, f;
} gl_viewport_t;

typedef struct {
    fm_mat4_t *storage;
    int32_t size;
    int32_t cur_depth;
} gl_matrix_stack_t;

typedef struct {
    gl_matrix_stack_t *mv_stack;
    fm_mat4_t mvp;
    bool is_mvp_dirty;
} gl_matrix_target_t;

typedef enum {
    ATTRIB_VERTEX,
    ATTRIB_NORMAL,
    ATTRIB_COLOR,
    ATTRIB_TEXCOORD,
    ATTRIB_MTX_INDEX,
    ATTRIB_COUNT
} gl_array_type_t;

typedef struct {
    GLvoid *data;
    uint32_t size;
} gl_storage_t;

typedef struct {
    uint32_t offset;
    uint32_t count;
    mg_input_assembly_parms_t parms;
    rspq_block_t *block;
    uint16_t min_index;
    uint16_t max_index;
    bool is_data_dirty;
} gl_element_array_cache_t;

typedef struct {
    GLenum usage;
    GLenum access;
    GLvoid *pointer;
    gl_storage_t storage;
    bool mapped;
    gl_element_array_cache_t *element_cache;
} gl_buffer_object_t;

typedef struct
{
    mg_vertex_attribute_t attributes[MAX_VERTEX_ATTRIBUTE_COUNT];
    mg_vertex_layout_t vertex_layout;
} vertex_layout;

typedef void (*read_attrib_func)(void*,const void*,uint32_t);

typedef struct {
    GLint size;
    GLenum type;
    GLsizei stride;
    const GLvoid *pointer;
    gl_buffer_object_t *binding;
    bool enabled;

    const GLvoid *final_pointer;
    uint16_t final_stride;
    uint16_t out_offset;
    read_attrib_func read_func;
} gl_array_t;

typedef struct {
    gl_array_t arrays[ATTRIB_COUNT];
    gl_buffer_object_t *element_array_buffer;
    vertex_layout layout;
    uint32_t pipeline_index;
    void *buffer;
    uint32_t cached_first;
    uint32_t cached_count;
    bool is_layout_dirty;
    bool is_data_dirty;
} gl_array_object_t;

typedef struct {
    mgfx_fog_t fog;
    mgfx_lighting_t lighting;
    mgfx_texturing_t texturing;
} gl_uniform_data;

typedef struct {
    GLuint target_precision;
    GLuint precision;
    GLint shift_amount;
    GLfloat to_float_factor;
} gl_fixed_precision_t;

typedef struct
{
    mg_pipeline_t *pipeline;
    vertex_layout layout;
    mgfx_features_t features;
} pipeline_data;

typedef struct {
    GLenum cull_face_mode;
    GLenum front_face;
    GLenum current_error;
    GLfloat fog_start;
    GLfloat fog_end;
    color_t clear_color;
    uint16_t clear_depth;
    gl_array_object_t default_array_object;
    gl_array_object_t *array_object;
    gl_buffer_object_t *array_buffer;
    const surface_t *color_buffer;
    gl_viewport_t viewport;
    gl_uniform_data *uniform_data;
    gl_fixed_precision_t vertex_halfx_precision;
    gl_fixed_precision_t texcoord_halfx_precision;
    uint32_t pipelines_count;
    pipeline_data pipelines[MAX_PIPELINE_COUNT]; // TODO: change this to a hashmap

    GLenum matrix_mode;
    GLint current_palette_matrix;

    fm_mat4_t *current_matrix;

    fm_mat4_t modelview_stack_storage[MODELVIEW_STACK_SIZE];
    fm_mat4_t projection_stack_storage[PROJECTION_STACK_SIZE];
    fm_mat4_t texture_stack_storage[TEXTURE_STACK_SIZE];
    fm_mat4_t palette_stack_storage[MATRIX_PALETTE_SIZE][PALETTE_STACK_SIZE];

    gl_matrix_stack_t modelview_stack;
    gl_matrix_stack_t projection_stack;
    gl_matrix_stack_t texture_stack;
    gl_matrix_stack_t palette_stacks[MATRIX_PALETTE_SIZE];
    gl_matrix_stack_t *current_matrix_stack;

    gl_matrix_target_t default_matrix_target;
    gl_matrix_target_t palette_matrix_targets[MATRIX_PALETTE_SIZE];

    gl_matrix_target_t *current_matrix_target;

    float near_plane;
    float far_plane;

    bool begin_end_active;
    bool is_pipeline_dirty;
    bool is_drawing_anything;
    bool cull_face;
    bool texture_1d;
    bool texture_2d;
    bool depth_test;
    bool lighting;
    bool fog;
    bool color_material;
    bool normalize;
    bool matrix_palette_enabled;
    bool tex_flip_t;
} gl_state_t;

inline bool is_in_heap_memory(void *ptr)
{
    ptr = CachedAddr(ptr);
    return ptr >= HEAP_START_ADDR && ptr < ((void*)KSEG0_START_ADDR + __boot_memsize);
}

inline bool is_valid_object_id(GLuint id)
{
    return is_in_heap_memory((void*)id);
}

bool gl_storage_alloc(gl_storage_t *storage, uint32_t size);
void gl_storage_free(gl_storage_t *storage);
bool gl_storage_resize(gl_storage_t *storage, uint32_t new_size);

uint32_t pipeline_get_or_create(const mg_vertex_layout_t *submesh_layout, mgfx_features_t features);
void array_object_update(gl_array_object_t *array_object, uint32_t first, uint32_t count);

fm_mat4_t *gl_matrix_stack_get_matrix(gl_matrix_stack_t *stack);
void gl_update_matrix_targets();
void update_culling();
void update_viewport();

inline uint32_t gl_type_to_index(GLenum type)
{
    switch (type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_FLOAT:
        return type - GL_BYTE;
    case GL_DOUBLE:
        return 7;
    case GL_HALF_FIXED_N64:
        return 8;
    default:
        return -1;
    }
}

#endif
