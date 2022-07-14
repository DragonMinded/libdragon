#include "gl_internal.h"
#include "debug.h"

extern gl_state_t state;

typedef struct {
    void (*cb_byte[4]) (const GLbyte*);
    void (*cb_ubyte[4]) (const GLubyte*);
    void (*cb_short[4]) (const GLshort*);
    void (*cb_ushort[4]) (const GLushort*);
    void (*cb_int[4]) (const GLint*);
    void (*cb_uint[4]) (const GLuint*);
    void (*cb_float[4]) (const GLfloat*);
    void (*cb_double[4]) (const GLdouble*);
} gl_attr_callback_t;

typedef void (*gl_attr_callback_func_t)(const GLvoid*);

typedef struct {
    GLboolean et, ec, en;
    GLint st, sc, sv;
    GLenum tc;
    GLuint pc, pn, pv;
    GLsizei s;
} gl_interleaved_array_t;

static const gl_attr_callback_t vertex_callback = {
    .cb_short  = { NULL, glVertex2sv, glVertex3sv, glVertex4sv },
    .cb_int    = { NULL, glVertex2iv, glVertex3iv, glVertex4iv },
    .cb_float  = { NULL, glVertex2fv, glVertex3fv, glVertex4fv },
    .cb_double = { NULL, glVertex2dv, glVertex3dv, glVertex4dv },
};

static const gl_attr_callback_t texcoord_callback = {
    .cb_short  = { glTexCoord1sv, glTexCoord2sv, glTexCoord3sv, glTexCoord4sv },
    .cb_int    = { glTexCoord1iv, glTexCoord2iv, glTexCoord3iv, glTexCoord4iv },
    .cb_float  = { glTexCoord1fv, glTexCoord2fv, glTexCoord3fv, glTexCoord4fv },
    .cb_double = { glTexCoord1dv, glTexCoord2dv, glTexCoord3dv, glTexCoord4dv },
};

static const gl_attr_callback_t normal_callback = {
    .cb_byte   = { NULL, NULL, glNormal3bv, NULL },
    .cb_short  = { NULL, NULL, glNormal3sv, NULL },
    .cb_int    = { NULL, NULL, glNormal3iv, NULL },
    .cb_float  = { NULL, NULL, glNormal3fv, NULL },
    .cb_double = { NULL, NULL, glNormal3dv, NULL },
};

static const gl_attr_callback_t color_callback = {
    .cb_byte   = { NULL, NULL, glColor3bv,  glColor4bv },
    .cb_ubyte  = { NULL, NULL, glColor3ubv, glColor4ubv },
    .cb_short  = { NULL, NULL, glColor3sv,  glColor4sv },
    .cb_ushort = { NULL, NULL, glColor3usv, glColor4usv },
    .cb_int    = { NULL, NULL, glColor3iv,  glColor4iv },
    .cb_uint   = { NULL, NULL, glColor3uiv, glColor4uiv },
    .cb_float  = { NULL, NULL, glColor3fv,  glColor4fv },
    .cb_double = { NULL, NULL, glColor3dv,  glColor4dv },
};

#define ILA_F (sizeof(GLfloat))
#define ILA_C (sizeof(GLubyte) * 4)

static const gl_interleaved_array_t interleaved_arrays[] = {
    /* GL_V2F */             { .et = false, .ec = false, .en = false,                   .sv = 2,                                                       .pv = 0,               .s = 2*ILA_F },
    /* GL_V3F */             { .et = false, .ec = false, .en = false,                   .sv = 3,                                                       .pv = 0,               .s = 3*ILA_F },
    /* GL_C4UB_V2F */        { .et = false, .ec = true,  .en = false,          .sc = 4, .sv = 2, .tc = GL_UNSIGNED_BYTE, .pc = 0,                      .pv = ILA_C,           .s = ILA_C + 2*ILA_F },
    /* GL_C4UB_V3F */        { .et = false, .ec = true,  .en = false,          .sc = 4, .sv = 3, .tc = GL_UNSIGNED_BYTE, .pc = 0,                      .pv = ILA_C,           .s = ILA_C + 3*ILA_F },
    /* GL_C3F_V3F */         { .et = false, .ec = true,  .en = false,          .sc = 3, .sv = 3, .tc = GL_FLOAT,         .pc = 0,                      .pv = 3*ILA_F,         .s = 6*ILA_F },
    /* GL_N3F_V3F */         { .et = false, .ec = false, .en = true,                    .sv = 3,                                        .pn = 0,       .pv = 3*ILA_F,         .s = 6*ILA_F },
    /* GL_C4F_N3F_V3F */     { .et = false, .ec = true,  .en = true,           .sc = 4, .sv = 3, .tc = GL_FLOAT,         .pc = 0,       .pn = 4*ILA_F, .pv = 7*ILA_F,         .s = 10*ILA_F },
    /* GL_T2F_V3F */         { .et = true,  .ec = false, .en = false, .st = 2,          .sv = 3,                                                       .pv = 2*ILA_F,         .s = 5*ILA_F },
    /* GL_T4F_V4F */         { .et = true,  .ec = false, .en = false, .st = 4,          .sv = 4,                                                       .pv = 4*ILA_F,         .s = 8*ILA_F },
    /* GL_T2F_C4UB_V3F */    { .et = true,  .ec = true,  .en = false, .st = 2, .sc = 4, .sv = 3, .tc = GL_UNSIGNED_BYTE, .pc = 2*ILA_F,                .pv = ILA_C + 2*ILA_F, .s = ILA_C + 5*ILA_F },
    /* GL_T2F_C3F_V3F */     { .et = true,  .ec = true,  .en = false, .st = 2, .sc = 3, .sv = 3, .tc = GL_FLOAT,         .pc = 2*ILA_F,                .pv = 5*ILA_F,         .s = 8*ILA_F },
    /* GL_T2F_N3F_V3F */     { .et = true,  .ec = false, .en = true,  .st = 2,          .sv = 3,                                        .pn = 2*ILA_F, .pv = 5*ILA_F,         .s = 8*ILA_F },
    /* GL_T2F_C4F_N3F_V3F */ { .et = true,  .ec = true,  .en = true,  .st = 2, .sc = 4, .sv = 3, .tc = GL_FLOAT,         .pc = 2*ILA_F, .pn = 6*ILA_F, .pv = 9*ILA_F,         .s = 12*ILA_F },
    /* GL_T4F_C4F_N3F_V4F */ { .et = true,  .ec = true,  .en = true,  .st = 4, .sc = 4, .sv = 4, .tc = GL_FLOAT,         .pc = 4*ILA_F, .pn = 8*ILA_F, .pv = 11*ILA_F,        .s = 15*ILA_F },
};

void gl_array_init()
{
    state.vertex_array.size = 4;
    state.vertex_array.type = GL_FLOAT;
    state.texcoord_array.size = 4;
    state.texcoord_array.type = GL_FLOAT;
    state.normal_array.size = 3;
    state.normal_array.type = GL_FLOAT;
    state.color_array.size = 4;
    state.color_array.type = GL_FLOAT;
}

gl_array_t * gl_get_array(GLenum array)
{
    switch (array) {
    case GL_VERTEX_ARRAY:
        return &state.vertex_array;
    case GL_TEXTURE_COORD_ARRAY:
        return &state.texcoord_array;
    case GL_NORMAL_ARRAY:
        return &state.normal_array;
    case GL_COLOR_ARRAY:
        return &state.color_array;
    case GL_EDGE_FLAG_ARRAY:
    case GL_INDEX_ARRAY:
        return NULL;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return NULL;
    }
}

void gl_set_array(gl_array_t *array, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    if (stride < 0) {
        gl_set_error(GL_INVALID_VALUE);
        return;
    }

    array->size = size;
    array->type = type;
    array->stride = stride;
    array->pointer = pointer;
}

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    switch (size) {
    case 2:
    case 3:
    case 4:
        break;
    default:
        gl_set_error(GL_INVALID_VALUE);
        return;
    }

    switch (type) {
    case GL_SHORT:
    case GL_INT:
    case GL_FLOAT:
    case GL_DOUBLE:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    gl_set_array(&state.vertex_array, size, type, stride, pointer);
}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    switch (size) {
    case 1:
    case 2:
    case 3:
    case 4:
        break;
    default:
        gl_set_error(GL_INVALID_VALUE);
        return;
    }

    switch (type) {
    case GL_SHORT:
    case GL_INT:
    case GL_FLOAT:
    case GL_DOUBLE:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    gl_set_array(&state.texcoord_array, size, type, stride, pointer);
}

void glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
    switch (type) {
    case GL_BYTE:
    case GL_SHORT:
    case GL_INT:
    case GL_FLOAT:
    case GL_DOUBLE:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    gl_set_array(&state.normal_array, 3, type, stride, pointer);
}

void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    switch (size) {
    case 3:
    case 4:
        break;
    default:
        gl_set_error(GL_INVALID_VALUE);
        return;
    }

    switch (type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_FLOAT:
    case GL_DOUBLE:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    gl_set_array(&state.color_array, size, type, stride, pointer);
}

void glEdgeFlagPointer(GLsizei stride, const GLvoid *pointer) { }
void glIndexPointer(GLenum type, GLsizei stride, const GLvoid *pointer) { }

void glEnableClientState(GLenum array)
{
    gl_array_t *array_obj = gl_get_array(array);
    if (array_obj == NULL) {
        return;
    }

    array_obj->enabled = true;
}
void glDisableClientState(GLenum array)
{
    gl_array_t *array_obj = gl_get_array(array);
    if (array_obj == NULL) {
        return;
    }

    array_obj->enabled = false;
}

gl_attr_callback_func_t * gl_get_type_array_callback(const gl_attr_callback_t *callback, GLenum type)
{
    switch (type) {
    case GL_BYTE:
        return (gl_attr_callback_func_t*)callback->cb_byte;
    case GL_UNSIGNED_BYTE:
        return (gl_attr_callback_func_t*)callback->cb_ubyte;
    case GL_SHORT:
        return (gl_attr_callback_func_t*)callback->cb_short;
    case GL_UNSIGNED_SHORT:
        return (gl_attr_callback_func_t*)callback->cb_ushort;
    case GL_INT:
        return (gl_attr_callback_func_t*)callback->cb_int;
    case GL_UNSIGNED_INT:
        return (gl_attr_callback_func_t*)callback->cb_uint;
    case GL_FLOAT:
        return (gl_attr_callback_func_t*)callback->cb_float;
    case GL_DOUBLE:
        return (gl_attr_callback_func_t*)callback->cb_double;
    default:
        return NULL;
    }
}

void gl_invoke_attr_callback(GLint i, const gl_array_t *array, const gl_attr_callback_t *callback)
{
    uint32_t stride = array->stride == 0 ? array->size * gl_get_type_size(array->type) : array->stride;
    const GLvoid *data = array->pointer + stride * i;

    gl_attr_callback_func_t *funcs = gl_get_type_array_callback(callback, array->type);
    assertf(funcs != NULL, "Illegal attribute type");

    gl_attr_callback_func_t func = funcs[array->size - 1];
    assertf(func != NULL, "Illegal attribute size");

    func(data);
}

void glArrayElement(GLint i)
{
    if (state.texcoord_array.enabled) {
        gl_invoke_attr_callback(i, &state.texcoord_array, &texcoord_callback);
    }
    if (state.normal_array.enabled) {
        gl_invoke_attr_callback(i, &state.normal_array, &normal_callback);
    }
    if (state.color_array.enabled) {
        gl_invoke_attr_callback(i, &state.color_array, &color_callback);
    }
    if (state.vertex_array.enabled) {
        gl_invoke_attr_callback(i, &state.vertex_array, &vertex_callback);
    }
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    switch (mode) {
    case GL_TRIANGLES:
    case GL_TRIANGLE_STRIP:
    case GL_QUAD_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_QUADS:
    case GL_POLYGON:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    glBegin(mode);

    for (GLint i = 0; i < count; i++) glArrayElement(i + first);

    glEnd();
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    switch (mode) {
    case GL_TRIANGLES:
    case GL_TRIANGLE_STRIP:
    case GL_QUAD_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_QUADS:
    case GL_POLYGON:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    switch (type) {
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_SHORT:
    case GL_UNSIGNED_INT:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    glBegin(mode);

    switch (type) {
    case GL_UNSIGNED_BYTE:
        for (GLint i = 0; i < count; i++) glArrayElement(((const GLubyte*)indices)[i]);
        break;
    case GL_UNSIGNED_SHORT:
        for (GLint i = 0; i < count; i++) glArrayElement(((const GLushort*)indices)[i]);
        break;
    case GL_UNSIGNED_INT:
        for (GLint i = 0; i < count; i++) glArrayElement(((const GLuint*)indices)[i]);
        break;
    }

    glEnd();
}

void glInterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer)
{
    switch (format) {
    case GL_V2F:
    case GL_V3F:
    case GL_C4UB_V2F:
    case GL_C4UB_V3F:
    case GL_C3F_V3F:
    case GL_N3F_V3F:
    case GL_C4F_N3F_V3F:
    case GL_T2F_V3F:
    case GL_T4F_V4F:
    case GL_T2F_C4UB_V3F:
    case GL_T2F_C3F_V3F:
    case GL_T2F_N3F_V3F:
    case GL_T2F_C4F_N3F_V3F:
    case GL_T4F_C4F_N3F_V4F:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    const gl_interleaved_array_t *a = &interleaved_arrays[format - GL_V2F];

    if (stride == 0) {
        stride = a->s;
    }

    if (a->et) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(a->st, GL_FLOAT, stride, pointer);
    } else {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    if (a->ec) {
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(a->sc, a->tc, stride, pointer + a->pc);
    } else {
        glDisableClientState(GL_COLOR_ARRAY);
    }

    if (a->en) {
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(GL_FLOAT, stride, pointer + a->pn);
    } else {
        glDisableClientState(GL_NORMAL_ARRAY);
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(a->sv, GL_FLOAT, stride, pointer + a->pv);
}
