#include "GL/gl.h"
#include "rdpq.h"
#include "rspq.h"
#include "display.h"
#include "rdp.h"
#include "utils.h"
#include "gl_internal.h"
#include <string.h>
#include <math.h>

gl_state_t state;

#define assert_framebuffer() ({ \
    assertf(state.cur_framebuffer != NULL, "GL: No target is set!"); \
})

void gl_set_framebuffer(gl_framebuffer_t *framebuffer)
{
    state.cur_framebuffer = framebuffer;
    glViewport(0, 0, framebuffer->color_buffer->width, framebuffer->color_buffer->height);
    rdpq_set_color_image_surface_no_scissor(state.cur_framebuffer->color_buffer);
    rdpq_set_z_image(state.cur_framebuffer->depth_buffer);
}

void gl_set_default_framebuffer()
{
    surface_t *ctx;

    RSP_WAIT_LOOP(200) {
        if ((ctx = display_lock())) {
            break;
        }
    }

    gl_framebuffer_t *fb = &state.default_framebuffer;

    if (fb->depth_buffer != NULL && (fb->color_buffer == NULL 
                                    || fb->color_buffer->width != ctx->width 
                                    || fb->color_buffer->height != ctx->height)) {
        free_uncached(fb->depth_buffer);
        fb->depth_buffer = NULL;
    }

    fb->color_buffer = ctx;

    // TODO: only allocate depth buffer if depth test is enabled? Lazily allocate?
    if (fb->depth_buffer == NULL) {
        // TODO: allocate in separate RDRAM bank?
        fb->depth_buffer = malloc_uncached_aligned(64, ctx->width * ctx->height * 2);
    }

    gl_set_framebuffer(fb);
}

void gl_init()
{
    rdpq_init();

    memset(&state, 0, sizeof(state));

    gl_matrix_init();
    gl_lighting_init();
    gl_texture_init();
    gl_rendermode_init();

    glDrawBuffer(GL_FRONT);
    glDepthRange(0, 1);
    glClearDepth(1.0);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    gl_set_default_framebuffer();
}

void gl_close()
{
    rdpq_close();
}

void gl_swap_buffers()
{
    rdpq_sync_full((void(*)(void*))display_show, state.default_framebuffer.color_buffer);
    rspq_flush();
    gl_set_default_framebuffer();
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

void gl_set_flag(GLenum target, bool value)
{
    switch (target) {
    case GL_SCISSOR_TEST:
        state.is_scissor_dirty = value != state.scissor_test;
        state.scissor_test = value;
        break;
    case GL_CULL_FACE:
        state.cull_face = value;
        break;
    case GL_DEPTH_TEST:
        GL_SET_STATE(state.depth_test, value, state.is_rendermode_dirty);
        break;
    case GL_TEXTURE_2D:
        GL_SET_STATE(state.texture_2d, value, state.is_rendermode_dirty);
        break;
    case GL_BLEND:
        GL_SET_STATE(state.blend, value, state.is_rendermode_dirty);
        break;
    case GL_ALPHA_TEST:
        GL_SET_STATE(state.alpha_test, value, state.is_rendermode_dirty);
        break;
    case GL_DITHER:
        GL_SET_STATE(state.dither, value, state.is_rendermode_dirty);
        break;
    case GL_FOG:
        GL_SET_STATE(state.fog, value, state.is_rendermode_dirty);
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
    case GL_COLOR_LOGIC_OP:
    case GL_INDEX_LOGIC_OP:
        assertf(!value, "Logical pixel operation is not supported!");
        break;
    case GL_LINE_STIPPLE:
    case GL_POLYGON_STIPPLE:
        assertf(!value, "Stipple is not supported!");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glEnable(GLenum target)
{
    gl_set_flag(target, true);
}

void glDisable(GLenum target)
{
    gl_set_flag(target, false);
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

void glIndexMask(GLuint mask)
{
    assertf(0, "Masking is not supported!");
}
void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
    assertf(0, "Masking is not supported!");
}
void glDepthMask(GLboolean mask)
{
    assertf(0, "Masking is not supported!");
}
void glStencilMask(GLuint mask)
{
    assertf(0, "Masking is not supported!");
}

void glClear(GLbitfield buf)
{
    assert_framebuffer();

    rdpq_set_other_modes(SOM_CYCLE_FILL);
    state.is_rendermode_dirty = true;

    gl_update_scissor();

    gl_framebuffer_t *fb = state.cur_framebuffer;

    if (buf & (GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT)) {
        assertf(0, "Only color and depth buffers are supported!");
    }

    if (buf & GL_DEPTH_BUFFER_BIT) {
        rdpq_set_color_image_no_scissor(fb->depth_buffer, FMT_RGBA16, fb->color_buffer->width, fb->color_buffer->height, fb->color_buffer->width * 2);
        rdpq_set_fill_color(color_from_packed16(state.clear_depth * 0xFFFC));
        rdpq_fill_rectangle(0, 0, fb->color_buffer->width, fb->color_buffer->height);

        rdpq_set_color_image_surface_no_scissor(fb->color_buffer);
    }

    if (buf & GL_COLOR_BUFFER_BIT) {
        rdpq_set_fill_color(RGBA32(
            CLAMPF_TO_U8(state.clear_color[0]), 
            CLAMPF_TO_U8(state.clear_color[1]), 
            CLAMPF_TO_U8(state.clear_color[2]), 
            CLAMPF_TO_U8(state.clear_color[3])));
        rdpq_fill_rectangle(0, 0, fb->color_buffer->width, fb->color_buffer->height);
    }
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

void glClearIndex(GLfloat index)
{
    // TODO: Can we support index mode?
    assertf(0, "Clear index is not supported!");
}

void glClearStencil(GLint s)
{
    assertf(0, "Clear stencil is not supported!");
}

void glClearAccum(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    assertf(0, "Clear accum is not supported!");
}

void glAccum(GLenum op, GLfloat value)
{
    assertf(0, "Accumulation buffer is not supported!");
}

void glInitNames(void)
{
    assertf(0, "Selection mode is not supported!");
}
void glPopName(void)
{
    assertf(0, "Selection mode is not supported!");
}
void glPushName(GLint name)
{
    assertf(0, "Selection mode is not supported!");
}
void glLoadName(GLint name)
{
    assertf(0, "Selection mode is not supported!");
}
void glSelectBuffer(GLsizei n, GLuint *buffer)
{
    assertf(0, "Selection mode is not supported!");
}

void glFeedbackBuffer(GLsizei n, GLenum type, GLfloat *buffer)
{
    assertf(0, "Feedback mode is not supported!");
}
void glPassThrough(GLfloat token)
{
    assertf(0, "Feedback mode is not supported!");
}

void glPushAttrib(GLbitfield mask)
{
    assertf(0, "Attribute stack is not supported!");
}
void glPushClientAttrib(GLbitfield mask)
{
    assertf(0, "Attribute stack is not supported!");
}
void glPopAttrib(void)
{
    assertf(0, "Attribute stack is not supported!");
}
void glPopClientAttrib(void)
{
    assertf(0, "Attribute stack is not supported!");
}
