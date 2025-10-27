/**
 * @file array.c
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 * @brief OpenGL array and attribute pointer management for vertex data.
 */
#include "gl_internal.h"
#include "debug.h"
#include <malloc.h>
#include "mgfx.h"

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

extern const cpu_read_attrib_func cpu_read_funcs[ATTRIB_COUNT][ATTRIB_TYPE_COUNT];
extern const rsp_read_attrib_func rsp_read_funcs[ATTRIB_COUNT][ATTRIB_TYPE_COUNT];

inline void assert_valid_array(GLuint array)
{
    assertf(array == 0 || is_valid_object_id(array),
            "Not a valid array object: %#lx. Make sure to allocate IDs via glGenVertexArray", array);
}

gl_array_type_t gl_array_type_from_enum(GLenum array)
{
    switch (array) {
    case GL_VERTEX_ARRAY:
        return ATTRIB_VERTEX;
    case GL_NORMAL_ARRAY:
        return ATTRIB_NORMAL;
    case GL_COLOR_ARRAY:
        return ATTRIB_COLOR;
    case GL_TEXTURE_COORD_ARRAY:
        return ATTRIB_TEXCOORD;
    case GL_MATRIX_INDEX_ARRAY_ARB:
        return ATTRIB_MTX_INDEX;
    default:
        assertf(0, "Invalid array type!");
    }
}

uint16_t get_stride_from_size_and_type(GLint size, GLenum type)
{
    if (type == GL_SHORT_5_6_5_N64) {
        return sizeof(GLshort);
    }

    uint32_t size_shift = 0;
    
    switch (type) {
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

    return size << size_shift;
}

void gl_update_array(gl_array_t *array, gl_array_type_t array_type)
{
    array->final_stride = array->stride == 0 ? get_stride_from_size_and_type(array->size, array->type) : array->stride;

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
    for (gl_array_type_t i = 0; i < ATTRIB_COUNT; i++)
    {
        gl_update_array_pointer(&obj->arrays[i]);
    }
}

static void array_object_init(gl_array_object_t *obj)
{
    obj->arrays[ATTRIB_VERTEX].size = 4;
    obj->arrays[ATTRIB_VERTEX].type = GL_FLOAT;
    obj->arrays[ATTRIB_NORMAL].size = 3;
    obj->arrays[ATTRIB_NORMAL].type = GL_FLOAT;
    obj->arrays[ATTRIB_NORMAL].normalize = true;
    obj->arrays[ATTRIB_COLOR].size = 4;
    obj->arrays[ATTRIB_COLOR].type = GL_FLOAT;
    obj->arrays[ATTRIB_COLOR].normalize = true;
    obj->arrays[ATTRIB_TEXCOORD].size = 4;
    obj->arrays[ATTRIB_TEXCOORD].type = GL_FLOAT;
    obj->arrays[ATTRIB_MTX_INDEX].size = 0;
    obj->arrays[ATTRIB_MTX_INDEX].type = GL_UNSIGNED_BYTE;

    for (gl_array_type_t i = 0; i < ATTRIB_COUNT; i++)
    {
        gl_update_array(&obj->arrays[i], i);
    }

    obj->is_layout_dirty = true;
}

static void array_object_free(gl_array_object_t *obj)
{
    if (obj->buffer != NULL) {
        rspq_call_deferred(free_uncached, obj->buffer);
    }

    buffer_object_set_binding(NULL, &obj->element_array_buffer);

    for (gl_array_type_t i = 0; i < ATTRIB_COUNT; i++)
    {
        array_object_set_buffer_binding(obj, i, NULL);
    }
}

void gl_array_init()
{
    array_object_init(&state->default_array_object);
    glBindVertexArray(0);
}

void gl_array_close()
{
    glBindVertexArray(0);
    array_object_free(&state->default_array_object);
}

void array_object_set_buffer_binding(gl_array_object_t *obj, gl_array_type_t array_type, gl_buffer_object_t *buffer)
{
    gl_array_t *array = &obj->arrays[array_type];
    if (array->binding == buffer) return;

    gl_buffer_object_t *old_binding = array->binding;
    if (old_binding != NULL) {
        gl_buffer_remove_array_ref(old_binding, state->array_object);
    }
    if (buffer != NULL) {
        gl_buffer_add_array_ref(buffer, state->array_object);
    }
    array->binding = buffer;
    obj->are_bindings_dirty = true;
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
    if (state->array_object != &state->default_array_object && state->array_buffer == NULL && pointer != NULL) {
        gl_set_error(GL_INVALID_OPERATION, "Vertex array objects can only be used in conjunction with vertex buffer objects");
        return;
    }

    gl_array_t *array = &state->array_object->arrays[array_type];

    array->size = size;
    array->type = type;
    array->stride = stride;
    array->pointer = pointer;
    array_object_set_buffer_binding(state->array_object, array_type, state->array_buffer);

    gl_update_array(array, array_type);

    state->array_object->is_layout_dirty = true;
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
    case GL_SHORT_5_6_5_N64:
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
    gl_array_t *array = &state->array_object->arrays[array_type];
    if (array->enabled != enabled) {
        array->enabled = enabled;
        state->array_object->is_layout_dirty = true;
    }
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
        gl_array_object_t *new_obj = calloc(1, sizeof(gl_array_object_t));
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

        array_object_free(obj);
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

static void array_object_update_layout(gl_array_object_t *array_object)
{
    vertex_layout *vl = &array_object->layout;
    vertex_layout_init(vl);

    if (array_object->arrays[ATTRIB_VERTEX].enabled) {
        array_object->out_offsets[ATTRIB_VERTEX] = vl->vertex_layout.stride;
        vertex_layout_append(vl, MGFX_ATTRIBUTE_POS_NORM, sizeof(int16_t) * 4);
    }

    if (array_object->arrays[ATTRIB_NORMAL].enabled) {
        array_object->out_offsets[ATTRIB_NORMAL] = vl->vertex_layout.stride - sizeof(int16_t);
    }

    if (array_object->arrays[ATTRIB_COLOR].enabled) {
        array_object->out_offsets[ATTRIB_COLOR] = vl->vertex_layout.stride;
        vertex_layout_append(vl, MGFX_ATTRIBUTE_COLOR, sizeof(uint32_t));
    }

    if (array_object->arrays[ATTRIB_TEXCOORD].enabled) {
        array_object->out_offsets[ATTRIB_TEXCOORD] = vl->vertex_layout.stride;
        vertex_layout_append(vl, MGFX_ATTRIBUTE_TEXCOORD, sizeof(int16_t) * 2);
    }
}

static void array_object_update_is_all_vbos(gl_array_object_t *array_object)
{
    bool is_all_vbos = true;
    for (gl_array_type_t i = 0; i < ATTRIB_COUNT; i++)
    {
        if (array_object->arrays[i].enabled && array_object->arrays[i].binding == NULL) {
            is_all_vbos = false;
            break;
        }
    }
    
    array_object->is_all_vbos = is_all_vbos;
}

void array_convert(gl_array_object_t *obj, const uint32_t out_offsets[ATTRIB_COUNT], void *dst_buffer, uint32_t first, uint32_t count, uint32_t stride)
{
    gl_array_t *arrays[ATTRIB_COUNT];
    uint32_t offsets[ATTRIB_COUNT];
    uint32_t array_count = 0;
    // TODO: convert matrix indices too as soon as they are supported
    for (size_t i = 0; i < ATTRIB_MTX_INDEX; i++)
    {
        if (!obj->arrays[i].enabled) continue;
        offsets[array_count] = out_offsets[i];
        arrays[array_count++] = &obj->arrays[i];
    }    

    for (size_t i = 0; i < count; i++)
    {
        for (size_t j = 0; j < array_count; j++)
        {
            uint8_t *dst = ((uint8_t*)dst_buffer) + offsets[j] + i * stride;
            const uint8_t *src = ((const uint8_t*)arrays[j]->final_pointer) + (i+first) * arrays[j]->final_stride;
            arrays[j]->rsp_read_func(dst, src, arrays[j]->size);
        }
    }
}

static void array_object_convert_data(gl_array_object_t *array_object, uint32_t first, uint32_t count)
{
    uint32_t stride = array_object->layout.vertex_layout.stride;

    // TODO: allocate from a pool?
    if (array_object->buffer != NULL) {
        rspq_call_deferred(free_uncached, array_object->buffer);
    }
    array_object->buffer = malloc_uncached(stride * count);

    array_convert(array_object, array_object->out_offsets, array_object->buffer, first, count, stride);
}

void array_object_update(gl_array_object_t *array_object, uint32_t first, uint32_t count)
{
    // TODO: throw INVALID OPERATION if any VBOs are currently mapped

    if (array_object->is_layout_dirty) {
        array_object_update_layout(array_object);
        array_object->is_layout_dirty = false;
        array_object->is_data_dirty = true;
    }

    if (array_object->are_bindings_dirty) {
        array_object_update_is_all_vbos(array_object);
        array_object->are_bindings_dirty = false;
        array_object->is_data_dirty = true;
    }

    uint32_t end = first + count;
    uint32_t cached_end = array_object->cached_first + array_object->cached_count;

    // If one or more arrays are not bound to VBOs, we cannot track when vertex data has actually changed.
    // Therefore, we must always assume the data has changed and re-convert.
    if (array_object->is_data_dirty || !array_object->is_all_vbos || first < array_object->cached_first || end > cached_end) {
        array_object_convert_data(array_object, first, count);
        array_object->cached_first = first;
        array_object->cached_count = count;
        array_object->is_data_dirty = false;
    }
}

