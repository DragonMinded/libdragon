#include "gl_internal.h"
#include "debug.h"
#include <malloc.h>

extern gl_state_t state;

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

extern const cpu_read_attrib_func cpu_read_funcs[ATTRIB_COUNT][ATTRIB_TYPE_COUNT];
extern const rsp_read_attrib_func rsp_read_funcs[ATTRIB_COUNT][ATTRIB_TYPE_COUNT];

gl_array_type_t gl_array_type_from_enum(GLenum array)
{
    switch (array) {
    case GL_VERTEX_ARRAY:
        return ATTRIB_VERTEX;
    case GL_TEXTURE_COORD_ARRAY:
        return ATTRIB_TEXCOORD;
    case GL_NORMAL_ARRAY:
        return ATTRIB_NORMAL;
    case GL_COLOR_ARRAY:
        return ATTRIB_COLOR;
    case GL_MATRIX_INDEX_ARRAY_ARB:
        return ATTRIB_MTX_INDEX;
    default:
        return -1;
    }
}

void gl_update_array(gl_array_t *array, gl_array_type_t array_type)
{
    uint32_t size_shift = 0;
    
    switch (array->type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
        size_shift = 0;
        break;
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
    case GL_HALF_FIXED_N64:
        size_shift = 1;
        break;
    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_FLOAT:
        size_shift = 2;
        break;
    case GL_DOUBLE:
        size_shift = 3;
        break;
    }

    array->final_stride = array->stride == 0 ? array->size << size_shift : array->stride;

    uint32_t func_index = gl_type_to_index(array->type);
    array->cpu_read_func = cpu_read_funcs[array_type][func_index];
    array->rsp_read_func = rsp_read_funcs[array_type][func_index];

    assertf(array->cpu_read_func != NULL, "CPU read function is missing");
    assertf(array->rsp_read_func != NULL, "RSP read function is missing");
}

void gl_update_array_pointer(gl_array_t *array)
{
    if (array->binding != NULL) {
        array->final_pointer = array->binding->storage.data + (uint32_t)array->pointer;
    } else {
        array->final_pointer = array->pointer;
    }
}

void gl_update_array_pointers(gl_array_object_t *obj)
{
    for (uint32_t i = 0; i < ATTRIB_COUNT; i++)
    {
        gl_update_array_pointer(&obj->arrays[i]);
    }
}

void gl_array_object_init(gl_array_object_t *obj)
{
    obj->arrays[ATTRIB_VERTEX].size = 4;
    obj->arrays[ATTRIB_VERTEX].type = GL_FLOAT;
    obj->arrays[ATTRIB_COLOR].size = 4;
    obj->arrays[ATTRIB_COLOR].type = GL_FLOAT;
    obj->arrays[ATTRIB_COLOR].normalize = true;
    obj->arrays[ATTRIB_TEXCOORD].size = 4;
    obj->arrays[ATTRIB_TEXCOORD].type = GL_FLOAT;
    obj->arrays[ATTRIB_NORMAL].size = 3;
    obj->arrays[ATTRIB_NORMAL].type = GL_FLOAT;
    obj->arrays[ATTRIB_NORMAL].normalize = true;
    obj->arrays[ATTRIB_MTX_INDEX].size = 0;
    obj->arrays[ATTRIB_MTX_INDEX].type = GL_UNSIGNED_BYTE;

    for (uint32_t i = 0; i < ATTRIB_COUNT; i++)
    {
        gl_update_array(&obj->arrays[i], i);
    }
}

void gl_array_init()
{
    gl_array_object_init(&state.default_array_object);
    state.array_object = &state.default_array_object;
}

void gl_set_array(gl_array_type_t array_type, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    if (stride < 0) {
        gl_set_error(GL_INVALID_VALUE, "Stride must not be negative");
        return;
    }

    // From the spec (https://registry.khronos.org/OpenGL/extensions/ARB/ARB_vertex_array_object.txt):
    // An INVALID_OPERATION error is generated if any of the *Pointer commands
    // specifying the location and organization of vertex data are called while
    // a non-zero vertex array object is bound, zero is bound to the
    // ARRAY_BUFFER buffer object, and the pointer is not NULL[fn].
    //     [fn: This error makes it impossible to create a vertex array
    //           object containing client array pointers.]
    if (state.array_object != &state.default_array_object && state.array_buffer == NULL && pointer != NULL) {
        gl_set_error(GL_INVALID_OPERATION, "Vertex array objects can only be used in conjunction with vertex buffer objects");
        return;
    }

    gl_array_t *array = &state.array_object->arrays[array_type];

    array->size = size;
    array->type = type;
    array->stride = stride;
    array->pointer = pointer;
    array->binding = state.array_buffer;

    gl_update_array(array, array_type);
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

    gl_set_array(ATTRIB_VERTEX, size, type, stride, pointer);
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

    gl_set_array(ATTRIB_TEXCOORD, size, type, stride, pointer);
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
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid normal data type", type);
        return;
    }

    gl_set_array(ATTRIB_NORMAL, 3, type, stride, pointer);
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

    gl_set_array(ATTRIB_COLOR, size, type, stride, pointer);
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

    gl_set_array(ATTRIB_MTX_INDEX, size, type, stride, pointer);
}

void gl_set_array_enabled(gl_array_type_t array_type, bool enabled)
{
    gl_array_t *array = &state.array_object->arrays[array_type];
    array->enabled = enabled;
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
        gl_set_array_enabled(gl_array_type_from_enum(array), true);
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
        gl_set_array_enabled(gl_array_type_from_enum(array), false);
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
        gl_array_object_t *new_obj = calloc(sizeof(gl_array_object_t), 1);
        gl_array_object_init(new_obj);
        arrays[i] = (GLuint)new_obj;
    }
}

void glDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
    if (!gl_ensure_no_begin_end()) return;

    for (GLsizei i = 0; i < n; i++)
    {
        assertf(arrays[i] == 0 || is_valid_object_id(arrays[i]), 
            "Not a valid array object: %#lx. Make sure to allocate IDs via glGenVertexArray", arrays[i]);

        gl_array_object_t *obj = (gl_array_object_t*)arrays[i];
        if (obj == NULL) {
            continue;
        }

        if (obj == state.array_object) {
            glBindVertexArray(0);
        }

        free(obj);
    }
}

void glBindVertexArray(GLuint array)
{
    if (!gl_ensure_no_begin_end()) return;
    assertf(array == 0 || is_valid_object_id(array), 
        "Not a valid array object: %#lx. Make sure to allocate IDs via glGenVertexArray", array);

    gl_array_object_t *obj = (gl_array_object_t*)array;

    if (obj == NULL) {
        obj = &state.default_array_object;
    }

    state.array_object = obj;
}

GLboolean glIsVertexArray(GLuint array)
{
    if (!gl_ensure_no_begin_end()) return 0;
    
    // FIXME: This doesn't actually guarantee that it's a valid array object, but just uses the heuristic of
    //        "is it somewhere in the heap memory?". This way we can at least rule out arbitrarily chosen integer constants,
    //        which used to be valid array IDs in legacy OpenGL.
    return is_valid_object_id(array);
}
