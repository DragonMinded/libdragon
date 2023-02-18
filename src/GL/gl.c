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

    memcpy(&server_state->bound_textures, state.default_textures, sizeof(gl_texture_object_t) * 2);
    server_state->texture_ids[0] = PhysicalAddr(&state.default_textures[0]);
    server_state->texture_ids[1] = PhysicalAddr(&state.default_textures[1]);

    server_state->color[0] = 0x7FFF;
    server_state->color[1] = 0x7FFF;
    server_state->color[2] = 0x7FFF;
    server_state->color[3] = 0x7FFF;
    server_state->tex_coords[3] = 1 << 5;
    server_state->normal[2] = 0x7F;

    server_state->point_size = 1 << 2;
    server_state->line_width = 1 << 2;
    server_state->polygon_mode = GL_FILL;

    server_state->tex_gen_mode[0] = GL_EYE_LINEAR;
    server_state->tex_gen_mode[1] = GL_EYE_LINEAR;
    server_state->tex_gen_mode[2] = GL_EYE_LINEAR;
    server_state->tex_gen_mode[3] = GL_EYE_LINEAR;

    server_state->tex_gen[0].object_plane.integer[0] = 1;
    server_state->tex_gen[0].eye_plane.integer[0] = 1;

    server_state->tex_gen[1].object_plane.integer[1] = 1;
    server_state->tex_gen[1].eye_plane.integer[1] = 1;

    state.matrix_stacks[0] = malloc_uncached(sizeof(gl_matrix_srv_t) * MODELVIEW_STACK_SIZE);
    state.matrix_stacks[1] = malloc_uncached(sizeof(gl_matrix_srv_t) * PROJECTION_STACK_SIZE);
    state.matrix_stacks[2] = malloc_uncached(sizeof(gl_matrix_srv_t) * TEXTURE_STACK_SIZE);

    server_state->matrix_pointers[0] = PhysicalAddr(state.matrix_stacks[0]);
    server_state->matrix_pointers[1] = PhysicalAddr(state.matrix_stacks[1]);
    server_state->matrix_pointers[2] = PhysicalAddr(state.matrix_stacks[2]);

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
    for (uint32_t i = 0; i < MAX_DELETION_LISTS; i++)
    {
        gl_deletion_list_t *list = &state.deletion_lists[i];
        if (list->slots != NULL) {
            free_uncached(list->slots);
        }
    }

    free_uncached(state.matrix_stacks[0]);
    free_uncached(state.matrix_stacks[1]);
    free_uncached(state.matrix_stacks[2]);
    
    gl_list_close();
    gl_primitive_close();
    gl_texture_close();
    rspq_overlay_unregister(gl_overlay_id);
    rspq_overlay_unregister(glp_overlay_id);
    rdpq_close();
}

void gl_context_begin()
{
    const surface_t *old_color_buffer = state.color_buffer;
    
    state.color_buffer = rdpq_get_attached();
    assertf(state.color_buffer, "GL: Tried to begin rendering without framebuffer attached");

    uint32_t width = state.color_buffer->width;
    uint32_t height = state.color_buffer->height;

    if (old_color_buffer == NULL || old_color_buffer->width != width || old_color_buffer->height != height) {
        if (state.depth_buffer.buffer != NULL) {
            surface_free(&state.depth_buffer);
        }
        // TODO: allocate in separate RDRAM bank?
        state.depth_buffer = surface_alloc(FMT_RGBA16, width, height);

        uint32_t packed_size = ((uint32_t)width) << 16 | (uint32_t)height;
        gl_set_word(GL_UPDATE_NONE, offsetof(gl_server_state_t, fb_size), packed_size);
        glViewport(0, 0, width, height);
        glScissor(0, 0, width, height);
    }

    rdpq_set_z_image(&state.depth_buffer);

    state.frame_id++;
}

gl_deletion_list_t * gl_find_empty_deletion_list()
{
    gl_deletion_list_t *list = NULL;
    // Look for unused deletion list
    for (uint32_t i = 0; i < MAX_DELETION_LISTS; i++)
    {
        if (state.deletion_lists[i].count == 0) {
            list = &state.deletion_lists[i];
            break;
        }
    }

    assertf(list != NULL, "Ran out of deletion lists!");

    if (list->slots == NULL) {
        // TODO: maybe cached memory is more efficient in this case?
        list->slots = malloc_uncached(sizeof(uint64_t) * DELETION_LIST_SIZE);
    }

    list->frame_id = state.frame_id;
    return list;
}

uint64_t * gl_reserve_deletion_slot()
{
    if (state.current_deletion_list == NULL) {
        state.current_deletion_list = gl_find_empty_deletion_list();
    }

    gl_deletion_list_t *list = state.current_deletion_list;

    // TODO: how to deal with list being full?
    assertf(list->count < DELETION_LIST_SIZE, "Deletion list is full!");

    uint64_t *slot = &list->slots[list->count];
    list->count++;
    return slot;
}

void gl_handle_deletion_lists()
{
    int frames_complete = state.frames_complete;
    MEMORY_BARRIER();

    for (uint32_t i = 0; i < MAX_DELETION_LISTS; i++)
    {
        gl_deletion_list_t *list = &state.deletion_lists[i];
        if (list->count == 0) continue;
        
        // Skip if the frame is not complete yet
        int difference = (int)((uint32_t)(list->frame_id) - (uint32_t)(frames_complete));
        if (difference >= 0) {
            continue;
        }
        
        for (uint32_t j = 0; j < list->count; j++)
        {
            volatile uint32_t *slots = (volatile uint32_t*)list->slots;
            uint32_t phys_ptr = slots[j*2 + 1];
            if (phys_ptr == 0) continue;

            void *ptr = UncachedAddr(KSEG0_START_ADDR + (phys_ptr & 0xFFFFFFFF));
            free_uncached(ptr);
        }

        list->count = 0;
    }

    state.current_deletion_list = NULL;
}

void gl_on_frame_complete(void *ptr)
{
    state.frames_complete = (uint32_t)ptr;
}

void gl_context_end()
{
    assertf(state.modelview_stack.cur_depth == 0, "Modelview stack not empty");
    assertf(state.projection_stack.cur_depth == 0, "Projection stack not empty");
    assertf(state.texture_stack.cur_depth == 0, "Texture stack not empty");

    if (state.current_deletion_list != NULL) {
        rdpq_sync_full((void(*)(void*))gl_on_frame_complete, (void*)state.frame_id);
    }

    gl_handle_deletion_lists();
}

GLenum glGetError(void)
{
    GLenum error = state.current_error;
    state.current_error = GL_NO_ERROR;
    return error;
}

void gl_set_flag2(GLenum target, bool value)
{
    switch (target) {
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
        gl_set_flag(GL_UPDATE_NONE, FLAG_MULTISAMPLE, value);
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
        set_can_use_rsp_dirty();
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
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glEnable(GLenum target)
{
    gl_set_flag2(target, true);
}

void glDisable(GLenum target)
{
    gl_set_flag2(target, false);
}

void gl_copy_fill_color(uint32_t offset)
{
    __rdpq_autosync_change(AUTOSYNC_PIPE);
    gl_write(GL_CMD_COPY_FILL_COLOR, offset);
}

void glClear(GLbitfield buf)
{
    if (!buf) {
        return;
    }

    rdpq_mode_push();

    // Set fill mode
    extern void __rdpq_reset_render_mode(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3);
    uint64_t som = (0xEFull << 56) | SOM_CYCLE_FILL;
    __rdpq_reset_render_mode(0, 0, som >> 32, som & 0xFFFFFFFF);

    if (buf & (GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT)) {
        assertf(0, "Only color and depth buffers are supported!");
    }

    uint32_t width = state.color_buffer->width;
    uint32_t height = state.color_buffer->height;

    if (buf & GL_DEPTH_BUFFER_BIT) {
        uint32_t old_cfg = rdpq_config_disable(RDPQ_CFG_AUTOSCISSOR);

        // TODO: Clearing will be implemented by rdpq at some point

        gl_copy_fill_color(offsetof(gl_server_state_t, clear_depth));
        rdpq_set_color_image(&state.depth_buffer);
        rdpq_fill_rectangle(0, 0, width, height);

        rdpq_set_color_image(state.color_buffer);

        rdpq_config_set(old_cfg);
    }

    if (buf & GL_COLOR_BUFFER_BIT) {
        gl_copy_fill_color(offsetof(gl_server_state_t, clear_color));
        rdpq_fill_rectangle(0, 0, width, height);
    }

    rdpq_mode_pop();
}

void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a)
{
    color_t clear_color = RGBA32(CLAMPF_TO_U8(r), CLAMPF_TO_U8(g), CLAMPF_TO_U8(b), CLAMPF_TO_U8(a));
    gl_set_word(GL_UPDATE_NONE, offsetof(gl_server_state_t, clear_color), color_to_packed32(clear_color));
}

void glClearDepth(GLclampd d)
{
    color_t clear_depth = color_from_packed16(d * 0xFFFC);
    gl_set_word(GL_UPDATE_NONE, offsetof(gl_server_state_t, clear_depth), color_to_packed32(clear_depth));
}

void glFlush(void)
{
    rspq_flush();
}

void glFinish(void)
{
    rspq_wait();
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
        free_uncached(storage->data);
        storage->data = NULL;
    }
}

bool gl_storage_resize(gl_storage_t *storage, uint32_t new_size)
{
    if (storage->size >= new_size) {
        return true;
    }

    GLvoid *mem = malloc_uncached(new_size);
    if (mem == NULL) {
        return false;
    }

    gl_storage_free(storage);

    storage->data = mem;
    storage->size = new_size;

    return true;
}

extern inline bool is_in_heap_memory(void *ptr);
extern inline void gl_set_flag_raw(gl_update_func_t update_func, uint32_t offset, uint32_t flag, bool value);
extern inline void gl_set_flag(gl_update_func_t update_func, uint32_t flag, bool value);
extern inline void gl_set_byte(gl_update_func_t update_func, uint32_t offset, uint8_t value);
extern inline void gl_set_short(gl_update_func_t update_func, uint32_t offset, uint16_t value);
extern inline void gl_set_word(gl_update_func_t update_func, uint32_t offset, uint32_t value);
extern inline void gl_set_long(gl_update_func_t update_func, uint32_t offset, uint64_t value);
extern inline void gl_update(gl_update_func_t update_func);
extern inline void gl_get_value(void *dst, uint32_t offset, uint32_t size);
extern inline void gl_bind_texture(GLenum target, gl_texture_object_t *texture);
extern inline void gl_update_texture_completeness(uint32_t offset);
extern inline void glpipe_set_prim_vertex(int idx, GLfloat attribs[ATTRIB_COUNT][4], int id);
extern inline void glpipe_draw_triangle(bool has_tex, bool has_z, int i0, int i1, int i2);
extern inline void glpipe_vtx(GLfloat attribs[ATTRIB_COUNT][4], int id, uint8_t cmd, uint32_t cmd_size);