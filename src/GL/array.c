#include "gl_internal.h"
#include "debug.h"

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

void read_u8(GLfloat *dst, const uint8_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = U8_TO_FLOAT(src[i]);
}

void read_i8(GLfloat *dst, const int8_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = I8_TO_FLOAT(src[i]);
}

void read_u16(GLfloat *dst, const uint16_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = U16_TO_FLOAT(src[i]);
}

void read_i16(GLfloat *dst, const int16_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = I16_TO_FLOAT(src[i]);
}

void read_u32(GLfloat *dst, const uint32_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = U32_TO_FLOAT(src[i]);
}

void read_i32(GLfloat *dst, const int32_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = I32_TO_FLOAT(src[i]);
}

void read_u8n(GLfloat *dst, const uint8_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

void read_i8n(GLfloat *dst, const int8_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

void read_u16n(GLfloat *dst, const uint16_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

void read_i16n(GLfloat *dst, const int16_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

void read_u32n(GLfloat *dst, const uint32_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

void read_i32n(GLfloat *dst, const int32_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

void read_f32(GLfloat *dst, const float *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

void read_f64(GLfloat *dst, const double *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

void gl_get_vertex_source(gl_vertex_source_t *source, const gl_array_t *array, bool normalize)
{
    if (!array->enabled) {
        return;
    }

    source->size = array->size;
    source->stride = array->stride;

    uint32_t size_shift = 0;
    
    switch (array->type) {
    case GL_BYTE:
        source->read_func = normalize ? (read_attrib_func)read_i8n : (read_attrib_func)read_i8;
        size_shift = 0;
        break;
    case GL_UNSIGNED_BYTE:
        source->read_func = normalize ? (read_attrib_func)read_u8n : (read_attrib_func)read_u8;
        size_shift = 0;
        break;
    case GL_SHORT:
        source->read_func = normalize ? (read_attrib_func)read_i16n : (read_attrib_func)read_i16;
        size_shift = 1;
        break;
    case GL_UNSIGNED_SHORT:
        source->read_func = normalize ? (read_attrib_func)read_u16n : (read_attrib_func)read_u16;
        size_shift = 1;
        break;
    case GL_INT:
        source->read_func = normalize ? (read_attrib_func)read_i32n : (read_attrib_func)read_i32;
        size_shift = 2;
        break;
    case GL_UNSIGNED_INT:
        source->read_func = normalize ? (read_attrib_func)read_u32n : (read_attrib_func)read_u32;
        size_shift = 2;
        break;
    case GL_FLOAT:
        source->read_func = (read_attrib_func)read_f32;
        size_shift = 3;
        break;
    case GL_DOUBLE:
        source->read_func = (read_attrib_func)read_f64;
        size_shift = 3;
        break;
    }

    source->elem_size = source->size << size_shift;

    if (source->stride == 0) {
        source->stride = source->elem_size;
    }

    if (array->binding != NULL) {
        source->pointer = array->binding->data + (uint32_t)array->pointer;
        source->copy_before_draw = false;
        source->final_stride = source->stride;
    } else {
        source->pointer = array->pointer;
        source->copy_before_draw = true;
        source->final_stride = source->elem_size;
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
    array->binding = state.array_buffer;
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
    gl_get_vertex_source(&state.vertex_sources[0], &state.vertex_array, false);
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
    gl_get_vertex_source(&state.vertex_sources[2], &state.texcoord_array, false);
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
    gl_get_vertex_source(&state.vertex_sources[3], &state.normal_array, true);
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
    gl_get_vertex_source(&state.vertex_sources[1], &state.color_array, true);
}

void glEnableClientState(GLenum array)
{
    switch (array) {
    case GL_VERTEX_ARRAY:
        state.vertex_array.enabled = true;
        gl_get_vertex_source(&state.vertex_sources[0], &state.vertex_array, false);
        break;
    case GL_TEXTURE_COORD_ARRAY:
        state.texcoord_array.enabled = true;
        gl_get_vertex_source(&state.vertex_sources[2], &state.texcoord_array, false);
        break;
    case GL_NORMAL_ARRAY:
        state.normal_array.enabled = true;
        gl_get_vertex_source(&state.vertex_sources[3], &state.normal_array, true);
        break;
    case GL_COLOR_ARRAY:
        state.color_array.enabled = true;
        gl_get_vertex_source(&state.vertex_sources[1], &state.color_array, true);
        break;
    case GL_EDGE_FLAG_ARRAY:
    case GL_INDEX_ARRAY:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        break;
    }
}
void glDisableClientState(GLenum array)
{
    switch (array) {
    case GL_VERTEX_ARRAY:
        state.vertex_array.enabled = false;
        gl_get_vertex_source(&state.vertex_sources[0], &state.vertex_array, false);
        break;
    case GL_TEXTURE_COORD_ARRAY:
        state.texcoord_array.enabled = false;
        gl_get_vertex_source(&state.vertex_sources[2], &state.texcoord_array, false);
        break;
    case GL_NORMAL_ARRAY:
        state.normal_array.enabled = false;
        gl_get_vertex_source(&state.vertex_sources[3], &state.normal_array, true);
        break;
    case GL_COLOR_ARRAY:
        state.color_array.enabled = false;
        gl_get_vertex_source(&state.vertex_sources[1], &state.color_array, true);
        break;
    case GL_EDGE_FLAG_ARRAY:
    case GL_INDEX_ARRAY:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        break;
    }
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
