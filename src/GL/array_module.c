/**
 * @file array_module.c
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 * @brief OpenGL array and attribute pointer management for vertex data.
 */
#include "gl_internal.h"
#include "debug.h"
#include "array_object.h"
#include "draw_call_cache.h"
#include "buffer.h"
#include <malloc.h>

extern gl_state_t *state;

typedef struct {
    GLboolean et, ec, en;
    GLint st, sc, sv;
    GLenum tc;
    GLuint pc, pn, pv;
    GLsizei s;
} gl_interleaved_array_t;

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

static array_type_t gl_array_type_from_enum(GLenum array)
{
    switch (array) {
    case GL_VERTEX_ARRAY:
        return ARRAY_VERTEX;
    case GL_NORMAL_ARRAY:
        return ARRAY_NORMAL;
    case GL_COLOR_ARRAY:
        return ARRAY_COLOR;
    case GL_TEXTURE_COORD_ARRAY:
        return ARRAY_TEXCOORD;
    case GL_MATRIX_INDEX_ARRAY_ARB:
        return ARRAY_MTX_INDEX;
    default:
        assertf(0, "Invalid array type!");
    }
}

void array_module_init()
{
    array_object_init(&state->default_array_object);
    glBindVertexArray(0);
}

void array_module_close()
{
    glBindVertexArray(0);
    array_object_destroy(&state->default_array_object);
}

static void assert_valid_array(GLuint array)
{
    assertf(array == 0 || is_valid_object_id(array),
            "Not a valid array object: %#lx. Make sure to allocate IDs via glGenVertexArray", array);
}

static void set_array_parms(array_type_t array_type, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    array_object_set_array_params(state->array_object, array_type, size, type, stride, pointer);
}

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    if (!gl_ensure_no_begin_end()) return;

    switch (size) {
    case 2:
    case 3:
    case 4:
        break;
    default:
        gl_set_error(GL_INVALID_VALUE, "Size must be 2, 3 or 4");
        return;
    }

    switch (type) {
    case GL_SHORT:
    case GL_INT:
    case GL_FLOAT:
    case GL_DOUBLE:
    case GL_HALF_FIXED_N64:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid vertex data type", type);
        return;
    }

    set_array_parms(ARRAY_VERTEX, size, type, stride, pointer);
}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    if (!gl_ensure_no_begin_end()) return;

    switch (size) {
    case 1:
    case 2:
    case 3:
    case 4:
        break;
    default:
        gl_set_error(GL_INVALID_VALUE, "Size must be 1, 2, 3 or 4");
        return;
    }

    switch (type) {
    case GL_SHORT:
    case GL_INT:
    case GL_FLOAT:
    case GL_DOUBLE:
    case GL_HALF_FIXED_N64:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid texture coordinate data type", type);
        return;
    }

    set_array_parms(ARRAY_TEXCOORD, size, type, stride, pointer);
}

void glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
    if (!gl_ensure_no_begin_end()) return;

    switch (type) {
    case GL_BYTE:
    case GL_SHORT:
    case GL_INT:
    case GL_FLOAT:
    case GL_DOUBLE:
    case GL_SHORT_5_6_5_N64:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid normal data type", type);
        return;
    }

    set_array_parms(ARRAY_NORMAL, 3, type, stride, pointer);
}

void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    if (!gl_ensure_no_begin_end()) return;

    switch (size) {
    case 3:
    case 4:
        break;
    default:
        gl_set_error(GL_INVALID_VALUE, "Size must be 3 or 4");
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
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid color data type", type);
        return;
    }

    set_array_parms(ARRAY_COLOR, size, type, stride, pointer);
}

void glMatrixIndexPointerARB(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    if (!gl_ensure_no_begin_end()) return;

    if (size < 1 || size > VERTEX_UNIT_COUNT) {
        gl_set_error(GL_INVALID_VALUE, "Size must be 1");
        return;
    }

    switch (type) {
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_SHORT:
    case GL_UNSIGNED_INT:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid matrix index data type", type);
        return;
    }

    set_array_parms(ARRAY_MTX_INDEX, size, type, stride, pointer);
}

static void set_array_enabled(array_type_t array_type, bool enabled)
{
    array_object_set_array_enabled(state->array_object, array_type, enabled);
}

void glEnableClientState(GLenum array)
{
    if (!gl_ensure_no_begin_end()) return;

    switch (array) {
    case GL_VERTEX_ARRAY:
    case GL_TEXTURE_COORD_ARRAY:
    case GL_NORMAL_ARRAY:
    case GL_COLOR_ARRAY:
    case GL_MATRIX_INDEX_ARRAY_ARB:
        set_array_enabled(gl_array_type_from_enum(array), true);
        break;
    case GL_EDGE_FLAG_ARRAY:
    case GL_INDEX_ARRAY:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid client state", array);
        break;
    }
}
void glDisableClientState(GLenum array)
{
    if (!gl_ensure_no_begin_end()) return;

    switch (array) {
    case GL_VERTEX_ARRAY:
    case GL_TEXTURE_COORD_ARRAY:
    case GL_NORMAL_ARRAY:
    case GL_COLOR_ARRAY:
    case GL_MATRIX_INDEX_ARRAY_ARB:
        set_array_enabled(gl_array_type_from_enum(array), false);
        break;
    case GL_EDGE_FLAG_ARRAY:
    case GL_INDEX_ARRAY:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid client state", array);
        break;
    }
}

void glInterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer)
{
    if (!gl_ensure_no_begin_end()) return;

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
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid interleaved array format", format);
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

void glGenVertexArrays(GLsizei n, GLuint *arrays)
{
    if (!gl_ensure_no_begin_end()) return;

    for (GLsizei i = 0; i < n; i++)
    {
        gl_array_object_t *new_obj = calloc(1, sizeof(gl_array_object_t));
        assertf(new_obj, "Out of memory");
        array_object_init(new_obj);
        arrays[i] = (GLuint)new_obj;
    }
}

void glDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
    if (!gl_ensure_no_begin_end()) return;

    for (GLsizei i = 0; i < n; i++)
    {
        assert_valid_array(arrays[i]);

        gl_array_object_t *obj = (gl_array_object_t*)arrays[i];
        if (obj == NULL) {
            continue;
        }

        if (obj == state->array_object) {
            glBindVertexArray(0);
        }

        array_object_destroy(obj);
        free(obj);
    }
}

void glBindVertexArray(GLuint array)
{
    if (!gl_ensure_no_begin_end()) return;
    assert_valid_array(array);

    gl_array_object_t *obj = (gl_array_object_t*)array;

    if (obj == NULL) {
        obj = &state->default_array_object;
    }

    state->array_object = obj;
}

GLboolean glIsVertexArray(GLuint array)
{
    if (!gl_ensure_no_begin_end()) return 0;
    
    // FIXME: This doesn't actually guarantee that it's a valid array object, but just uses the heuristic of
    //        "is it somewhere in the heap memory?". This way we can at least rule out arbitrarily chosen integer constants,
    //        which used to be valid array IDs in legacy OpenGL.
    return is_valid_object_id(array);
}
