#include "GL/gl.h"
#include "rdpq.h"
#include "rspq.h"
#include "display.h"
#include <string.h>

#define CLAMPF_TO_BOOL(x)  ((x)!=0.0)

#define CLAMPF_TO_U8(x)  ((x)*0xFF)
#define CLAMPF_TO_I8(x)  ((x)*0x7F)
#define CLAMPF_TO_U16(x) ((x)*0xFFFF)
#define CLAMPF_TO_I16(x) ((x)*0x7FFF)
#define CLAMPF_TO_U32(x) ((x)*0xFFFFFFFF)
#define CLAMPF_TO_I32(x) ((x)*0x7FFFFFFF)

typedef struct gl_framebuffer_s {
    surface_t *color_buffer;
    // TODO
    //void *depth_buffer;
} framebuffer_t;

static framebuffer_t default_framebuffer;
static framebuffer_t *cur_framebuffer;
static GLenum current_error;

static GLclampf clear_color[4];

#define assert_framebuffer() ({ \
    assertf(cur_framebuffer != NULL, "GL: No target is set!"); \
})

void gl_set_framebuffer(framebuffer_t *framebuffer)
{
    cur_framebuffer = framebuffer;
    rdpq_set_color_image_surface(cur_framebuffer->color_buffer);
}

void gl_set_default_framebuffer()
{
    display_context_t ctx;
    while (!(ctx = display_lock()));

    default_framebuffer.color_buffer = ctx;

    gl_set_framebuffer(&default_framebuffer);
}

void gl_init()
{
    rdpq_init();
    rdpq_set_other_modes(0);
    gl_set_default_framebuffer();
}

void gl_close()
{
    rdpq_close();
}

GLenum glGetError(void)
{
    GLenum error = current_error;
    current_error = GL_NO_ERROR;
    return error;
}

void gl_set_error(GLenum error)
{
    current_error = error;
}

void gl_swap_buffers()
{
    rdpq_sync_full((void(*)(void*))display_show, default_framebuffer.color_buffer);
    rspq_flush();
    gl_set_default_framebuffer();
}

void glScissor(GLint left, GLint bottom, GLsizei width, GLsizei height)
{
    rdpq_set_scissor(left, bottom, left + width, bottom + height);
}

void glClear(GLbitfield buf)
{
    assert_framebuffer();

    rdpq_set_cycle_mode(SOM_CYCLE_FILL);

    if (buf & GL_COLOR_BUFFER_BIT) {
        rdpq_set_fill_color(RGBA32(
            CLAMPF_TO_U8(clear_color[0]), 
            CLAMPF_TO_U8(clear_color[1]), 
            CLAMPF_TO_U8(clear_color[2]), 
            CLAMPF_TO_U8(clear_color[3])));
        rdpq_fill_rectangle(0, 0, cur_framebuffer->color_buffer->width, cur_framebuffer->color_buffer->height);
    }
}

void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a)
{
    clear_color[0] = r;
    clear_color[1] = g;
    clear_color[2] = b;
    clear_color[3] = a;
}

void glFlush(void)
{
    rspq_flush();
}

void glFinish(void)
{
    rspq_wait();
}

void glGetBooleanv(GLenum value, GLboolean *data)
{
    switch (value) {
    case GL_COLOR_CLEAR_VALUE:
        data[0] = CLAMPF_TO_BOOL(clear_color[0]);
        data[1] = CLAMPF_TO_BOOL(clear_color[1]);
        data[2] = CLAMPF_TO_BOOL(clear_color[2]);
        data[3] = CLAMPF_TO_BOOL(clear_color[3]);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        break;
    }
}

void glGetIntegerv(GLenum value, GLint *data)
{
    switch (value) {
    case GL_COLOR_CLEAR_VALUE:
        data[0] = CLAMPF_TO_I32(clear_color[0]);
        data[1] = CLAMPF_TO_I32(clear_color[1]);
        data[2] = CLAMPF_TO_I32(clear_color[2]);
        data[3] = CLAMPF_TO_I32(clear_color[3]);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        break;
    }
}

void glGetFloatv(GLenum value, GLfloat *data)
{
    switch (value) {
    case GL_COLOR_CLEAR_VALUE:
        data[0] = clear_color[0];
        data[1] = clear_color[1];
        data[2] = clear_color[2];
        data[3] = clear_color[3];
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        break;
    }
}

void glGetDoublev(GLenum value, GLdouble *data)
{
    switch (value) {
    case GL_COLOR_CLEAR_VALUE:
        data[0] = clear_color[0];
        data[1] = clear_color[1];
        data[2] = clear_color[2];
        data[3] = clear_color[3];
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        break;
    }
}

GLubyte *glGetString(GLenum name)
{
    switch (name) {
    case GL_VENDOR:
        return (GLubyte*)"Libdragon";
    case GL_RENDERER:
        return (GLubyte*)"N64";
    case GL_VERSION:
        return (GLubyte*)"1.1";
    case GL_EXTENSIONS:
        return (GLubyte*)"";
    default:
        gl_set_error(GL_INVALID_ENUM);
        return NULL;
    }
}
