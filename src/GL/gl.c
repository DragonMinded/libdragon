/**
 * @file gl.c
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 * @brief OpenGL core initialization and state management.
 */
#include "GL/gl.h"
#include "gl_internal.h"
#include "magma.h"
#include "rdpq.h"
#include "rdpq_attach.h"

gl_state_t *state;

void gl_init(void)
{
    mg_init();
    rdpq_init();

    state = calloc(1, sizeof(gl_state_t));
    glClearColor(0, 0, 0, 0);
    glClearDepth(1.0);
}

void gl_close(void)
{
    rspq_wait();
    free(state);

    mg_close();
    rdpq_close();
}
void gl_context_begin()
{
}

void gl_context_end()
{
}

void glEnable(GLenum target)
{

}

void glDisable(GLenum target)
{

}

void glClear(GLbitfield buf)
{
    if (!gl_ensure_no_begin_end()) return;
    
    if (!buf) {
        return;
    }

    if (buf & (GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT)) {
        assertf(0, "Only color and depth buffers are supported!");
    }

    if (buf & GL_DEPTH_BUFFER_BIT) {
        rdpq_clear_z(state->clear_depth);
    }

    if (buf & GL_COLOR_BUFFER_BIT) {
        rdpq_clear(state->clear_color);
    }
}

void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a)
{
    if (!gl_ensure_no_begin_end()) return;
    state->clear_color = RGBA32(CLAMPF_TO_U8(r), CLAMPF_TO_U8(g), CLAMPF_TO_U8(b), CLAMPF_TO_U8(a));
}

void glClearDepth(GLclampd d)
{
    if (!gl_ensure_no_begin_end()) return;
    state->clear_depth = ZBUF_VAL(d);
}

static mg_primitive_topology_t get_primitive_topology(GLenum mode)
{
    switch (mode)
    {
    case GL_TRIANGLES:
        return MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case GL_TRIANGLE_STRIP:
        return MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case GL_TRIANGLE_FAN:
        return MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;

    case GL_POINTS:
    case GL_LINES:
    case GL_LINE_LOOP:
    case GL_LINE_STRIP:
    case GL_QUADS:
    case GL_QUAD_STRIP:
    case GL_POLYGON:
    default:
        assertf(0, "Draw mode %ld is not supported", mode);
    }
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    mg_draw_begin();
    mg_draw(&(mg_input_assembly_parms_t) {
        .primitive_topology = get_primitive_topology(mode)
    }, count, first);
    mg_draw_end();
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    assertf(type == GL_UNSIGNED_SHORT, "Index type must be GL_UNSIGNED_SHORT");

    mg_draw_begin();
    mg_draw_indexed(&(mg_input_assembly_parms_t) {
        .primitive_topology = get_primitive_topology(mode)
    }, indices, count, 0);
    mg_draw_end();
}

