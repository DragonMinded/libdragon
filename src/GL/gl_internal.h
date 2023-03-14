#ifndef __GL_INTERNAL
#define __GL_INTERNAL

#include "GL/gl.h"
#include "GL/gl_integration.h"
#include "obj_map.h"
#include "surface.h"
#include "utils.h"
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

extern uint32_t gl_overlay_id;
extern uint32_t glp_overlay_id;
extern uint32_t gl_rsp_state;

#define gl_write(cmd_id, ...)  rspq_write(gl_overlay_id, cmd_id, ##__VA_ARGS__)
#define glp_write(cmd_id, ...) rspq_write(glp_overlay_id, cmd_id, ##__VA_ARGS__)

enum {
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
};

enum {
    GLP_CMD_INIT_PIPE      = 0x00,
    GLP_CMD_DRAW_TRI       = 0x01,
    GLP_CMD_SET_PRIM_VTX   = 0x02,
};

typedef enum {
    GL_UPDATE_NONE                  = 0x0,
    GL_UPDATE_SCISSOR               = 0x1,
    GL_UPDATE_TEXTURE_COMPLETENESS  = 0x2,
} gl_update_func_t;

typedef enum {
    ATTRIB_VERTEX,
    ATTRIB_COLOR,
    ATTRIB_TEXCOORD,
    ATTRIB_NORMAL,
    ATTRIB_COUNT
} gl_array_type_t;

typedef struct {
    GLfloat screen_pos[2];
    GLfloat depth;
    GLfloat shade[4];
    GLfloat texcoord[2];
    GLfloat inv_w;
    GLfloat cs_pos[4];
    GLfloat obj_pos[4];
    GLfloat color[4];
    GLfloat obj_texcoord[4];
    GLfloat normal[3];
    uint8_t clip_code;
    uint8_t tr_code;
    uint8_t t_l_applied;
    uint8_t padding;
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
    int16_t  i[4][4];
    uint16_t f[4][4];
} gl_matrix_srv_t;
_Static_assert(sizeof(gl_matrix_srv_t) == MATRIX_SIZE, "Matrix size does not match");

typedef struct {
    uint32_t tex_image;
    void *data;
    uint32_t set_load_tile;
    uint32_t load_block;
    uint32_t set_tile;
    uint16_t width;
    uint16_t height;
    uint16_t stride;
    uint16_t internal_format;
    uint16_t tmem_size;
    uint8_t width_log;
    uint8_t height_log;
} __attribute__((aligned(16), packed)) gl_texture_image_t;
_Static_assert(sizeof(gl_texture_image_t) == TEXTURE_IMAGE_SIZE, "Texture image has incorrect size!");
_Static_assert(offsetof(gl_texture_image_t, tex_image) == IMAGE_TEX_IMAGE_OFFSET, "Texture image has incorrect layout!");
_Static_assert(offsetof(gl_texture_image_t, data) == IMAGE_DATA_OFFSET, "Texture image has incorrect layout!");
_Static_assert(offsetof(gl_texture_image_t, set_load_tile) == IMAGE_SET_LOAD_TILE_OFFSET, "Texture image has incorrect layout!");
_Static_assert(offsetof(gl_texture_image_t, load_block) == IMAGE_LOAD_BLOCK_OFFSET, "Texture image has incorrect layout!");
_Static_assert(offsetof(gl_texture_image_t, set_tile) == IMAGE_SET_TILE_OFFSET, "Texture image has incorrect layout!");
_Static_assert(offsetof(gl_texture_image_t, width) == IMAGE_WIDTH_OFFSET, "Texture image has incorrect layout!");
_Static_assert(offsetof(gl_texture_image_t, height) == IMAGE_HEIGHT_OFFSET, "Texture image has incorrect layout!");
_Static_assert(offsetof(gl_texture_image_t, stride) == IMAGE_STRIDE_OFFSET, "Texture image has incorrect layout!");
_Static_assert(offsetof(gl_texture_image_t, internal_format) == IMAGE_INTERNAL_FORMAT_OFFSET, "Texture image has incorrect layout!");
_Static_assert(offsetof(gl_texture_image_t, tmem_size) == IMAGE_TMEM_SIZE_OFFSET, "Texture image has incorrect layout!");
_Static_assert(offsetof(gl_texture_image_t, width_log) == IMAGE_WIDTH_LOG_OFFSET, "Texture image has incorrect layout!");
_Static_assert(offsetof(gl_texture_image_t, height_log) == IMAGE_HEIGHT_LOG_OFFSET, "Texture image has incorrect layout!");

typedef struct {
    gl_texture_image_t levels[MAX_TEXTURE_LEVELS];

    uint32_t flags;
    int32_t priority;
    uint16_t wrap_s;
    uint16_t wrap_t;
    uint16_t min_filter;
    uint16_t mag_filter;

    // These properties are not DMA'd
    uint16_t dimensionality;
    uint16_t padding[7];
} __attribute__((aligned(16), packed)) gl_texture_object_t;
_Static_assert(sizeof(gl_texture_object_t) == TEXTURE_OBJECT_SIZE, "Texture object has incorrect size!");
_Static_assert((1 << TEXTURE_OBJECT_SIZE_LOG) == TEXTURE_OBJECT_SIZE, "Texture object has incorrect size!");
_Static_assert(offsetof(gl_texture_object_t, flags)             == TEXTURE_FLAGS_OFFSET, "Texture object has incorrect layout!");
_Static_assert(offsetof(gl_texture_object_t, priority)          == TEXTURE_PRIORITY_OFFSET, "Texture object has incorrect layout!");
_Static_assert(offsetof(gl_texture_object_t, wrap_s)            == TEXTURE_WRAP_S_OFFSET, "Texture object has incorrect layout!");
_Static_assert(offsetof(gl_texture_object_t, wrap_t)            == TEXTURE_WRAP_T_OFFSET, "Texture object has incorrect layout!");
_Static_assert(offsetof(gl_texture_object_t, min_filter)        == TEXTURE_MIN_FILTER_OFFSET, "Texture object has incorrect layout!");
_Static_assert(offsetof(gl_texture_object_t, mag_filter)        == TEXTURE_MAG_FILTER_OFFSET, "Texture object has incorrect layout!");
_Static_assert(offsetof(gl_texture_object_t, dimensionality)    == TEXTURE_DIMENSIONALITY_OFFSET, "Texture object has incorrect layout!");

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
    GLint size;
    GLenum type;
    GLsizei stride;
    const GLvoid *pointer;
    gl_buffer_object_t *binding;
    bool normalize;
    bool enabled;
} gl_array_t;

typedef struct {
    gl_array_t arrays[ATTRIB_COUNT];
} gl_array_object_t;

typedef void (*read_attrib_func)(GLfloat*,const void*,uint32_t);

typedef struct {
    const GLvoid *pointer;
    read_attrib_func read_func;
    uint16_t stride;
    uint8_t size;
} gl_attrib_source_t;

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
    GLsizei size;
    GLfloat entries[MAX_PIXEL_MAP_SIZE];
} gl_pixel_map_t;

typedef struct {
    int frame_id;
    uint32_t count;
    uint64_t *slots;
} gl_deletion_list_t;

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

    GLenum cull_face_mode;
    GLenum front_face;
    GLenum polygon_mode;

    GLenum primitive_mode;

    GLfloat point_size;
    GLfloat line_width;

    GLfloat fog_start;
    GLfloat fog_end;

    gl_material_t material;
    gl_light_t lights[LIGHT_COUNT];

    GLfloat light_model_ambient[4];
    bool light_model_local_viewer;

    GLenum shade_model;

    gl_tex_gen_t tex_gen[TEX_GEN_COUNT];

    gl_viewport_t current_viewport;

    GLenum matrix_mode;
    gl_matrix_t final_matrix;
    gl_matrix_t *current_matrix;
    bool final_matrix_dirty;

    gl_matrix_t modelview_stack_storage[MODELVIEW_STACK_SIZE];
    gl_matrix_t projection_stack_storage[PROJECTION_STACK_SIZE];
    gl_matrix_t texture_stack_storage[TEXTURE_STACK_SIZE];

    gl_matrix_stack_t modelview_stack;
    gl_matrix_stack_t projection_stack;
    gl_matrix_stack_t texture_stack;
    gl_matrix_stack_t *current_matrix_stack;

    bool immediate_active;

    gl_texture_object_t *texture_1d_object;
    gl_texture_object_t *texture_2d_object;

    GLfloat current_attribs[ATTRIB_COUNT][4];

    uint8_t prim_size;
    uint8_t prim_indices[3];
    uint8_t prim_progress;
    uint32_t prim_counter;
    uint8_t (*prim_func)(void);
    uint16_t prim_id;
    bool lock_next_vertex;
    uint8_t locked_vertex;

    uint16_t prim_tex_width;
    uint16_t prim_tex_height;
    bool prim_texture;
    bool prim_bilinear;
    uint8_t prim_mipmaps;

    rdpq_trifmt_t trifmt;

    gl_vtx_t vertex_cache[VERTEX_CACHE_SIZE];
    uint16_t vertex_cache_ids[VERTEX_CACHE_SIZE];
    uint32_t lru_age_table[VERTEX_CACHE_SIZE];
    uint32_t lru_next_age;

    gl_vtx_t *primitive_vertices[3];

    GLfloat flat_color[4];

    // Client state

    const surface_t *color_buffer;

    GLenum current_error;

    gl_array_object_t default_array_object;
    gl_array_object_t *array_object;

    gl_attrib_source_t attrib_sources[ATTRIB_COUNT];

    gl_texture_object_t *default_textures;

    obj_map_t list_objects;
    GLuint next_list_name;
    GLuint list_base;
    GLuint current_list;

    gl_buffer_object_t *array_buffer;
    gl_buffer_object_t *element_array_buffer;

    gl_matrix_srv_t *matrix_stacks[3];

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

    gl_deletion_list_t deletion_lists[MAX_DELETION_LISTS];
    gl_deletion_list_t *current_deletion_list;

    int frame_id;
    volatile int frames_complete;

    bool rsp_pipeline_enabled;
    bool can_use_rsp;
    bool can_use_rsp_dirty;
} gl_state_t;

typedef struct {
    gl_matrix_srv_t matrices[4];
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
    int8_t normal[4];
    uint32_t matrix_pointers[3];
    uint32_t flags;
    int32_t fog_start;
    int32_t fog_end;
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

    gl_texture_object_t bound_textures[2];
    uint16_t scissor_rect[4];
    uint32_t blend_cycle;
    uint32_t fog_color;
    uint32_t flags2;
    uint32_t texture_ids[2];
    uint32_t uploaded_tex;
    uint32_t clear_color;
    uint32_t clear_depth;
    uint16_t fb_size[2];
    uint16_t depth_func;
    uint16_t alpha_func;
    uint16_t blend_src;
    uint16_t blend_dst;
    uint16_t tex_env_mode;
    uint8_t alpha_ref;
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

#define gl_set_error(error)  ({ \
    state.current_error = error; \
    assertf(error == GL_NO_ERROR, "GL error: 0x%04x (%s)", error, #error); \
})

gl_matrix_t * gl_matrix_stack_get_matrix(gl_matrix_stack_t *stack);

void gl_update_final_matrix();

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

uint64_t * gl_reserve_deletion_slot();

void set_can_use_rsp_dirty();

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
    return obj->flags & TEX_FLAG_COMPLETE;
}

inline uint8_t gl_tex_get_levels(const gl_texture_object_t *obj)
{
    return obj->flags & 0x7;
}

inline void gl_set_flag_raw(gl_update_func_t update_func, uint32_t offset, uint32_t flag, bool value)
{
    gl_write(GL_CMD_SET_FLAG, _carg(update_func, 0x7FF, 13) | _carg(offset, 0xFFC, 0) | _carg(value, 0x1, 0), value ? flag : ~flag);
}

inline void gl_set_flag(gl_update_func_t update_func, uint32_t flag, bool value)
{
    gl_set_flag_raw(update_func, offsetof(gl_server_state_t, flags), flag, value);
}

inline void gl_set_flag_word2(gl_update_func_t update_func, uint32_t flag, bool value)
{
    gl_set_flag_raw(update_func, offsetof(gl_server_state_t, flags2), flag, value);
}

inline void gl_set_byte(gl_update_func_t update_func, uint32_t offset, uint8_t value)
{
    gl_write(GL_CMD_SET_BYTE, _carg(update_func, 0x7FF, 13) | _carg(offset, 0xFFF, 0), value);
}

inline void gl_set_short(gl_update_func_t update_func, uint32_t offset, uint16_t value)
{
    gl_write(GL_CMD_SET_SHORT, _carg(update_func, 0x7FF, 13) | _carg(offset, 0xFFF, 0), value);
}

inline void gl_set_word(gl_update_func_t update_func, uint32_t offset, uint32_t value)
{
    gl_write(GL_CMD_SET_WORD, _carg(update_func, 0x7FF, 13) | _carg(offset, 0xFFF, 0), value);
}

inline void gl_set_long(gl_update_func_t update_func, uint32_t offset, uint64_t value)
{
    gl_write(GL_CMD_SET_LONG, _carg(update_func, 0x7FF, 13) | _carg(offset, 0xFFF, 0), value >> 32, value & 0xFFFFFFFF);
}

inline void gl_update(gl_update_func_t update_func)
{
    gl_write(GL_CMD_UPDATE, _carg(update_func, 0x7FF, 13));
}

inline void gl_get_value(void *dst, uint32_t offset, uint32_t size)
{
    gl_write(GL_CMD_GET_VALUE, _carg(size-1, 0xFFF, 12) | _carg(offset, 0xFFF, 0), PhysicalAddr(dst));
}

inline void gl_bind_texture(GLenum target, gl_texture_object_t *texture)
{
    uint32_t is_2d = target == GL_TEXTURE_2D ? 1 : 0;
    gl_write(GL_CMD_BIND_TEXTURE, is_2d, PhysicalAddr(texture));
}

inline void gl_update_texture_completeness(uint32_t offset)
{
    gl_write(GL_CMD_UPDATE, _carg(GL_UPDATE_TEXTURE_COMPLETENESS, 0x7FF, 13) | (offset - offsetof(gl_server_state_t, bound_textures)));
}

inline void gl_pre_init_pipe(GLenum primitive_mode)
{
    gl_write(GL_CMD_PRE_INIT_PIPE, primitive_mode);
}

inline void glpipe_init()
{
    glp_write(GLP_CMD_INIT_PIPE, gl_rsp_state);
}

#define PRIM_VTX_SIZE   44

inline void glpipe_set_prim_vertex(int idx, GLfloat attribs[ATTRIB_COUNT][4])
{
    #define TEX_SCALE   32.0f
    #define OBJ_SCALE   32.0f
    #define fx16(v)  ((uint32_t)((int32_t)((v))) & 0xFFFF)

    uint32_t normal = (((uint32_t)(attribs[ATTRIB_NORMAL][0]*127.0f) & 0xFF) << 24) |
                      (((uint32_t)(attribs[ATTRIB_NORMAL][1]*127.0f) & 0xFF) << 16) |
                      (((uint32_t)(attribs[ATTRIB_NORMAL][2]*127.0f) & 0xFF) <<  8);

    glp_write(
        GLP_CMD_SET_PRIM_VTX, (idx*PRIM_VTX_SIZE), 
        (fx16(attribs[ATTRIB_VERTEX][0]*OBJ_SCALE) << 16) | fx16(attribs[ATTRIB_VERTEX][1]*OBJ_SCALE),
        (fx16(attribs[ATTRIB_VERTEX][2]*OBJ_SCALE) << 16) | fx16(attribs[ATTRIB_VERTEX][3]*OBJ_SCALE),
        (fx16(FLOAT_TO_I16(attribs[ATTRIB_COLOR][0])) << 16) | fx16(FLOAT_TO_I16(attribs[ATTRIB_COLOR][1])),
        (fx16(FLOAT_TO_I16(attribs[ATTRIB_COLOR][2])) << 16) | fx16(FLOAT_TO_I16(attribs[ATTRIB_COLOR][3])),
        (fx16(attribs[ATTRIB_TEXCOORD][0]*TEX_SCALE) << 16) | fx16(attribs[ATTRIB_TEXCOORD][1]*TEX_SCALE),
        (fx16(attribs[ATTRIB_TEXCOORD][2]*TEX_SCALE) << 16) | fx16(attribs[ATTRIB_TEXCOORD][3]*TEX_SCALE),
        normal
    );
}

inline void glpipe_draw_triangle(int i0, int i1, int i2)
{
    glp_write(GLP_CMD_DRAW_TRI,
        (i0*PRIM_VTX_SIZE),
        ((i1*PRIM_VTX_SIZE)<<16) | (i2*PRIM_VTX_SIZE)
    );
}

#endif
