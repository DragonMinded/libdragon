#include "GL/gl.h"
#include "rdpq.h"
#include "rdpq_rect.h"
#include "rdpq_mode.h"
#include "rdpq_debug.h"
#include "rspq.h"
#include "display.h"
#include "rdp.h"
#include "utils.h"
#include <string.h>
#include <math.h>
#include <malloc.h>
#include "gl_internal.h"

DEFINE_RSP_UCODE(rsp_gl);
DEFINE_RSP_UCODE(rsp_gl_pipeline);

uint32_t gl_overlay_id;
uint32_t glp_overlay_id;
uint32_t gl_rsp_state;

gl_state_t state;

uint32_t gl_get_type_size(GLenum type)
{
    switch (type) {
    case GL_BYTE:
        return sizeof(GLbyte);
    case GL_UNSIGNED_BYTE:
        return sizeof(GLubyte);
    case GL_SHORT:
        return sizeof(GLshort);
    case GL_UNSIGNED_SHORT:
        return sizeof(GLushort);
    case GL_INT:
        return sizeof(GLint);
    case GL_UNSIGNED_INT:
        return sizeof(GLuint);
    case GL_FLOAT:
        return sizeof(GLfloat);
    case GL_DOUBLE:
        return sizeof(GLdouble);
    case GL_HALF_FIXED_N64:
        return sizeof(GLhalfxN64);
    default:
        return 0;
    }
}

void gl_init()
{
    rdpq_init();

    memset(&state, 0, sizeof(state));

    gl_texture_init();

    gl_server_state_t *server_state = UncachedAddr(rspq_overlay_get_state(&rsp_gl));
    memset(server_state, 0, sizeof(gl_server_state_t));

    memcpy(&server_state->bound_textures[0], state.default_textures[0].srv_object, sizeof(gl_srv_texture_object_t));
    memcpy(&server_state->bound_textures[1], state.default_textures[1].srv_object, sizeof(gl_srv_texture_object_t));
    server_state->texture_ids[0] = PhysicalAddr(state.default_textures[0].srv_object);
    server_state->texture_ids[1] = PhysicalAddr(state.default_textures[1].srv_object);

    server_state->color[0] = 0x7FFF;
    server_state->color[1] = 0x7FFF;
    server_state->color[2] = 0x7FFF;
    server_state->color[3] = 0x7FFF;
    server_state->tex_coords[3] = 1 << 5;
    server_state->normal[2] = 0x7F;

    server_state->point_size = 1 << 2;
    server_state->line_width = 1 << 2;
    server_state->polygon_mode = GL_FILL;

    server_state->tex_gen.mode[0] = GL_EYE_LINEAR;
    server_state->tex_gen.mode[1] = GL_EYE_LINEAR;
    server_state->tex_gen.mode[2] = GL_EYE_LINEAR;
    server_state->tex_gen.mode[3] = GL_EYE_LINEAR;
    
    server_state->tex_gen.mode_const[0] = GL_OBJECT_LINEAR;
    server_state->tex_gen.mode_const[1] = GL_EYE_LINEAR;
    server_state->tex_gen.mode_const[2] = GL_SPHERE_MAP;

    server_state->tex_gen.integer[0][0][0] = 1;
    server_state->tex_gen.integer[0][1][0] = 1;

    server_state->tex_gen.integer[1][0][1] = 1;
    server_state->tex_gen.integer[1][1][1] = 1;

    state.matrix_stacks[0] = malloc_uncached(sizeof(gl_matrix_srv_t) * MODELVIEW_STACK_SIZE);
    state.matrix_stacks[1] = malloc_uncached(sizeof(gl_matrix_srv_t) * PROJECTION_STACK_SIZE);
    state.matrix_stacks[2] = malloc_uncached(sizeof(gl_matrix_srv_t) * TEXTURE_STACK_SIZE);
    state.matrix_palette = malloc_uncached(sizeof(gl_matrix_srv_t) * MATRIX_PALETTE_SIZE * 2); // Double size for mvp-matrices

    server_state->matrix_pointers[0] = PhysicalAddr(state.matrix_stacks[0]);
    server_state->matrix_pointers[1] = PhysicalAddr(state.matrix_stacks[1]);
    server_state->matrix_pointers[2] = PhysicalAddr(state.matrix_stacks[2]);
    server_state->matrix_pointers[3] = PhysicalAddr(state.matrix_palette);
    server_state->matrix_pointers[4] = PhysicalAddr(state.matrix_palette + MATRIX_PALETTE_SIZE);
    server_state->loaded_mtx_index[0] = -1;
    server_state->loaded_mtx_index[1] = -1;

    server_state->flags |= FLAG_FINAL_MTX_DIRTY;

    server_state->mat_ambient[0] = 0x1999;  // 0.2
    server_state->mat_ambient[1] = 0x1999;  // 0.2
    server_state->mat_ambient[2] = 0x1999;  // 0.2
    server_state->mat_ambient[3] = 0x7FFF;  // 1.0
    server_state->mat_diffuse[0] = 0x6666;  // 0.8
    server_state->mat_diffuse[1] = 0x6666;  // 0.8
    server_state->mat_diffuse[2] = 0x6666;  // 0.8
    server_state->mat_diffuse[3] = 0x7FFF;  // 1.0
    server_state->mat_specular[3] = 0x7FFF; // 1.0
    server_state->mat_emissive[3] = 0x7FFF; // 1.0
    server_state->mat_color_target[0] = 1;
    server_state->mat_color_target[1] = 1;

    for (uint32_t i = 0; i < LIGHT_COUNT; i++)
    {
        server_state->lights.position[i][2] = 0x7FFF; // 1.0
        server_state->lights.ambient[i][3] = 0x7FFF;  // 1.0
        server_state->lights.diffuse[i][3] = 0x7FFF;  // 1.0
        server_state->lights.attenuation_frac[i][0] = 1 << 15; // 1.0
    }
    
    server_state->light_ambient[0] = 0x1999; // 0.2
    server_state->light_ambient[1] = 0x1999; // 0.2
    server_state->light_ambient[2] = 0x1999; // 0.2
    server_state->light_ambient[3] = 0x7FFF; // 1.0

    server_state->dither_mode = DITHER_SQUARE_SQUARE << (SOM_ALPHADITHER_SHIFT - 32);

    gl_overlay_id = rspq_overlay_register(&rsp_gl);
    glp_overlay_id = rspq_overlay_register(&rsp_gl_pipeline);
    gl_rsp_state = PhysicalAddr(rspq_overlay_get_state(&rsp_gl));

    gl_matrix_init();
    gl_lighting_init();
    gl_rendermode_init();
    gl_array_init();
    gl_primitive_init();
    gl_pixel_init();
    gl_list_init();

    glDepthRange(0, 1);
    glClearDepth(1.0);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

void gl_close()
{
    rspq_wait();

    gl_list_close();
    gl_primitive_close();
    gl_texture_close();
    rspq_overlay_unregister(gl_overlay_id);
    rspq_overlay_unregister(glp_overlay_id);

    // FIXME: some of the above to deferred deletions, others don't.
    // So we need another rspq_wait.
    rspq_wait();

    free_uncached(state.matrix_stacks[0]);
    free_uncached(state.matrix_stacks[1]);
    free_uncached(state.matrix_stacks[2]);
    free_uncached(state.matrix_palette);    
}

void gl_reset_uploaded_texture()
{
    gl_set_word(GL_UPDATE_NONE, offsetof(gl_server_state_t, uploaded_tex), 0);
}

void gl_context_begin()
{
    const surface_t *old_color_buffer = state.color_buffer;
    
    state.color_buffer = rdpq_get_attached();
    assertf(state.color_buffer, "GL: Tried to begin rendering without framebuffer attached");

    uint32_t width = state.color_buffer->width;
    uint32_t height = state.color_buffer->height;

    if (old_color_buffer == NULL || old_color_buffer->width != width || old_color_buffer->height != height) {
        uint32_t packed_size = ((uint32_t)width) << 16 | (uint32_t)height;
        gl_set_word(GL_UPDATE_NONE, offsetof(gl_server_state_t, fb_size), packed_size);
        glViewport(0, 0, width, height);
        glScissor(0, 0, width, height);
    }

    gl_reset_uploaded_texture();
}

void gl_context_end()
{
}

GLenum glGetError(void)
{
    if (!gl_ensure_no_begin_end()) return 0;

    GLenum error = state.current_error;
    state.current_error = GL_NO_ERROR;
    return error;
}

void gl_set_flag2(GLenum target, bool value)
{
    switch (target) {
    case GL_RDPQ_MATERIAL_N64:
        gl_set_flag_word2(GL_UPDATE_NONE, FLAG2_USE_RDPQ_MATERIAL, value);
        break;
    case GL_RDPQ_TEXTURING_N64:
        gl_set_flag_word2(GL_UPDATE_NONE, FLAG2_USE_RDPQ_TEXTURING, value);
        gl_reset_uploaded_texture();
        break;
    case GL_SCISSOR_TEST:
        gl_set_flag(GL_UPDATE_SCISSOR, FLAG_SCISSOR_TEST, value);
        break;
    case GL_DEPTH_TEST:
        gl_set_flag(GL_UPDATE_NONE, FLAG_DEPTH_TEST, value);
        state.depth_test = value;
        break;
    case GL_BLEND:
        gl_set_flag(GL_UPDATE_NONE, FLAG_BLEND, value);
        break;
    case GL_ALPHA_TEST:
        gl_set_flag(GL_UPDATE_NONE, FLAG_ALPHA_TEST, value);
        break;
    case GL_DITHER:
        gl_set_flag(GL_UPDATE_NONE, FLAG_DITHER, value);
        break;
    case GL_FOG:
        gl_set_flag(GL_UPDATE_NONE, FLAG_FOG, value);
        state.fog = value;
        break;
    case GL_MULTISAMPLE_ARB:
        gl_set_flag_word2(GL_UPDATE_NONE, FLAG2_MULTISAMPLE, value);
        break;
    case GL_TEXTURE_1D:
        gl_set_flag(GL_UPDATE_NONE, FLAG_TEXTURE_1D, value);
        state.texture_1d = value;
        break;
    case GL_TEXTURE_2D:
        gl_set_flag(GL_UPDATE_NONE, FLAG_TEXTURE_2D, value);
        state.texture_2d = value;
        break;
    case GL_CULL_FACE:
        gl_set_flag(GL_UPDATE_NONE, FLAG_CULL_FACE, value);
        state.cull_face = value;
        break;
    case GL_LIGHTING:
        gl_set_flag(GL_UPDATE_NONE, FLAG_LIGHTING, value);
        state.lighting = value;
        set_can_use_rsp_dirty();
        break;
    case GL_LIGHT0:
    case GL_LIGHT1:
    case GL_LIGHT2:
    case GL_LIGHT3:
    case GL_LIGHT4:
    case GL_LIGHT5:
    case GL_LIGHT6:
    case GL_LIGHT7:
        uint32_t light_index = target - GL_LIGHT0;
        gl_set_flag(GL_UPDATE_NONE, FLAG_LIGHT0 << light_index, value);
        state.lights[light_index].enabled = value;
        break;
    case GL_COLOR_MATERIAL:
        gl_set_flag(GL_UPDATE_NONE, FLAG_COLOR_MATERIAL, value);
        state.color_material = value;
        break;
    case GL_TEXTURE_GEN_S:
    case GL_TEXTURE_GEN_T:
    case GL_TEXTURE_GEN_R:
    case GL_TEXTURE_GEN_Q:
        uint32_t tex_gen_index = target - GL_TEXTURE_GEN_S;
        gl_set_flag(GL_UPDATE_NONE, FLAG_TEX_GEN_S << tex_gen_index, value);
        state.tex_gen[tex_gen_index].enabled = value;
        set_can_use_rsp_dirty();
        break;
    case GL_NORMALIZE:
        gl_set_flag(GL_UPDATE_NONE, FLAG_NORMALIZE, value);
        state.normalize = value;
        break;
    case GL_MATRIX_PALETTE_ARB:
        gl_set_flag(GL_UPDATE_NONE, FLAG_MATRIX_PALETTE, value);
        state.matrix_palette_enabled = value;
        break;
    case GL_TEXTURE_FLIP_T_N64:
        gl_set_flag_word2(GL_UPDATE_NONE, FLAG2_TEX_FLIP_T, value);
        state.tex_flip_t = value;
        break;
    case GL_CLIP_PLANE0:
    case GL_CLIP_PLANE1:
    case GL_CLIP_PLANE2:
    case GL_CLIP_PLANE3:
    case GL_CLIP_PLANE4:
    case GL_CLIP_PLANE5:
        assertf(!value, "User clip planes are not supported!");
        break;
    case GL_STENCIL_TEST:
        assertf(!value, "Stencil test is not supported!");
        break;
    case GL_COLOR_LOGIC_OP:
    case GL_INDEX_LOGIC_OP:
        assertf(!value, "Logical pixel operation is not supported!");
        break;
    case GL_POINT_SMOOTH:
    case GL_LINE_SMOOTH:
    case GL_POLYGON_SMOOTH:
        assertf(!value, "Smooth rendering is not supported (Use multisampling instead)!");
        break;
    case GL_LINE_STIPPLE:
    case GL_POLYGON_STIPPLE:
        assertf(!value, "Stipple is not supported!");
        break;
    case GL_POLYGON_OFFSET_FILL:
    case GL_POLYGON_OFFSET_LINE:
    case GL_POLYGON_OFFSET_POINT:
        assertf(!value, "Polygon offset is not supported!");
        break;
    case GL_SAMPLE_ALPHA_TO_COVERAGE_ARB:
    case GL_SAMPLE_ALPHA_TO_ONE_ARB:
    case GL_SAMPLE_COVERAGE_ARB:
        assertf(!value, "Coverage value manipulation is not supported!");
        break;
    case GL_MAP1_COLOR_4:
    case GL_MAP1_INDEX:
    case GL_MAP1_NORMAL:
    case GL_MAP1_TEXTURE_COORD_1:
    case GL_MAP1_TEXTURE_COORD_2:
    case GL_MAP1_TEXTURE_COORD_3:
    case GL_MAP1_TEXTURE_COORD_4:
    case GL_MAP1_VERTEX_3:
    case GL_MAP1_VERTEX_4:
    case GL_MAP2_COLOR_4:
    case GL_MAP2_INDEX:
    case GL_MAP2_NORMAL:
    case GL_MAP2_TEXTURE_COORD_1:
    case GL_MAP2_TEXTURE_COORD_2:
    case GL_MAP2_TEXTURE_COORD_3:
    case GL_MAP2_TEXTURE_COORD_4:
    case GL_MAP2_VERTEX_3:
    case GL_MAP2_VERTEX_4:
        assertf(!value, "Evaluators are not supported!");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid enable target", target);
        return;
    }
}

void glEnable(GLenum target)
{
    if (!gl_ensure_no_begin_end()) return;
    gl_set_flag2(target, true);
}

void glDisable(GLenum target)
{
    if (!gl_ensure_no_begin_end()) return;
    gl_set_flag2(target, false);
}

void gl_copy_fill_color(uint32_t offset)
{
    __rdpq_autosync_change(AUTOSYNC_PIPE);
    gl_write_rdp(1, GL_CMD_COPY_FILL_COLOR, offset);
}

void glClear(GLbitfield buf)
{
    extern void __rdpq_set_mode_fill(void);
    extern void __rdpq_clear_z(const uint16_t *z);
    extern void __rdpq_clear(const color_t* color);

    if (!gl_ensure_no_begin_end()) return;
    
    if (!buf) {
        return;
    }

    rdpq_mode_push();
    __rdpq_set_mode_fill();

    if (buf & (GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT)) {
        assertf(0, "Only color and depth buffers are supported!");
    }

    if (buf & GL_DEPTH_BUFFER_BIT) {
        gl_copy_fill_color(offsetof(gl_server_state_t, clear_depth));
        __rdpq_clear_z(NULL);
    }

    if (buf & GL_COLOR_BUFFER_BIT) {
        gl_copy_fill_color(offsetof(gl_server_state_t, clear_color));
        __rdpq_clear(NULL);
    }

    rdpq_mode_pop();
}

void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a)
{
    if (!gl_ensure_no_begin_end()) return;
    
    color_t clear_color = RGBA32(CLAMPF_TO_U8(r), CLAMPF_TO_U8(g), CLAMPF_TO_U8(b), CLAMPF_TO_U8(a));
    gl_set_word(GL_UPDATE_NONE, offsetof(gl_server_state_t, clear_color), color_to_packed32(clear_color));
}

void glClearDepth(GLclampd d)
{
    if (!gl_ensure_no_begin_end()) return;
    
    color_t clear_depth = color_from_packed16((uint16_t)(d * 0xFFFF) & 0xFFFC);
    gl_set_word(GL_UPDATE_NONE, offsetof(gl_server_state_t, clear_depth), color_to_packed32(clear_depth));
}

void glDitherModeN64(rdpq_dither_t mode){
    gl_set_word(GL_UPDATE_NONE, offsetof(gl_server_state_t, dither_mode), mode << (SOM_ALPHADITHER_SHIFT - 32));
}

void glFlush(void)
{
    if (!gl_ensure_no_begin_end()) return;
    
    rspq_flush();
}

void glFinish(void)
{
    if (!gl_ensure_no_begin_end()) return;
    
    rspq_wait();
}

void glHint(GLenum target, GLenum hint)
{
    if (!gl_ensure_no_begin_end()) return;
    
    switch (target)
    {
    case GL_PERSPECTIVE_CORRECTION_HINT:
        // TODO: enable/disable texture perspective correction?
        break;
    case GL_FOG_HINT:
        // TODO: per-pixel fog
        break;
    case GL_MULTISAMPLE_HINT_N64:
        // Use full AA by default, unless RA has been requested
        gl_set_flag_word2(GL_UPDATE_NONE, FLAG2_REDUCED_ALIASING, hint == GL_FASTEST);
        break;
    case GL_POINT_SMOOTH_HINT:
    case GL_LINE_SMOOTH_HINT:
    case GL_POLYGON_SMOOTH_HINT:
        // Ignored
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid hint target", target);
        break;
    }
}

bool gl_storage_alloc(gl_storage_t *storage, uint32_t size)
{
    GLvoid *mem = malloc_uncached(size);
    if (mem == NULL) {
        return false;
    }

    storage->data = mem;
    storage->size = size;

    return true;
}

void gl_storage_free(gl_storage_t *storage)
{
    // TODO: need to wait until buffer is no longer used!

    if (storage->data != NULL) {
        free(storage->data);
        storage->data = NULL;
    }
}

bool gl_storage_resize(gl_storage_t *storage, uint32_t new_size)
{
    if (storage->size >= new_size) {
        return true;
    }

    GLvoid *mem = malloc(new_size);
    if (mem == NULL) {
        return false;
    }

    gl_storage_free(storage);

    storage->data = mem;
    storage->size = new_size;

    return true;
}

void glTexSizeN64(GLushort width, GLushort height)
{
    width <<= TEX_COORD_SHIFT;
    height <<= TEX_COORD_SHIFT;
    gl_set_word(GL_UPDATE_NONE, offsetof(gl_server_state_t, tex_size[0]), (width << 16) | height);
}

extern inline uint32_t next_pow2(uint32_t v);
extern inline bool is_in_heap_memory(void *ptr);
extern inline void gl_set_flag_raw(gl_update_func_t update_func, uint32_t offset, uint32_t flag, bool value);
extern inline void gl_set_flag(gl_update_func_t update_func, uint32_t flag, bool value);
extern inline void gl_set_flag_word2(gl_update_func_t update_func, uint32_t flag, bool value);
extern inline void gl_set_byte(gl_update_func_t update_func, uint32_t offset, uint8_t value);
extern inline void gl_set_short(gl_update_func_t update_func, uint32_t offset, uint16_t value);
extern inline void gl_set_word(gl_update_func_t update_func, uint32_t offset, uint32_t value);
extern inline void gl_set_long(gl_update_func_t update_func, uint32_t offset, uint64_t value);
extern inline void gl_update(gl_update_func_t update_func);
extern inline void gl_get_value(void *dst, uint32_t offset, uint32_t size);
extern inline void gl_bind_texture(GLenum target, gl_texture_object_t *texture);
extern inline void gl_update_texture_completeness(uint32_t offset);
extern inline void gl_set_palette_idx(uint32_t index);
extern inline void gl_set_current_color(GLfloat *color);
extern inline void gl_set_current_texcoords(GLfloat *texcoords);
extern inline void gl_set_current_normal(GLfloat *normal);
extern inline void gl_pre_init_pipe(GLenum primitive_mode);
extern inline void glpipe_init();
extern inline void glpipe_draw_triangle(int i0, int i1, int i2);
extern inline int gl_get_rdpcmds_for_update_func(gl_update_func_t update_func);
extern inline void* gl_get_attrib_pointer(gl_obj_attributes_t *attribs, gl_array_type_t array_type);
extern inline uint32_t gl_type_to_index(GLenum type);
extern inline void gl_set_current_mtx_index(GLubyte *index);
extern inline gl_cmd_stream_t gl_cmd_stream_begin(uint32_t ovl_id, uint32_t cmd_id, int size);
extern inline void gl_cmd_stream_commit(gl_cmd_stream_t *s);
extern inline void gl_cmd_stream_put_byte(gl_cmd_stream_t *s, uint8_t v);
extern inline void gl_cmd_stream_put_half(gl_cmd_stream_t *s, uint16_t v);
extern inline void gl_cmd_stream_end(gl_cmd_stream_t *s);
extern inline void glpipe_set_vtx_cmd_size(uint16_t patched_cmd_descriptor, uint16_t *cmd_descriptor);
