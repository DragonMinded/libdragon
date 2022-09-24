#include "GL/gl.h"
#include "rdpq.h"
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

uint32_t gl_overlay_id;

gl_state_t state;

#define assert_framebuffer() ({ \
    assertf(state.cur_framebuffer != NULL, "GL: No target is set!"); \
})

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

void gl_set_framebuffer(gl_framebuffer_t *framebuffer)
{
    state.cur_framebuffer = framebuffer;
    // TODO: disable auto scissor?
    rdpq_set_color_image(state.cur_framebuffer->color_buffer);
    rdpq_set_z_image_raw(0, PhysicalAddr(state.cur_framebuffer->depth_buffer));
}

void gl_set_default_framebuffer()
{
    surface_t *surf;

    RSP_WAIT_LOOP(200) {
        if ((surf = state.open_surface())) {
            break;
        }
    }

    gl_framebuffer_t *fb = &state.default_framebuffer;

    if (fb->depth_buffer != NULL && (fb->color_buffer == NULL 
                                    || fb->color_buffer->width != surf->width 
                                    || fb->color_buffer->height != surf->height)) {
        free_uncached(fb->depth_buffer);
        fb->depth_buffer = NULL;
    }

    fb->color_buffer = surf;

    // TODO: only allocate depth buffer if depth test is enabled? Lazily allocate?
    if (fb->depth_buffer == NULL) {
        // TODO: allocate in separate RDRAM bank?
        fb->depth_buffer = malloc_uncached_aligned(64, surf->width * surf->height * 2);
    }

    gl_set_framebuffer(fb);
}

void gl_init()
{
    gl_init_with_callbacks(display_lock, display_show);
}

void gl_init_with_callbacks(gl_open_surf_func_t open_surface, gl_close_surf_func_t close_surface)
{
    rdpq_init();

    memset(&state, 0, sizeof(state));

    state.open_surface = open_surface;
    state.close_surface = close_surface;

    gl_texture_init();

    gl_server_state_t *server_state = UncachedAddr(rspq_overlay_get_state(&rsp_gl));
    memset(server_state, 0, sizeof(gl_server_state_t));

    memcpy(&server_state->bound_textures, state.default_textures, sizeof(gl_texture_object_t) * 2);
    server_state->texture_ids[0] = PhysicalAddr(&state.default_textures[0]);
    server_state->texture_ids[1] = PhysicalAddr(&state.default_textures[1]);

    gl_overlay_id = rspq_overlay_register(&rsp_gl);

    rdpq_mode_begin();
    rdpq_set_mode_standard();

    gl_matrix_init();
    gl_lighting_init();
    gl_rendermode_init();
    gl_array_init();
    gl_primitive_init();
    gl_pixel_init();
    gl_list_init();

    glDrawBuffer(GL_FRONT);
    glDepthRange(0, 1);
    glClearDepth(1.0);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    gl_set_default_framebuffer();
    glViewport(0, 0, state.default_framebuffer.color_buffer->width, state.default_framebuffer.color_buffer->height);

    uint32_t packed_size = ((uint32_t)state.default_framebuffer.color_buffer->width) << 16 | (uint32_t)state.default_framebuffer.color_buffer->height;
    gl_set_word(GL_UPDATE_SCISSOR, offsetof(gl_server_state_t, fb_size), packed_size);

    glScissor(0, 0, state.default_framebuffer.color_buffer->width, state.default_framebuffer.color_buffer->height);
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
    
    gl_list_close();
    gl_primitive_close();
    gl_texture_close();
    rspq_overlay_unregister(gl_overlay_id);
    rdpq_close();
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

void gl_on_frame_complete(surface_t *surface)
{
    state.frames_complete++;
    state.close_surface(surface);
}

void gl_swap_buffers()
{
    rdpq_sync_full((void(*)(void*))gl_on_frame_complete, state.default_framebuffer.color_buffer);
    rspq_flush();
    gl_handle_deletion_lists();
    gl_set_default_framebuffer();

    state.frame_id++;
}

GLenum glGetError(void)
{
    GLenum error = state.current_error;
    state.current_error = GL_NO_ERROR;
    return error;
}

void gl_set_error(GLenum error)
{
    state.current_error = error;
    assert(error);
}

void gl_set_flag2(GLenum target, bool value)
{
    switch (target) {
    case GL_SCISSOR_TEST:
        gl_set_flag(GL_UPDATE_SCISSOR, FLAG_SCISSOR_TEST, value);
        break;
    case GL_DEPTH_TEST:
        gl_set_flag(GL_UPDATE_DEPTH_TEST, FLAG_DEPTH_TEST, value);
        gl_update(GL_UPDATE_DEPTH_MASK);
        state.depth_test = value;
        break;
    case GL_BLEND:
        gl_set_flag(GL_UPDATE_BLEND, FLAG_BLEND, value);
        gl_update(GL_UPDATE_BLEND_CYCLE);
        break;
    case GL_ALPHA_TEST:
        gl_set_flag(GL_UPDATE_ALPHA_TEST, FLAG_ALPHA_TEST, value);
        state.alpha_test = value;
        break;
    case GL_DITHER:
        gl_set_flag(GL_UPDATE_DITHER, FLAG_DITHER, value);
        break;
    case GL_FOG:
        gl_set_flag(GL_UPDATE_FOG_CYCLE, FLAG_FOG, value);
        state.fog = value;
        break;
    case GL_MULTISAMPLE_ARB:
        gl_set_flag(GL_UPDATE_NONE, FLAG_MULTISAMPLE, value);
        rdpq_mode_antialias(value);
        break;
    case GL_TEXTURE_1D:
        gl_set_flag(GL_UPDATE_TEXTURE, FLAG_TEXTURE_1D, value);
        state.texture_1d = value;
        break;
    case GL_TEXTURE_2D:
        gl_set_flag(GL_UPDATE_TEXTURE, FLAG_TEXTURE_2D, value);
        state.texture_2d = value;
        break;
    case GL_CULL_FACE:
        state.cull_face = value;
        break;
    case GL_LIGHTING:
        state.lighting = value;
        break;
    case GL_LIGHT0:
    case GL_LIGHT1:
    case GL_LIGHT2:
    case GL_LIGHT3:
    case GL_LIGHT4:
    case GL_LIGHT5:
    case GL_LIGHT6:
    case GL_LIGHT7:
        state.lights[target - GL_LIGHT0].enabled = value;
        break;
    case GL_COLOR_MATERIAL:
        state.color_material = value;
        break;
    case GL_TEXTURE_GEN_S:
        state.s_gen.enabled = value;
        break;
    case GL_TEXTURE_GEN_T:
        state.t_gen.enabled = value;
        break;
    case GL_TEXTURE_GEN_R:
        state.r_gen.enabled = value;
        break;
    case GL_TEXTURE_GEN_Q:
        state.q_gen.enabled = value;
        break;
    case GL_NORMALIZE:
        state.normalize = value;
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

void glDrawBuffer(GLenum buf)
{
    switch (buf) {
    case GL_NONE:
    case GL_FRONT_LEFT:
    case GL_FRONT:
    case GL_LEFT:
    case GL_FRONT_AND_BACK:
        state.draw_buffer = buf;
        break;
    case GL_FRONT_RIGHT:
    case GL_BACK_LEFT:
    case GL_BACK_RIGHT:
    case GL_BACK:
    case GL_RIGHT:
    case GL_AUX0:
    case GL_AUX1:
    case GL_AUX2:
    case GL_AUX3:
        gl_set_error(GL_INVALID_OPERATION);
        return;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glClear(GLbitfield buf)
{
    if (!buf) {
        return;
    }

    assert_framebuffer();

    rdpq_mode_push();

    rdpq_set_mode_fill(RGBA16(0,0,0,0));

    gl_framebuffer_t *fb = state.cur_framebuffer;

    if (buf & (GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT)) {
        assertf(0, "Only color and depth buffers are supported!");
    }

    rdpq_mode_end();

    if (buf & GL_DEPTH_BUFFER_BIT) {
        uint32_t old_cfg = rdpq_config_disable(RDPQ_CFG_AUTOSCISSOR);

        rdpq_set_color_image_raw(0, PhysicalAddr(fb->depth_buffer), FMT_RGBA16, fb->color_buffer->width, fb->color_buffer->height, fb->color_buffer->width * 2);
        rdpq_set_fill_color(color_from_packed16(state.clear_depth * 0xFFFC));
        rdpq_fill_rectangle(0, 0, fb->color_buffer->width, fb->color_buffer->height);

        rdpq_set_color_image(fb->color_buffer);

        rdpq_config_set(old_cfg);
    }

    if (buf & GL_COLOR_BUFFER_BIT) {
        rdpq_set_fill_color(RGBA32(
            CLAMPF_TO_U8(state.clear_color[0]), 
            CLAMPF_TO_U8(state.clear_color[1]), 
            CLAMPF_TO_U8(state.clear_color[2]), 
            CLAMPF_TO_U8(state.clear_color[3])));
        rdpq_fill_rectangle(0, 0, fb->color_buffer->width, fb->color_buffer->height);
    }

    rdpq_mode_begin();
    rdpq_mode_pop();
}

void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a)
{
    state.clear_color[0] = r;
    state.clear_color[1] = g;
    state.clear_color[2] = b;
    state.clear_color[3] = a;
}

void glClearDepth(GLclampd d)
{
    state.clear_depth = d;
}

void glRenderMode(GLenum mode)
{
    switch (mode) {
    case GL_RENDER:
        break;
    case GL_SELECT:
    case GL_FEEDBACK:
        assertf(0, "Select and feedback modes are not supported!");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
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