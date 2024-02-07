#ifndef __GL_INTERNAL
#define __GL_INTERNAL

#include "GL/gl.h"
#include "GL/gl_integration.h"
#include "obj_map.h"
#include "surface.h"
#include "../utils.h"
#include <stdbool.h>
#include <math.h>
#include "gl_constants.h"
#include "rspq.h"
#include "rdpq.h"
#include "../rdpq/rdpq_internal.h"
#include "rdpq_tri.h"

#define RADIANS(x) ((x) * M_PI / 180.0f)

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

#define RGBA32_FROM_FLOAT(r, g, b, a) RGBA32(FLOAT_TO_U8(r), FLOAT_TO_U8(g), FLOAT_TO_U8(b), FLOAT_TO_U8(a))
#define PACKED_RGBA32_FROM_FLOAT(r, g, b, a) color_to_packed32(RGBA32_FROM_FLOAT(r, g, b, a))

#define GL_SET_STATE(var, value) ({ \
    typeof(value) _v = (value); \
    bool dirty_flag = _v != var; \
    var = _v; \
    dirty_flag; \
})

#define gl_set_error(error, message, ...)  ({ \
    state.current_error = error; \
    assertf(error == GL_NO_ERROR, "%s: " message, #error, ##__VA_ARGS__); \
})

#define gl_ensure_no_begin_end() ({ \
    if (state.begin_end_active) { \
        gl_set_error(GL_INVALID_OPERATION, "%s is not allowed between glBegin/glEnd", __func__); \
        false; \
    } \
    true; \
})

#define gl_assert_no_display_list() assertf(state.current_list == 0, "%s cannot be recorded into a display list", __func__)

typedef int16_t int16u_t __attribute__((aligned(1)));
typedef uint16_t uint16u_t __attribute__((aligned(1)));
typedef int32_t int32u_t __attribute__((aligned(1)));
typedef uint32_t uint32u_t __attribute__((aligned(1)));
typedef float floatu __attribute__((aligned(1)));
typedef double doubleu __attribute__((aligned(1)));

extern uint32_t gl_overlay_id;
extern uint32_t glp_overlay_id;
extern uint32_t gl_rsp_state;

#define gl_write(cmd_id, ...)               rspq_write(gl_overlay_id, cmd_id, ##__VA_ARGS__)
#define glp_write(cmd_id, ...)              rspq_write(glp_overlay_id, cmd_id, ##__VA_ARGS__)
#define gl_write_rdp(rdpcmds, cmd_id, ...)  rdpq_write(rdpcmds, gl_overlay_id, cmd_id, ##__VA_ARGS__)
#define glp_write_rdp(rdpcmds, cmd_id, ...) rdpq_write(rdpcmds, glp_overlay_id, cmd_id, ##__VA_ARGS__)


typedef enum {
    GL_CMD_SET_FLAG         = 0x0,
    GL_CMD_SET_BYTE         = 0x1,
    GL_CMD_SET_SHORT        = 0x2,
    GL_CMD_SET_WORD         = 0x3,
    GL_CMD_SET_LONG         = 0x4,
    GL_CMD_UPDATE           = 0x5,
    GL_CMD_BIND_TEXTURE     = 0x6,
    GL_CMD_GET_VALUE        = 0x7,
    GL_CMD_COPY_FILL_COLOR  = 0x8,
    GL_CMD_SET_LIGHT_POS    = 0x9,
    GL_CMD_MATRIX_PUSH      = 0xA,
    GL_CMD_MATRIX_POP       = 0xB,
    GL_CMD_MATRIX_LOAD      = 0xC,
    GL_CMD_PRE_INIT_PIPE    = 0xD,
    GL_CMD_PRE_INIT_PIPE_TEX= 0xE,
    GL_CMD_SET_PALETTE_IDX  = 0xF,
    GL_CMD_MATRIX_COPY      = 0x10,
} gl_command_t;

typedef enum {
    GLP_CMD_INIT_PIPE           = 0x0,
    GLP_CMD_SET_VTX_LOADER      = 0x1,
    GLP_CMD_SET_VTX_CMD_SIZE    = 0x2,
    GLP_CMD_DRAW_TRI            = 0x3,
    GLP_CMD_SET_PRIM_VTX        = 0x4,
    GLP_CMD_SET_BYTE            = 0x5,
    GLP_CMD_SET_WORD            = 0x6,
    GLP_CMD_SET_LONG            = 0x7,
} glp_command_t;

typedef enum {
    GL_UPDATE_NONE                  = 0x0,
    GL_UPDATE_SCISSOR               = 0x1,
    GL_UPDATE_TEXTURE_COMPLETENESS  = 0x2,
    GL_UPDATE_TEXTURE_OBJECTS       = 0x3,
} gl_update_func_t;

typedef enum {
    ATTRIB_VERTEX,
    ATTRIB_COLOR,
    ATTRIB_TEXCOORD,
    ATTRIB_NORMAL,
    ATTRIB_MTX_INDEX,
    ATTRIB_COUNT
} gl_array_type_t;

typedef struct {
    GLfloat position[4];
    GLfloat color[4];
    GLfloat texcoord[4];
    GLfloat normal[3];
    GLubyte mtx_index[VERTEX_UNIT_COUNT];
} gl_obj_attributes_t;

typedef struct {
    GLfloat screen_pos[2];
    GLfloat depth;
    GLfloat shade[4];
    GLfloat texcoord[2];
    GLfloat inv_w;
    GLfloat cs_pos[4];
    gl_obj_attributes_t obj_attributes;
    uint8_t clip_code;
    uint8_t tr_code;
    uint8_t t_l_applied;
} gl_vtx_t;

#define VTX_SCREEN_POS_OFFSET   (offsetof(gl_vtx_t, screen_pos)  / sizeof(float))
#define VTX_SHADE_OFFSET        (offsetof(gl_vtx_t, shade)       / sizeof(float))
#define VTX_TEXCOORD_OFFSET     (offsetof(gl_vtx_t, texcoord)    / sizeof(float))
#define VTX_DEPTH_OFFSET        (offsetof(gl_vtx_t, depth)       / sizeof(float))

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
    gl_matrix_stack_t *mv_stack;
    gl_matrix_t mvp;
    bool is_mvp_dirty;
} gl_matrix_target_t;

typedef struct {
    int16_t  i[4][4];
    uint16_t f[4][4];
} gl_matrix_srv_t;
_Static_assert(sizeof(gl_matrix_srv_t) == MATRIX_SIZE, "Matrix size does not match");

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t internal_format;
} __attribute__((packed)) gl_texture_image_t;
_Static_assert(sizeof(gl_texture_image_t) == TEXTURE_IMAGE_SIZE, "Texture image has incorrect size!");
_Static_assert(offsetof(gl_texture_image_t, width) == IMAGE_WIDTH_OFFSET, "Texture image has incorrect layout!");
_Static_assert(offsetof(gl_texture_image_t, height) == IMAGE_HEIGHT_OFFSET, "Texture image has incorrect layout!");
_Static_assert(offsetof(gl_texture_image_t, internal_format) == IMAGE_INTERNAL_FORMAT_OFFSET, "Texture image has incorrect layout!");

typedef struct {
    gl_texture_image_t levels[MAX_TEXTURE_LEVELS];
    uint8_t levels_count; // number of mipmaps minus one
    uint8_t tlut_mode;
    uint32_t levels_block[MAX_TEXTURE_LEVELS*2+1];
    uint32_t flags;
    uint16_t min_filter;
    uint16_t mag_filter;
} __attribute__((aligned(16), packed)) gl_srv_texture_object_t;
_Static_assert(sizeof(gl_srv_texture_object_t) == TEXTURE_OBJECT_SIZE, "Texture object has incorrect size!");
_Static_assert((TEXTURE_OBJECT_SIZE % 8) == 0, "Texture object has incorrect size!");
_Static_assert(offsetof(gl_srv_texture_object_t, levels_count)      == TEXTURE_LEVELS_COUNT_OFFSET, "Texture object has incorrect layout!");
_Static_assert(offsetof(gl_srv_texture_object_t, tlut_mode)         == TEXTURE_TLUT_MODE_OFFSET, "Texture object has incorrect layout!");
_Static_assert(offsetof(gl_srv_texture_object_t, levels_block)      == TEXTURE_LEVELS_BLOCK_OFFSET, "Texture object has incorrect layout!");
_Static_assert((TEXTURE_LEVELS_BLOCK_OFFSET % 4) == 0, "Texture object has incorrect layout!");
_Static_assert(offsetof(gl_srv_texture_object_t, flags)             == TEXTURE_FLAGS_OFFSET, "Texture object has incorrect layout!");
_Static_assert(offsetof(gl_srv_texture_object_t, min_filter)        == TEXTURE_MIN_FILTER_OFFSET, "Texture object has incorrect layout!");
_Static_assert(offsetof(gl_srv_texture_object_t, mag_filter)        == TEXTURE_MAG_FILTER_OFFSET, "Texture object has incorrect layout!");

typedef enum {
    TEX_IS_DEFAULT  = 0x1,
    TEX_HAS_IMAGE   = 0x2,
} gl_texture_flag_t;

typedef struct {
    GLenum dimensionality;
    uint16_t flags;
    uint16_t wrap_s;
    uint16_t wrap_t;

    sprite_t *sprite;
    surface_t surfaces[MAX_TEXTURE_LEVELS];
    rspq_block_t *blocks[MAX_TEXTURE_LEVELS];

    gl_srv_texture_object_t *srv_object;
} gl_texture_object_t;

typedef struct {
    gl_vtx_t *vertices[CLIPPING_PLANE_COUNT + 3];
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
    GLfloat spot_cutoff_cos;
    GLfloat constant_attenuation;
    GLfloat linear_attenuation;
    GLfloat quadratic_attenuation;
    bool enabled;
} gl_light_t;

typedef struct {
    int16_t position[LIGHT_COUNT][4];
    int16_t ambient[LIGHT_COUNT][4];
    int16_t diffuse[LIGHT_COUNT][4];
    int16_t attenuation_int[LIGHT_COUNT][4];
    uint16_t attenuation_frac[LIGHT_COUNT][4];
} __attribute__((packed)) gl_lights_soa_t;
_Static_assert(sizeof(gl_lights_soa_t) == LIGHT_STRUCT_SIZE);
_Static_assert(offsetof(gl_lights_soa_t, position) == LIGHT_POSITION_OFFSET);
_Static_assert(offsetof(gl_lights_soa_t, ambient) == LIGHT_AMBIENT_OFFSET);
_Static_assert(offsetof(gl_lights_soa_t, diffuse) == LIGHT_DIFFUSE_OFFSET);
_Static_assert(offsetof(gl_lights_soa_t, attenuation_int) == LIGHT_ATTENUATION_INT_OFFSET);
_Static_assert(offsetof(gl_lights_soa_t, attenuation_frac) == LIGHT_ATTENUATION_FRAC_OFFSET);

typedef struct {
    GLvoid *data;
    uint32_t size;
} gl_storage_t;

typedef struct {
    GLenum usage;
    GLenum access;
    GLvoid *pointer;
    gl_storage_t storage;
    bool mapped;
} gl_buffer_object_t;

typedef struct {
    rspq_write_t w;
    union {
        uint8_t  bytes[4];
        uint32_t word;
    };
    uint32_t buffer_head;
} gl_cmd_stream_t;

typedef void (*cpu_read_attrib_func)(void*,const void*,uint32_t);
typedef void (*rsp_read_attrib_func)(gl_cmd_stream_t*,const void*,uint32_t);

typedef struct {
    GLint size;
    GLenum type;
    GLsizei stride;
    const GLvoid *pointer;
    gl_buffer_object_t *binding;
    bool normalize;
    bool enabled;

    const GLvoid *final_pointer;
    uint16_t final_stride;
    cpu_read_attrib_func cpu_read_func;
    rsp_read_attrib_func rsp_read_func;
} gl_array_t;

typedef struct {
    gl_array_t arrays[ATTRIB_COUNT];
} gl_array_object_t;

typedef uint32_t (*read_index_func)(const void*,uint32_t);

typedef struct {
    GLenum mode;
    GLfloat eye_plane[TEX_COORD_COUNT];
    GLfloat object_plane[TEX_COORD_COUNT];
    bool enabled;
} gl_tex_gen_t;

typedef struct {
    int16_t integer[TEX_COORD_COUNT][TEX_GEN_PLANE_COUNT][TEX_GEN_COUNT];
    uint16_t fraction[TEX_COORD_COUNT][TEX_GEN_PLANE_COUNT][TEX_GEN_COUNT];
    uint16_t mode[TEX_GEN_COUNT];
    uint16_t mode_const[4];
} gl_tex_gen_soa_t;
_Static_assert(sizeof(gl_tex_gen_soa_t) == TEX_GEN_STRUCT_SIZE);
_Static_assert(offsetof(gl_tex_gen_soa_t, integer) == TEX_GEN_INTEGER_OFFSET);
_Static_assert(offsetof(gl_tex_gen_soa_t, fraction) == TEX_GEN_FRACTION_OFFSET);
_Static_assert(offsetof(gl_tex_gen_soa_t, mode) == TEX_GEN_MODE_OFFSET);

typedef struct {
    int16_t factor_int;
    int16_t offset_int;
    uint16_t factor_frac;
    uint16_t offset_frac;
} gl_fog_params_t;

typedef struct {
    GLsizei size;
    GLfloat entries[MAX_PIXEL_MAP_SIZE];
} gl_pixel_map_t;

typedef struct {
    void (*begin)();
    void (*end)();
    void (*vertex)(const void*,GLenum,uint32_t);
    void (*color)(const void*,GLenum,uint32_t);
    void (*tex_coord)(const void*,GLenum,uint32_t);
    void (*normal)(const void*,GLenum,uint32_t);
    void (*mtx_index)(const void*,GLenum,uint32_t);
    void (*array_element)(uint32_t);
    void (*draw_arrays)(uint32_t,uint32_t);
    void (*draw_elements)(uint32_t,const void*,read_index_func);
} gl_pipeline_t;

typedef struct {
    GLuint target_precision;
    GLuint precision;
    GLint shift_amount;
    GLfloat to_float_factor;
} gl_fixed_precision_t;

typedef struct {
    // Pipeline state

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

    GLenum cull_face_mode;
    GLenum front_face;
    GLenum polygon_mode;

    GLenum primitive_mode;

    GLfloat point_size;
    GLfloat line_width;

    GLfloat fog_start;
    GLfloat fog_end;
    GLfloat fog_offset;
    GLfloat fog_factor;

    gl_material_t material;
    gl_light_t lights[LIGHT_COUNT];

    GLfloat light_model_ambient[4];
    bool light_model_local_viewer;

    GLenum shade_model;

    gl_tex_gen_t tex_gen[TEX_GEN_COUNT];

    gl_viewport_t current_viewport;

    GLenum matrix_mode;
    GLint current_palette_matrix;

    gl_matrix_t *current_matrix;

    gl_matrix_t modelview_stack_storage[MODELVIEW_STACK_SIZE];
    gl_matrix_t projection_stack_storage[PROJECTION_STACK_SIZE];
    gl_matrix_t texture_stack_storage[TEXTURE_STACK_SIZE];
    gl_matrix_t palette_stack_storage[MATRIX_PALETTE_SIZE][PALETTE_STACK_SIZE];

    gl_matrix_stack_t modelview_stack;
    gl_matrix_stack_t projection_stack;
    gl_matrix_stack_t texture_stack;
    gl_matrix_stack_t palette_stacks[MATRIX_PALETTE_SIZE];
    gl_matrix_stack_t *current_matrix_stack;

    gl_matrix_target_t default_matrix_target;
    gl_matrix_target_t palette_matrix_targets[MATRIX_PALETTE_SIZE];
    gl_matrix_target_t *current_matrix_target;

    bool begin_end_active;

    gl_texture_object_t *texture_1d_object;
    gl_texture_object_t *texture_2d_object;

    gl_obj_attributes_t current_attributes;

    uint8_t prim_size;
    uint8_t prim_indices[3];
    uint8_t prim_progress;
    uint32_t prim_counter;
    uint8_t (*prim_func)(void);
    uint32_t prim_id;
    bool lock_next_vertex;
    uint8_t locked_vertex;

    uint16_t prim_tex_width;
    uint16_t prim_tex_height;
    bool prim_texture;
    bool prim_bilinear;
    uint8_t prim_mipmaps;

    int32_t last_array_element;

    rdpq_trifmt_t trifmt;

    gl_vtx_t vertex_cache[VERTEX_CACHE_SIZE];
    uint32_t vertex_cache_ids[VERTEX_CACHE_SIZE];
    uint32_t lru_age_table[VERTEX_CACHE_SIZE];
    uint32_t lru_next_age;

    gl_vtx_t *primitive_vertices[3];

    GLfloat flat_color[4];

    // Client state

    const surface_t *color_buffer;

    GLenum current_error;

    gl_array_object_t default_array_object;
    gl_array_object_t *array_object;

    gl_texture_object_t *default_textures;

    obj_map_t list_objects;
    GLuint next_list_name;
    GLuint list_base;
    GLuint current_list;

    gl_buffer_object_t *array_buffer;
    gl_buffer_object_t *element_array_buffer;

    gl_matrix_srv_t *matrix_stacks[3];
    gl_matrix_srv_t *matrix_palette;

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

    gl_fixed_precision_t vertex_halfx_precision;
    gl_fixed_precision_t texcoord_halfx_precision;

    bool can_use_rsp;
    bool can_use_rsp_dirty;

    const gl_pipeline_t *current_pipeline;
} gl_state_t;

typedef struct {
    gl_matrix_srv_t matrices[5];
    gl_lights_soa_t lights;
    gl_tex_gen_soa_t tex_gen;
    int16_t viewport_scale[4];
    int16_t viewport_offset[4];
    int16_t light_ambient[4];
    int16_t mat_ambient[4];
    int16_t mat_diffuse[4];
    int16_t mat_specular[4];
    int16_t mat_emissive[4];
    uint16_t mat_color_target[3];
    uint16_t mat_shininess;
    int16_t color[4];
    int16_t tex_coords[4];
    int8_t normal[3];
    uint8_t mtx_index;
    uint32_t matrix_pointers[5];
    uint32_t loaded_mtx_index[2];
    uint32_t flags;
    gl_fog_params_t fog_params;
    uint16_t tex_size[2];
    uint16_t tex_offset[2];
    uint16_t polygon_mode;
    uint16_t prim_type;
    uint16_t cull_mode;
    uint16_t front_face;
    uint16_t shade_model;
    uint16_t point_size;
    uint16_t line_width;
    uint16_t matrix_mode;
    uint16_t tri_cmd;
    uint8_t tri_cull[2];

    gl_srv_texture_object_t bound_textures[2];
    uint16_t scissor_rect[4];
    uint32_t blend_cycle;
    uint32_t fog_color;
    uint32_t flags2;
    uint32_t texture_ids[2];
    uint32_t uploaded_tex;
    uint32_t clear_color;
    uint32_t clear_depth;
    uint32_t palette_index;
    uint32_t dither_mode;
    uint16_t fb_size[2];
    uint16_t depth_func;
    uint16_t alpha_func;
    uint16_t blend_src;
    uint16_t blend_dst;
    uint16_t tex_env_mode;
    uint8_t alpha_ref;
    uint8_t palette_dirty[PALETTE_DIRTY_FLAGS_SIZE];
} __attribute__((aligned(8), packed)) gl_server_state_t;

_Static_assert((offsetof(gl_server_state_t, bound_textures) & 0x7) == 0, "Bound textures must be aligned to 8 bytes in server state");

void gl_matrix_init();
void gl_texture_init();
void gl_lighting_init();
void gl_rendermode_init();
void gl_array_init();
void gl_primitive_init();
void gl_pixel_init();
void gl_list_init();

void gl_texture_close();
void gl_primitive_close();
void gl_list_close();

gl_matrix_t * gl_matrix_stack_get_matrix(gl_matrix_stack_t *stack);

void gl_update_matrix_targets();

void gl_matrix_mult(GLfloat *d, const gl_matrix_t *m, const GLfloat *v);
void gl_matrix_mult3x3(GLfloat *d, const gl_matrix_t *m, const GLfloat *v);
void gl_matrix_mult4x2(GLfloat *d, const gl_matrix_t *m, const GLfloat *v);

void gl_perform_lighting(GLfloat *color, const GLfloat *input, const GLfloat *v, const GLfloat *n, const gl_material_t *material);

gl_texture_object_t * gl_get_active_texture();

void gl_cross(GLfloat* p, const GLfloat* a, const GLfloat* b);
float dot_product3(const float *a, const float *b);
void gl_normalize(GLfloat *d, const GLfloat *v);

uint32_t gl_get_type_size(GLenum type);

bool gl_storage_alloc(gl_storage_t *storage, uint32_t size);
void gl_storage_free(gl_storage_t *storage);
bool gl_storage_resize(gl_storage_t *storage, uint32_t new_size);

void set_can_use_rsp_dirty();

void gl_update_array_pointers(gl_array_object_t *obj);

void gl_fill_attrib_defaults(gl_array_type_t array_type, uint32_t size);
void gl_fill_all_attrib_defaults(const gl_array_t *arrays);
void gl_load_attribs(const gl_array_t *arrays, uint32_t index);
bool gl_get_cache_index(uint32_t vertex_id, uint8_t *cache_index);
bool gl_prim_assembly(uint8_t cache_index, uint8_t *indices);
void gl_read_attrib(gl_array_type_t array_type, const void *value, GLenum type, uint32_t size);

inline uint32_t next_pow2(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
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
    default:
        return -1;
    }
}

#define next_prim_id() (state.prim_id++)

inline const void *gl_get_attrib_element(const gl_array_t *src, uint32_t index)
{
    return src->final_pointer + index * src->final_stride;
}

inline gl_cmd_stream_t gl_cmd_stream_begin(uint32_t ovl_id, uint32_t cmd_id, int size)
{
    return (gl_cmd_stream_t) {
        .w = rspq_write_begin(ovl_id, cmd_id, size),
        .buffer_head = 2,
    };
}

inline void gl_cmd_stream_commit(gl_cmd_stream_t *s)
{
    rspq_write_arg(&s->w, s->word);
    s->buffer_head = 0;
    s->word = 0;
}

inline void gl_cmd_stream_put_byte(gl_cmd_stream_t *s, uint8_t v)
{
    s->bytes[s->buffer_head++] = v;
    
    if (s->buffer_head == sizeof(uint32_t)) {
        gl_cmd_stream_commit(s);
    }
}

inline void gl_cmd_stream_put_half(gl_cmd_stream_t *s, uint16_t v)
{
    s->bytes[s->buffer_head++] = v >> 8;
    s->bytes[s->buffer_head++] = v & 0xFF;
    
    if (s->buffer_head == sizeof(uint32_t)) {
        gl_cmd_stream_commit(s);
    }
}

inline void gl_cmd_stream_end(gl_cmd_stream_t *s)
{
    if (s->buffer_head > 0) {
        gl_cmd_stream_commit(s);
    }

    rspq_write_end(&s->w);
}

inline bool is_in_heap_memory(void *ptr)
{
    ptr = CachedAddr(ptr);
    return ptr >= HEAP_START_ADDR && ptr < ((void*)KSEG0_START_ADDR + get_memory_size());
}

inline bool is_valid_object_id(GLuint id)
{
    return is_in_heap_memory((void*)id);
}

inline bool gl_tex_is_complete(const gl_texture_object_t *obj)
{
    return obj->srv_object->flags & TEX_FLAG_COMPLETE;
}

inline uint8_t gl_tex_get_levels(const gl_texture_object_t *obj)
{
    return obj->srv_object->levels_count + 1;
}

__attribute__((always_inline))
inline int gl_get_rdpcmds_for_update_func(gl_update_func_t update_func)
{
    switch (update_func) {
    case GL_UPDATE_NONE:                 return 0;
    case GL_UPDATE_SCISSOR:              return 1;
    case GL_UPDATE_TEXTURE_COMPLETENESS: return 0;
    case GL_UPDATE_TEXTURE_OBJECTS:      return 0;
    }
    __builtin_unreachable();
}

__attribute__((always_inline))
inline void gl_set_flag_raw(gl_update_func_t update_func, uint32_t offset, uint32_t flag, bool value)
{
    gl_write_rdp(gl_get_rdpcmds_for_update_func(update_func),
        GL_CMD_SET_FLAG, _carg(update_func, 0x7FF, 13) | _carg(offset, 0xFFC, 0) | _carg(value, 0x1, 0), value ? flag : ~flag);
}

__attribute__((always_inline))
inline void gl_set_flag(gl_update_func_t update_func, uint32_t flag, bool value)
{
    gl_set_flag_raw(update_func, offsetof(gl_server_state_t, flags), flag, value);
}

__attribute__((always_inline))
inline void gl_set_flag_word2(gl_update_func_t update_func, uint32_t flag, bool value)
{
    gl_set_flag_raw(update_func, offsetof(gl_server_state_t, flags2), flag, value);
}

__attribute__((always_inline))
inline void gl_set_byte(gl_update_func_t update_func, uint32_t offset, uint8_t value)
{
    gl_write_rdp(gl_get_rdpcmds_for_update_func(update_func),
        GL_CMD_SET_BYTE, _carg(update_func, 0x7FF, 13) | _carg(offset, 0xFFF, 0), value);
}

__attribute__((always_inline))
inline void gl_set_short(gl_update_func_t update_func, uint32_t offset, uint16_t value)
{
    gl_write_rdp(gl_get_rdpcmds_for_update_func(update_func),
        GL_CMD_SET_SHORT, _carg(update_func, 0x7FF, 13) | _carg(offset, 0xFFF, 0), value);
}

__attribute__((always_inline))
inline void gl_set_word(gl_update_func_t update_func, uint32_t offset, uint32_t value)
{
    gl_write_rdp(gl_get_rdpcmds_for_update_func(update_func),
        GL_CMD_SET_WORD, _carg(update_func, 0x7FF, 13) | _carg(offset, 0xFFF, 0), value);
}

__attribute__((always_inline))
inline void gl_set_long(gl_update_func_t update_func, uint32_t offset, uint64_t value)
{
    gl_write_rdp(gl_get_rdpcmds_for_update_func(update_func),
        GL_CMD_SET_LONG, _carg(update_func, 0x7FF, 13) | _carg(offset, 0xFFF, 0), value >> 32, value & 0xFFFFFFFF);
}

__attribute__((always_inline))
inline void gl_update(gl_update_func_t update_func)
{
    gl_write_rdp(gl_get_rdpcmds_for_update_func(update_func),
        GL_CMD_UPDATE, _carg(update_func, 0x7FF, 13));
}

inline void gl_get_value(void *dst, uint32_t offset, uint32_t size)
{
    gl_write(GL_CMD_GET_VALUE, _carg(size-1, 0xFFF, 12) | _carg(offset, 0xFFF, 0), PhysicalAddr(dst));
}

inline void gl_bind_texture(GLenum target, gl_texture_object_t *texture)
{
    uint32_t index = target == GL_TEXTURE_2D ? 1 : 0;
    uint32_t id_offset = index * sizeof(uint32_t);
    uint32_t tex_offset = index * sizeof(gl_srv_texture_object_t);
    gl_write(GL_CMD_BIND_TEXTURE, (id_offset << 16) | tex_offset, PhysicalAddr(texture->srv_object));
}

inline void gl_update_texture_completeness(uint32_t offset)
{
    gl_write(GL_CMD_UPDATE, _carg(GL_UPDATE_TEXTURE_COMPLETENESS, 0x7FF, 13) | (offset - offsetof(gl_server_state_t, bound_textures)));
}

inline void* gl_get_attrib_pointer(gl_obj_attributes_t *attribs, gl_array_type_t array_type)
{
    switch (array_type) {
    case ATTRIB_VERTEX:
        return attribs->position;
    case ATTRIB_COLOR:
        return attribs->color;
    case ATTRIB_TEXCOORD:
        return attribs->texcoord;
    case ATTRIB_NORMAL:
        return attribs->normal;
    case ATTRIB_MTX_INDEX:
        return attribs->mtx_index;
    default:
        assert(0);
        return NULL;
    }
}

inline void gl_set_current_color(GLfloat *color)
{
    int16_t r_fx = FLOAT_TO_I16(color[0]);
    int16_t g_fx = FLOAT_TO_I16(color[1]);
    int16_t b_fx = FLOAT_TO_I16(color[2]);
    int16_t a_fx = FLOAT_TO_I16(color[3]);
    
    uint64_t packed = ((uint64_t)r_fx << 48) | ((uint64_t)g_fx << 32) | ((uint64_t)b_fx << 16) | (uint64_t)a_fx;
    gl_set_long(GL_UPDATE_NONE, offsetof(gl_server_state_t, color), packed);
}

inline void gl_set_current_texcoords(GLfloat *texcoords)
{
    int16_t fixed_s = texcoords[0] * (1 << 5);
    int16_t fixed_t = texcoords[1] * (1 << 5);
    int16_t fixed_r = texcoords[2] * (1 << 5);
    int16_t fixed_q = texcoords[3] * (1 << 5);

    uint64_t packed = ((uint64_t)fixed_s << 48) | ((uint64_t)fixed_t << 32) | ((uint64_t)fixed_r << 16) | (uint64_t)fixed_q;
    gl_set_long(GL_UPDATE_NONE, offsetof(gl_server_state_t, tex_coords), packed);
}

inline void gl_set_current_normal(GLfloat *normal)
{
    int8_t fixed_nx = normal[0] * 0x7F;
    int8_t fixed_ny = normal[1] * 0x7F;
    int8_t fixed_nz = normal[2] * 0x7F;

    uint32_t packed = ((uint32_t)fixed_nx << 24) | ((uint32_t)fixed_ny << 16) | ((uint32_t)fixed_nz << 8);
    gl_set_word(GL_UPDATE_NONE, offsetof(gl_server_state_t, normal), packed);
}

inline void gl_set_current_mtx_index(GLubyte *index)
{
    for (uint32_t i = 0; i < VERTEX_UNIT_COUNT; i++)
    {
        gl_set_byte(GL_UPDATE_NONE, offsetof(gl_server_state_t, mtx_index) + i, index[i]);
    }
}

inline void gl_set_palette_idx(uint32_t index)
{
    gl_write(GL_CMD_SET_PALETTE_IDX, index * sizeof(gl_matrix_srv_t));
}

inline void gl_pre_init_pipe(GLenum primitive_mode)
{
    // PreInitPipeTex will run a block with nesting level 1 for texture upload.
    // The command itself does not emit RDP commands (the block does that, so
    // we use a plain gl_write() for it.
    rspq_block_run_rsp(1);
    gl_write(GL_CMD_PRE_INIT_PIPE_TEX);

    // PreInitPipe is similar to rdpq_set_mode_standard wrt RDP commands.
    // It issues SET_SCISSOR + CC + SOM.
    gl_write_rdp(3, GL_CMD_PRE_INIT_PIPE, primitive_mode);
}

inline void glpipe_init()
{
    glp_write(GLP_CMD_INIT_PIPE, gl_rsp_state);
}

inline void glpipe_set_vtx_cmd_size(uint16_t patched_cmd_descriptor, uint16_t *cmd_descriptor)
{
    glp_write(GLP_CMD_SET_VTX_CMD_SIZE, patched_cmd_descriptor, PhysicalAddr(cmd_descriptor));
}

#define TEX_SCALE   32.0f
#define OBJ_SCALE   32.0f

inline void glpipe_draw_triangle(int i0, int i1, int i2)
{
    // We pass -1 because the triangle can be clipped and split into multiple
    // triangles.
    glp_write_rdp(-1, GLP_CMD_DRAW_TRI,
        (i0*PRIM_VTX_SIZE),
        ((i1*PRIM_VTX_SIZE)<<16) | (i2*PRIM_VTX_SIZE)
    );
}

#endif
