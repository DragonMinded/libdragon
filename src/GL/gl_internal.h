/**
 * @file gl_internal.h
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 */
#ifndef __GL_INTERNAL
#define __GL_INTERNAL

#include "GL/gl.h"
#include "GL/gl_integration.h"
#include "gl_constants.h"
#include "vertex_layout.h"
#include "ringbuffer.h"
#include "magma.h"
#include "mgfx.h"
#include "rdpq.h"
#include "rdpq_tex.h"
#include "../utils.h"
#include "../hashtable_internal.h"

#define VERTEX_UNIT_COUNT     1
#define ATTRIB_TYPE_COUNT     10

#define MODELVIEW_STACK_SIZE  32
#define PROJECTION_STACK_SIZE 2
#define TEXTURE_STACK_SIZE    2
#define PALETTE_STACK_SIZE    1

#define LIGHT_COUNT     8

#define MAX_TEXTURE_SIZE      64
#define MAX_TEXTURE_LEVELS    7

#define MAX_PIXEL_MAP_SIZE    32

#define TEX_COORD_COUNT         4
#define TEX_GEN_COUNT           TEX_COORD_COUNT

#define RDP_TEX_SHIFT       5
#define TEX_SIZE_SHIFT      (MGFX_VTX_TEX_SHIFT-RDP_TEX_SHIFT)
#define RDP_HALF_TEXEL      (1<<(RDP_TEX_SHIFT-1))

#define TEXTURE_BILINEAR_MASK       0x001
#define TEXTURE_INTERPOLATE_MASK    0x002
#define TEXTURE_MIPMAP_MASK         0x100

#define MAX_PIPELINE_COUNT          (1<<4)

#define BEGIN_END_BUFFER_COUNT      2

// divisible by 2, 3, 4, which are all possible required multiples of vertices (see get_begin_end_multiple)
#define BEGIN_END_BUFFER_SIZE       36

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

//#define gl_assert_no_display_list() assertf(state->current_list == 0, "%s cannot be recorded into a display list", __func__)
#define gl_assert_no_display_list()

typedef int16_t int16u_t __attribute__((aligned(1)));
typedef uint16_t uint16u_t __attribute__((aligned(1)));
typedef int32_t int32u_t __attribute__((aligned(1)));
typedef uint32_t uint32u_t __attribute__((aligned(1)));
typedef float floatu __attribute__((aligned(1)));
typedef double doubleu __attribute__((aligned(1)));

typedef enum {
    ENABLE_SCISSOR_TEST     = 1 << 0,
    ENABLE_ALPHA_TEST       = 1 << 1,
    ENABLE_DEPTH_TEST       = 1 << 2,
    ENABLE_BLEND            = 1 << 3,
    ENABLE_DITHER           = 1 << 4,
    ENABLE_MULTISAMPLE      = 1 << 5,
    ENABLE_FOG              = 1 << 6,
    ENABLE_LIGHTING         = 1 << 7,
    ENABLE_COLOR_MATERIAL   = 1 << 8,
    ENABLE_NORMALIZE        = 1 << 9,
    ENABLE_TEXTURE_1D       = 1 << 10,
    ENABLE_TEXTURE_2D       = 1 << 11,
    ENABLE_CULL_FACE        = 1 << 12,
    ENABLE_MATRIX_PALETTE   = 1 << 13,
    ENABLE_RDPQ_TEXTURING   = 1 << 14,
    ENABLE_RDPQ_MATERIAL    = 1 << 15,
} gl_enable_flags_t;

typedef enum {
    DIRTY_PIPELINE      = 1 << 0,
    DIRTY_GEOM_FLAGS    = 1 << 1,
    DIRTY_LIGHTING      = 1 << 2,
    DIRTY_FOG           = 1 << 3,
    DIRTY_TEXTURING     = 1 << 4,
    DIRTY_RENDERMODE    = 1 << 5,
    DIRTY_COMBINER      = 1 << 6,
} gl_dirty_flags_t;

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

typedef struct gl_array_object_s gl_array_object_t;
typedef struct gl_array_object_ref_s gl_array_object_ref_t;

typedef struct gl_array_object_ref_s {
    gl_array_object_t *array_object;
    gl_array_object_ref_t *next;
} gl_array_object_ref_t;

typedef struct {
    GLenum usage;
    GLenum access;
    GLvoid *pointer;
    gl_storage_t storage;
    bool mapped;
    gl_array_object_ref_t *array_obj_ref;
    gl_element_array_cache_t *element_cache;
    uint32_t ref_count;
} gl_buffer_object_t;

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
    read_attrib_func read_func;
} gl_array_t;

typedef struct gl_array_object_s {
    gl_array_t arrays[ATTRIB_COUNT];
    uint32_t out_offsets[ATTRIB_COUNT];
    gl_buffer_object_t *element_array_buffer;
    vertex_layout layout;
    void *buffer;
    uint32_t cached_first;
    uint32_t cached_count;
    bool is_layout_dirty;
    bool is_data_dirty;
    bool is_all_vbos;
    bool are_bindings_dirty;
} gl_array_object_t;

typedef struct {
    GLuint target_precision;
    GLuint precision;
    GLint shift_amount;
    GLfloat to_float_factor;
} gl_fixed_precision_t;

typedef struct {
    GLenum mode;
    GLfloat eye_plane[TEX_COORD_COUNT];
    GLfloat object_plane[TEX_COORD_COUNT];
    bool enabled;
} gl_tex_gen_t;

typedef enum {
    TEX_IS_DEFAULT          = (1 << 0),
    TEX_IS_COMPLETE         = (1 << 1),
    TEX_HAS_IMAGE           = (1 << 2),
    TEX_IS_BLOCK_DIRTY      = (1 << 3),
} gl_texture_flag_t;

typedef struct {
    surface_t surface;
    rdpq_texparms_t parms;
} gl_texture_image_t;

typedef struct {
    uint32_t flags;
    GLenum dimensionality;
    GLenum wrap_s;
    GLenum wrap_t;
    GLenum min_filter;
    GLenum mag_filter;
    
    uint32_t levels_count;
    gl_texture_image_t levels[MAX_TEXTURE_LEVELS]; // TODO: allocate lazily
    sprite_t *sprite;
    rspq_block_t *upload_block;
} gl_texture_object_t;

typedef struct {
    GLsizei size;
    GLfloat entries[MAX_PIXEL_MAP_SIZE];
} gl_pixel_map_t;

typedef struct {
    GLfloat ambient[4];
    GLfloat diffuse[4];
    GLfloat specular[4];
    GLfloat emissive[4];
    GLfloat shininess;
    GLenum color_target;
} gl_material_t;

typedef struct {
    GLfloat ambient[4];
    GLfloat diffuse[4];
    GLfloat specular[4];
    fm_vec4_t position;
    fm_vec3_t direction;
    GLfloat spot_exponent;
    GLfloat spot_cutoff_cos;
    GLfloat constant_attenuation;
    GLfloat linear_attenuation;
    GLfloat quadratic_attenuation;
    bool enabled;
} gl_light_t;

typedef struct {
    int16_t position[3];
    uint16_t normal;
    uint32_t color;
    int16_t texcoord[2];
    uint8_t mtx_index[VERTEX_UNIT_COUNT];
} native_vertex_t;

typedef struct {

} gl_list_t;

typedef struct {
    GLenum cull_face_mode;
    GLenum front_face;
    GLenum current_error;
    GLenum blend_src;
    GLenum blend_dst;
    GLenum depth_func;
    GLenum alpha_func;
    GLenum tex_env_mode;
    GLclampf alpha_ref;
    rdpq_blender_t blender;
    rdpq_combiner_t combiner;
    rdpq_dither_t dither_mode;
    GLboolean depth_mask;
    GLfloat fog_start;
    GLfloat fog_end;
    color_t fog_color;
    color_t clear_color;
    uint16_t clear_depth;
    gl_array_object_t default_array_object;
    gl_array_object_t *array_object;
    gl_buffer_object_t *array_buffer;
    const surface_t *color_buffer;
    gl_viewport_t viewport;

    // TODO: move this to array object?
    gl_fixed_precision_t vertex_halfx_precision;
    gl_fixed_precision_t texcoord_halfx_precision;
    
    hashtable_t pipeline_cache;
    uint32_t current_pipeline_key;
    const mg_uniform_t *fog_uniform;
    const mg_uniform_t *lighting_uniform;
    const mg_uniform_t *texturing_uniform;
    const mg_uniform_t *matrices_uniform;

    ringbuffer fog_buffer, lighting_buffer, texturing_buffer;
    
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

    gl_material_t material;
    gl_light_t lights[LIGHT_COUNT];

    GLfloat light_model_ambient[4];
    bool light_model_local_viewer;

    GLenum shade_model;

    gl_texture_object_t *texture_1d_object;
    gl_texture_object_t *texture_2d_object;
    gl_texture_object_t *default_textures;

    GLboolean unpack_swap_bytes;
    GLboolean unpack_lsb_first;
    GLint unpack_row_length;
    GLint unpack_skip_rows;
    GLint unpack_skip_pixels;
    GLint unpack_alignment;

    GLboolean map_color;
    GLfloat transfer_scale[4];
    GLfloat transfer_bias[4];

    gl_pixel_map_t pixel_maps[4];

    bool transfer_is_noop;

    gl_tex_gen_t tex_gen[TEX_GEN_COUNT];

    GLushort rdpq_tex_width, rdpq_tex_height;

    native_vertex_t current_attribs;
    native_vertex_t begin_end_saved_vtx;
    ringbuffer begin_end_buffer;
    native_vertex_t *begin_end_current_buffer;
    GLenum begin_end_mode;
    mg_primitive_topology_t begin_end_topology;
    uint32_t begin_end_index;
    uint32_t begin_end_multiple;
    bool begin_end_need_save;
    bool begin_end_restore;
    vertex_layout begin_end_layout;

    GLenum polygon_mode;
    GLfloat point_size;
    GLfloat line_width;

    hashtable_t lists;
    GLuint next_list_name;
    GLuint list_base;
    GLuint current_list_name;
    gl_list_t *current_list;
    
    // TODO: Generic system that tracks state changes and applies changes automatically
    gl_dirty_flags_t dirty_flags;

    bool begin_end_active;
    bool is_drawing_anything;
    bool cull_face;
    bool texture_1d;
    bool texture_2d;
    bool depth_test;
    bool blend;
    bool alpha_test;
    bool dither;
    bool scissor_test;
    bool lighting;
    bool fog;
    bool color_material;
    bool normalize;
    bool matrix_palette_enabled;
    bool tex_flip_t;
    bool multisample;
    bool reduced_aa;
    bool persp_correct;
    bool rdpq_texture;
} gl_state_t;

extern gl_state_t *state;

#ifdef __cplusplus
extern "C" {
#endif

inline bool is_in_heap_memory(void *ptr)
{
    ptr = CachedAddr(ptr);
    return ptr >= HEAP_START_ADDR && ptr < ((void*)KSEG0_START_ADDR + __boot_memsize);
}

inline bool is_valid_object_id(GLuint id)
{
    return is_in_heap_memory((void*)id);
}

void gl_rendermode_init();
void gl_rendermode_close();
void gl_array_init();
void gl_array_close();
void gl_primitive_init();
void gl_primitive_close();
void gl_matrix_init();
void gl_lighting_init();
void gl_lighting_close();
void gl_texture_init();
void gl_texture_close();
void gl_list_init();
void gl_list_close();

bool gl_storage_alloc(gl_storage_t *storage, uint32_t size);
void gl_storage_free(gl_storage_t *storage);
bool gl_storage_resize(gl_storage_t *storage, uint32_t new_size);

read_attrib_func get_read_func(gl_array_type_t array_type, GLenum type);
void array_object_update(gl_array_object_t *array_object, uint32_t first, uint32_t count);

fm_mat4_t *gl_matrix_stack_get_matrix(gl_matrix_stack_t *stack);
void update_pipeline();
void update_culling();
void update_viewport();
void update_rendermode();

void gl_buffer_add_array_ref(gl_buffer_object_t *buffer, gl_array_object_t *array);
void gl_buffer_remove_array_ref(gl_buffer_object_t *buffer, gl_array_object_t *array);
void buffer_object_set_binding(gl_buffer_object_t *obj, gl_buffer_object_t **binding);

void gl_update_array_pointers(gl_array_object_t *obj);
void array_object_set_buffer_binding(gl_array_object_t *obj, gl_array_type_t array_type, gl_buffer_object_t *buffer);
void array_convert(gl_array_object_t *obj, const uint32_t out_offsets[ATTRIB_COUNT], void *dst_buffer, uint32_t first, uint32_t count, uint32_t stride);

void gl_set_light_enabled(GLenum light, bool enabled);
void gl_set_texture_enabled(GLenum target, bool enabled);
void gl_set_tex_gen_enabled(GLenum target, bool enabled);

gl_texture_object_t * gl_get_active_texture();
inline bool texture_is_complete(gl_texture_object_t *obj)
{
    return (obj->flags & TEX_IS_COMPLETE) != 0;
}

bool gl_is_diffuse_tracking_color();
color_t gl_get_material_diffuse();

bool gl_is_shade_active();
bool gl_is_texture_active();
bool gl_is_depth_active();

bool gl_is_env_map_enabled();

void gl_upload_matrices(const mg_uniform_t *uniform);
void gl_upload_lighting(const mg_uniform_t *uniform);
void gl_upload_fog(const mg_uniform_t *uniform);
void gl_upload_texture(const mg_uniform_t *uniform);

inline void gl_set_dirty_flags(gl_dirty_flags_t flags)
{
    state->dirty_flags |= flags;
}

inline void gl_clear_dirty_flags(gl_dirty_flags_t flags)
{
    state->dirty_flags &= ~flags;
}

inline bool gl_check_dirty_flags(gl_dirty_flags_t flags)
{
    return (state->dirty_flags & flags) == flags;
}

inline bool gl_check_and_clear_dirty_flags(gl_dirty_flags_t flags)
{
    if (gl_check_dirty_flags(flags)) {
        gl_clear_dirty_flags(flags);
        return true;
    }
    return false;
}

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
    case GL_SHORT_5_6_5_N64:
        return 9;
    default:
        return -1;
    }
}

inline color_t color_from_floats(const float color[4])
{
    return RGBA32(
        FLOAT_TO_U8(color[0]),
        FLOAT_TO_U8(color[1]),
        FLOAT_TO_U8(color[2]),
        FLOAT_TO_U8(color[3])
    );
}

#ifdef __cplusplus
}
#endif

#endif
