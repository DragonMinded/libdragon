#ifndef __GL_INTERNAL
#define __GL_INTERNAL

#include "GL/gl.h"
#include "obj_map.h"
#include "surface.h"
#include "utils.h"
#include <stdbool.h>
#include <math.h>

#define MODELVIEW_STACK_SIZE  32
#define PROJECTION_STACK_SIZE 2
#define TEXTURE_STACK_SIZE    2

#define VERTEX_CACHE_SIZE     16

#define CLIPPING_PLANE_COUNT  6
#define CLIPPING_CACHE_SIZE   9

#define LIGHT_COUNT           8

#define MAX_TEXTURE_SIZE      64
#define MAX_TEXTURE_LEVELS    7

#define MAX_PIXEL_MAP_SIZE    32

#define RADIANS(x) ((x) * M_PI / 180.0f)

#define CLAMP(x, min, max) (MIN(MAX((x), (min)), (max)))
#define CLAMP01(x) CLAMP((x), 0, 1)

#define CLAMPF_TO_BOOL(x)  ((x)!=0.0)

#define CLAMPF_TO_U8(x)  ((x)*0xFF)
#define CLAMPF_TO_I8(x)  ((x)*0x7F)
#define CLAMPF_TO_U16(x) ((x)*0xFFFF)
#define CLAMPF_TO_I16(x) ((x)*0x7FFF)
#define CLAMPF_TO_U32(x) ((x)*0xFFFFFFFF)
#define CLAMPF_TO_I32(x) ((x)*0x7FFFFFFF)

#define FLOAT_TO_U8(x)  (CLAMP((x), 0.f, 1.f)*0xFF)

#define U8_TO_FLOAT(x) ((x)/(float)(0xFF))
#define U16_TO_FLOAT(x) ((x)/(float)(0xFFFF))
#define U32_TO_FLOAT(x) ((x)/(float)(0xFFFFFFFF))
#define I8_TO_FLOAT(x) MAX((x)/(float)(0x7F),-1.f)
#define I16_TO_FLOAT(x) MAX((x)/(float)(0x7FFF),-1.f)
#define I32_TO_FLOAT(x) MAX((x)/(float)(0x7FFFFFFF),-1.f)

#define GL_SET_STATE(var, value, dirty_flag) ({ \
    typeof(value) _v = (value); \
    if (_v != var) { \
        dirty_flag = true; \
    } \
    var = _v; \
})

typedef struct {
    surface_t *color_buffer;
    void *depth_buffer;
} gl_framebuffer_t;

typedef struct {
    GLfloat position[4];
    GLfloat screen_pos[2];
    GLfloat color[4];
    GLfloat texcoord[2];
    GLfloat inverse_w;
    GLfloat depth;
    uint8_t clip;
} gl_vertex_t;

typedef struct {
    GLfloat m[4][4];
} gl_matrix_t;

typedef struct {
    GLfloat scale[3];
    GLfloat offset[3];
} gl_viewport_t;

typedef struct {
    gl_matrix_t *storage;
    int32_t size;
    int32_t cur_depth;
} gl_matrix_stack_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    GLenum internal_format;
    void *data;
} gl_texture_image_t;

typedef struct {
    gl_texture_image_t levels[MAX_TEXTURE_LEVELS];
    uint32_t num_levels;
    GLenum dimensionality;
    GLenum wrap_s;
    GLenum wrap_t;
    GLenum min_filter;
    GLenum mag_filter;
    GLclampf border_color[4];
    GLclampf priority;
    bool is_complete;
} gl_texture_object_t;

typedef struct {
    gl_vertex_t *vertices[CLIPPING_PLANE_COUNT + 3];
    bool edge_flags[CLIPPING_PLANE_COUNT + 3];
    uint32_t count;
} gl_clipping_list_t;

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
    GLfloat position[4];
    GLfloat direction[3];
    GLfloat spot_exponent;
    GLfloat spot_cutoff;
    GLfloat constant_attenuation;
    GLfloat linear_attenuation;
    GLfloat quadratic_attenuation;
    bool enabled;
} gl_light_t;

typedef struct {
    GLuint name;
    uint32_t size;
    GLenum usage;
    GLenum access;
    bool mapped;
    GLvoid *pointer;
    GLvoid *data;
} gl_buffer_object_t;

typedef struct {
    GLint size;
    GLenum type;
    GLsizei stride;
    const GLvoid *pointer;
    gl_buffer_object_t *binding;
    bool enabled;
} gl_array_t;

typedef void (*read_attrib_func)(GLfloat*,const void*,uint32_t);

typedef struct {
    const GLvoid *pointer;
    read_attrib_func read_func;
    uint16_t stride;
    uint8_t size;
    uint8_t elem_size;
    bool copy_before_draw;
    void *tmp_buffer;
    uint32_t tmp_buffer_size;
    const void *final_pointer;
    uint16_t final_stride;
    uint16_t offset;
} gl_vertex_source_t;

typedef struct {
    GLenum mode;
    GLfloat eye_plane[4];
    GLfloat object_plane[4];
    bool enabled;
} gl_tex_gen_t;

typedef struct {
    GLsizei size;
    GLfloat entries[MAX_PIXEL_MAP_SIZE];
} gl_pixel_map_t;

typedef struct {
    gl_framebuffer_t default_framebuffer;
    gl_framebuffer_t *cur_framebuffer;

    GLenum current_error;

    GLenum draw_buffer;

    GLenum primitive_mode;

    GLfloat point_size;
    GLfloat line_width;

    GLclampf clear_color[4];
    GLclampd clear_depth;

    uint32_t scissor_box[4];

    GLfloat persp_norm_factor;

    bool cull_face;
    GLenum cull_face_mode;
    GLenum front_face;
    GLenum polygon_mode;

    GLenum blend_src;
    GLenum blend_dst;
    uint32_t blend_cycle;

    GLenum depth_func;

    GLenum alpha_func;
    GLclampf alpha_ref;

    GLfloat fog_start;
    GLfloat fog_end;

    bool scissor_test;
    bool depth_test;
    bool texture_1d;
    bool texture_2d;
    bool blend;
    bool alpha_test;
    bool dither;
    bool lighting;
    bool fog;
    bool color_material;
    bool multisample;
    bool normalize;

    gl_vertex_t vertex_cache[VERTEX_CACHE_SIZE];
    uint32_t vertex_cache_indices[VERTEX_CACHE_SIZE];
    uint32_t lru_age_table[VERTEX_CACHE_SIZE];
    uint32_t lru_next_age;
    uint8_t next_cache_index;
    bool lock_next_vertex;
    uint8_t locked_vertex;

    uint32_t prim_size;
    uint32_t primitive_indices[3];
    uint32_t primitive_progress;
    uint32_t prim_counter;
    void (*primitive_func)(void);

    GLfloat current_color[4];
    GLfloat current_texcoord[4];
    GLfloat current_normal[3];

    gl_vertex_source_t vertex_sources[4];
    void *tmp_index_buffer;
    uint32_t tmp_index_buffer_size;

    gl_viewport_t current_viewport;

    GLenum matrix_mode;
    gl_matrix_t final_matrix;
    gl_matrix_t *current_matrix;

    gl_matrix_t modelview_stack_storage[MODELVIEW_STACK_SIZE];
    gl_matrix_t projection_stack_storage[PROJECTION_STACK_SIZE];
    gl_matrix_t texture_stack_storage[TEXTURE_STACK_SIZE];

    gl_matrix_stack_t modelview_stack;
    gl_matrix_stack_t projection_stack;
    gl_matrix_stack_t texture_stack;
    gl_matrix_stack_t *current_matrix_stack;

    gl_texture_object_t default_texture_1d;
    gl_texture_object_t default_texture_2d;

    obj_map_t texture_objects;
    GLuint next_tex_name;

    gl_texture_object_t *texture_1d_object;
    gl_texture_object_t *texture_2d_object;

    gl_material_t material;
    gl_light_t lights[LIGHT_COUNT];

    GLfloat light_model_ambient[4];
    bool light_model_local_viewer;
    bool light_model_two_side;

    GLenum shade_model;

    gl_tex_gen_t s_gen;
    gl_tex_gen_t t_gen;
    gl_tex_gen_t r_gen;
    gl_tex_gen_t q_gen;

    gl_array_t vertex_array;
    gl_array_t texcoord_array;
    gl_array_t normal_array;
    gl_array_t color_array;

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

    GLenum tex_env_mode;
    GLfloat tex_env_color[4];

    obj_map_t list_objects;
    GLuint next_list_name;
    GLuint list_base;
    GLuint current_list;

    obj_map_t buffer_objects;
    GLuint next_buffer_name;

    gl_buffer_object_t *array_buffer;
    gl_buffer_object_t *element_array_buffer;

    bool immediate_active;
    bool is_points;

    bool is_scissor_dirty;
    bool is_rendermode_dirty;
    bool is_texture_dirty;
} gl_state_t;

void gl_matrix_init();
void gl_texture_init();
void gl_lighting_init();
void gl_rendermode_init();
void gl_array_init();
void gl_primitive_init();
void gl_pixel_init();
void gl_list_init();
void gl_buffer_init();

void gl_texture_close();
void gl_primitive_close();
void gl_list_close();
void gl_buffer_close();

void gl_set_error(GLenum error);

gl_matrix_t * gl_matrix_stack_get_matrix(gl_matrix_stack_t *stack);

void gl_matrix_mult(GLfloat *d, const gl_matrix_t *m, const GLfloat *v);
void gl_matrix_mult3x3(GLfloat *d, const gl_matrix_t *m, const GLfloat *v);
void gl_matrix_mult4x2(GLfloat *d, const gl_matrix_t *m, const GLfloat *v);

bool gl_is_invisible();

void gl_update_scissor();
void gl_update_render_mode();
void gl_update_texture();

void gl_perform_lighting(GLfloat *color, const GLfloat *input, const GLfloat *v, const GLfloat *n, const gl_material_t *material);

gl_texture_object_t * gl_get_active_texture();

float dot_product3(const float *a, const float *b);
void gl_normalize(GLfloat *d, const GLfloat *v);

uint32_t gl_get_type_size(GLenum type);

void read_f32(GLfloat *dst, const float *src, uint32_t count);

#endif
