#include "gl_internal.h"

extern gl_state_t state;

void glGetBooleanv(GLenum value, GLboolean *data)
{
    switch (value) {
    case GL_COLOR_CLEAR_VALUE:
        data[0] = CLAMPF_TO_BOOL(state.clear_color[0]);
        data[1] = CLAMPF_TO_BOOL(state.clear_color[1]);
        data[2] = CLAMPF_TO_BOOL(state.clear_color[2]);
        data[3] = CLAMPF_TO_BOOL(state.clear_color[3]);
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
        data[0] = CLAMPF_TO_I32(state.clear_color[0]);
        data[1] = CLAMPF_TO_I32(state.clear_color[1]);
        data[2] = CLAMPF_TO_I32(state.clear_color[2]);
        data[3] = CLAMPF_TO_I32(state.clear_color[3]);
        break;
    case GL_CURRENT_COLOR:
        data[0] = CLAMPF_TO_I32(state.current_attribs[ATTRIB_COLOR][0]);
        data[1] = CLAMPF_TO_I32(state.current_attribs[ATTRIB_COLOR][1]);
        data[2] = CLAMPF_TO_I32(state.current_attribs[ATTRIB_COLOR][2]);
        data[3] = CLAMPF_TO_I32(state.current_attribs[ATTRIB_COLOR][3]);
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
        data[0] = state.clear_color[0];
        data[1] = state.clear_color[1];
        data[2] = state.clear_color[2];
        data[3] = state.clear_color[3];
        break;
    case GL_CURRENT_COLOR:
        data[0] = state.current_attribs[ATTRIB_COLOR][0];
        data[1] = state.current_attribs[ATTRIB_COLOR][1];
        data[2] = state.current_attribs[ATTRIB_COLOR][2];
        data[3] = state.current_attribs[ATTRIB_COLOR][3];
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
        data[0] = state.clear_color[0];
        data[1] = state.clear_color[1];
        data[2] = state.clear_color[2];
        data[3] = state.clear_color[3];
        break;
    case GL_CURRENT_COLOR:
        data[0] = state.current_attribs[ATTRIB_COLOR][0];
        data[1] = state.current_attribs[ATTRIB_COLOR][1];
        data[2] = state.current_attribs[ATTRIB_COLOR][2];
        data[3] = state.current_attribs[ATTRIB_COLOR][3];
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
        return (GLubyte*)"GL_ARB_multisample GL_EXT_packed_pixels GL_ARB_vertex_buffer_object";
    default:
        gl_set_error(GL_INVALID_ENUM);
        return NULL;
    }
}
