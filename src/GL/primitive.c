/**
 * @file primitive.c
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 * @brief OpenGL primitive drawing and vertex processing routines.
 */
#include "gl_internal.h"
#include "utils.h"
#include "rdpq.h"
#include "rdpq_tri.h"
#include "rdpq_rect.h"
#include "rdpq_mode.h"
#include "rdpq_debug.h"
#include "../rdpq/rdpq_internal.h"
#include <malloc.h>
#include <string.h>
#include "array.h"

_Static_assert(((RDPQ_CMD_TRI << 8) | (FLAG_DEPTH_TEST     >> TRICMD_ATTR_SHIFT)) == (RDPQ_CMD_TRI_ZBUF << 8));
_Static_assert(((RDPQ_CMD_TRI << 8) | (FLAG_TEXTURE_ACTIVE >> TRICMD_ATTR_SHIFT)) == (RDPQ_CMD_TRI_TEX  << 8));
_Static_assert(((RDPQ_CMD_TRI << 8) | TRICMD_ATTR_MASK) == (RDPQ_CMD_TRI_TEX_ZBUF << 8));
_Static_assert(((FLAG_DEPTH_TEST | FLAG_TEXTURE_ACTIVE) >> TRICMD_ATTR_SHIFT) == TRICMD_ATTR_MASK);

extern gl_state_t *state;

void gl_primitive_init()
{
    state->tex_gen[0].mode = GL_EYE_LINEAR;
    state->tex_gen[0].object_plane[0] = 1;
    state->tex_gen[0].eye_plane[0] = 1;

    state->tex_gen[1].mode = GL_EYE_LINEAR;
    state->tex_gen[1].object_plane[1] = 1;
    state->tex_gen[1].eye_plane[1] = 1;

    state->tex_gen[2].mode = GL_EYE_LINEAR;
    state->tex_gen[3].mode = GL_EYE_LINEAR;

    state->point_size = 1;
    state->line_width = 1;

    glNormal3f(0, 0, 1);
    glColor4f(1, 1, 1, 1);
    glTexCoord4f(0, 0, 0, 1);

    state->vertex_halfx_precision.target_precision = GLP_VTX_POS_SHIFT;
    state->texcoord_halfx_precision.target_precision = GLP_VTX_TEX_SHIFT;

    glVertexHalfFixedPrecisionN64(GLP_VTX_POS_SHIFT);
    glTexCoordHalfFixedPrecisionN64(GLP_VTX_TEX_SHIFT);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    set_can_use_rsp_dirty();

    vertex_layout_init(&state->begin_end_layout);
    vertex_layout_add(&state->begin_end_layout, GLP_ATTRIBUTE_POS_NORM, offsetof(native_vertex_t, position), sizeof(int16_t)*4);
    vertex_layout_add(&state->begin_end_layout, GLP_ATTRIBUTE_COLOR, offsetof(native_vertex_t, color), sizeof(uint32_t));
    vertex_layout_add(&state->begin_end_layout, GLP_ATTRIBUTE_TEXCOORD, offsetof(native_vertex_t, texcoord), sizeof(int16_t)*2);
    state->begin_end_layout.vertex_layout.stride = sizeof(native_vertex_t);
}

void gl_primitive_close()
{
    if (state->begin_end_buffer.buffer != NULL) {
        ringbuffer_free(&state->begin_end_buffer);
    }
}

bool gl_can_use_rsp_pipeline(GLenum mode)
{
    #define WARN_CPU_REQUIRED(msg) ({ \
        static bool warn_state ## __LINE__; \
        if (!warn_state ## __LINE__) { \
            warn_state ## __LINE__ = true; \
            debugf("GL WARNING: The CPU pipeline is being used because a feature is enabled that is not supported on RSP: " msg "\n"); \
        } \
    })

    switch (mode) {
    case GL_TRIANGLES:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_POLYGON:
        break;
    case GL_POINTS:
    case GL_LINES:
    case GL_LINE_LOOP:
    case GL_LINE_STRIP:
    case GL_QUADS:
    case GL_QUAD_STRIP:
    default:
        WARN_CPU_REQUIRED("primitive mode");
        return false;
    }

    // Points and lines are not implemented
    if (state->polygon_mode != GL_FILL) {
        WARN_CPU_REQUIRED("polygon mode");
        return false;
    }

    if (state->lighting) {
        // Flat shading is not implemented
        if (state->shade_model == GL_FLAT) {
            WARN_CPU_REQUIRED("flat shading");
            return false;
        }

        // Spot lights are not implemented
        for (uint32_t i = 0; i < LIGHT_COUNT; i++)
        {
            if (state->lights[i].spot_cutoff_cos >= 0.0f) {
                WARN_CPU_REQUIRED("spotlights");
                return false;
            }
        }
        
        // Specular material is not implemented
        if (state->material.specular[0] != 0.0f || 
            state->material.specular[1] != 0.0f || 
            state->material.specular[2] != 0.0f) {
            WARN_CPU_REQUIRED("specular lighting");
            return false;
        }
    }

    for (size_t i = 0; i < TEX_GEN_COUNT; i++)
    {
        if (state->tex_gen[i].enabled) {
            if (state->tex_gen[i].mode == GL_OBJECT_LINEAR) {
                WARN_CPU_REQUIRED("tex gen object linear");
                return false;
            }
            if (state->tex_gen[i].mode == GL_EYE_LINEAR) {
                WARN_CPU_REQUIRED("tex gen eye linear");
                return false;
            }
        }
    }

    return true;

    #undef WARN_CPU_REQUIRED
}

void set_can_use_rsp_dirty()
{
    state->can_use_rsp_dirty = true;
}

extern const gl_pipeline_t gl_cpu_pipeline;
extern const gl_pipeline_t gl_rsp_pipeline;

static void prepare_drawing(GLenum mode)
{
    state->can_use_rsp = gl_can_use_rsp_pipeline(mode);
    state->current_pipeline = state->can_use_rsp ? &gl_rsp_pipeline : &gl_cpu_pipeline;

    gl_pre_init_pipe(mode);
    array_object_update_pointers(state->array_object);
}

static bool validate_mode(GLenum mode)
{
    switch (mode) {
    case GL_POINTS:
    case GL_LINES:
    case GL_LINE_LOOP:
    case GL_LINE_STRIP:
    case GL_TRIANGLES:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_QUADS:
    case GL_QUAD_STRIP:
    case GL_POLYGON:
        return true;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid primitive mode", mode);
        return false;
    }
}

void glBegin(GLenum mode)
{
    if (!gl_ensure_no_begin_end()) return;
    if (!validate_mode(mode)) return;

    prepare_drawing(mode);
    state->begin_end_active = true;
    state->current_pipeline->begin(mode);
}

void glEnd(void)
{
    if (!state->begin_end_active) {
        gl_set_error(GL_INVALID_OPERATION, "glEnd must be called after glBegin");
    }

    state->current_pipeline->end();
    state->begin_end_active = false;
}

void gl_load_attribs(const array_t *arrays, uint32_t index)
{
    for (uint32_t i = 0; i < ARRAY_COUNT; i++)
    {
        const array_t *array = &arrays[i];
        if (!array->enabled) {
            continue;
        }

        void *dst = gl_get_attrib_pointer(&state->current_attributes, i);
        const void *src = gl_get_attrib_element(array, index);

        array->cpu_read_func(dst, src, array->size);
    }
}

void gl_fill_attrib_defaults(array_type_t array_type, uint32_t size)
{
    static const GLfloat default_attribute_value[] = {0.0f, 0.0f, 0.0f, 1.0f};
    uint32_t element_size = sizeof(GLfloat);

    switch (array_type) {
    case ARRAY_VERTEX:
    case ARRAY_COLOR:
    case ARRAY_TEXCOORD:
        break;
    default:
        return;
    }

    const GLfloat *src = default_attribute_value + size;
    void *dst = gl_get_attrib_pointer(&state->current_attributes, array_type) + size * element_size;
    memcpy(dst, src, (4 - size) * element_size);
}

void gl_fill_all_attrib_defaults(const array_t *arrays)
{
    // There are no default values for the matrix index because it is always specified fully.
    for (uint32_t i = 0; i < ARRAY_COUNT; i++)
    {
        const array_t *array = &arrays[i];
        if (!arrays[i].enabled) {
            continue;
        }

        gl_fill_attrib_defaults(i, array->size);
    }
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    if (!gl_ensure_no_begin_end()) return;
    if (!validate_mode(mode)) return;

    if (count == 0) {
        return;
    }

    array_object_validate_drawing(state->array_object, false);

    prepare_drawing(mode);
    state->current_pipeline->draw_arrays(mode, first, count);
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    if (!gl_ensure_no_begin_end()) return;
    if (!validate_mode(mode)) return;
    
    if (count == 0) {
        return;
    }

    array_object_validate_drawing(state->array_object, true);

    prepare_drawing(mode);
    state->current_pipeline->draw_elements(mode, count, indices, type);
}

void glArrayElement(GLint i)
{
    // Calling glArrayElement while the vertex array is enabled has, among other things,
    // the same effect as glVertex. See __gl_vertex for that function's behavior.
    assertf(!state->array_object->arrays[ARRAY_VERTEX].enabled || state->begin_end_active, 
        "glArrayElement was called outside of glBegin/glEnd while vertex array was enabled");

    if (i < 0) {
        gl_set_error(GL_INVALID_VALUE, "Index must not be negative");
        return;
    }

    array_object_validate_drawing(state->array_object, false);

    state->current_pipeline->array_element(i);
}

void __gl_vertex(GLenum type, const void *value, uint32_t size)
{
    // According to the spec, calling glVertex outside of glBegin/glEnd 
    // specifically results in UB instead of generating an error, so just assert.
    assertf(state->begin_end_active, "glVertex was called outside of glBegin/glEnd");
    state->current_pipeline->vertex(value, type, size);
}

static void read_attrib(array_type_t array_type, const void *value, GLenum type, uint32_t size)
{
    gl_read_attrib(array_type, value, type, size);
    rsp_read_attrib(array_type, type, value, size);
}

void __gl_normal(GLenum type, const void *value, uint32_t size)
{
    read_attrib(ARRAY_NORMAL, value, type, size);
    if (!state->begin_end_active) {
        gl_set_current_normal(state->current_attributes.normal);
    }
}

void __gl_color(GLenum type, const void *value, uint32_t size)
{
    read_attrib(ARRAY_COLOR, value, type, size);
    if (!state->begin_end_active) {
        gl_set_current_color(state->current_attributes.color);
    }
}

void __gl_tex_coord(GLenum type, const void *value, uint32_t size)
{
    read_attrib(ARRAY_TEXCOORD, value, type, size);
    if (!state->begin_end_active) {
        gl_set_current_texcoords(state->current_attributes.texcoord);
    }
}

void __gl_mtx_index(GLenum type, const void *value, uint32_t size)
{
    if (size > VERTEX_UNIT_COUNT) {
        gl_set_error(GL_INVALID_VALUE, "Size must not be greater than %d", VERTEX_UNIT_COUNT);
        return;
    }

    read_attrib(ARRAY_MTX_INDEX, value, type, size);
    if (!state->begin_end_active) {
        gl_set_current_mtx_index(state->current_attributes.mtx_index);
    }
    state->current_pipeline->mtx_index(state->current_attributes.mtx_index);
}

#define __ATTR_IMPL(func, argtype, enumtype, ...) ({\
    argtype tmp[] = { __VA_ARGS__ }; \
    func(enumtype, tmp, __COUNT_VARARGS(__VA_ARGS__)); \
})

void glVertex2sv(const GLshort *v)          { __gl_vertex(GL_SHORT,             v, 2); }
void glVertex2iv(const GLint *v)            { __gl_vertex(GL_INT,               v, 2); }
void glVertex2fv(const GLfloat *v)          { __gl_vertex(GL_FLOAT,             v, 2); }
void glVertex2dv(const GLdouble *v)         { __gl_vertex(GL_DOUBLE,            v, 2); }
void glVertex2hxvN64(const GLhalfxN64 *v)   { __gl_vertex(GL_HALF_FIXED_N64,    v, 2); }

void glVertex3sv(const GLshort *v)          { __gl_vertex(GL_SHORT,             v, 3); }
void glVertex3iv(const GLint *v)            { __gl_vertex(GL_INT,               v, 3); }
void glVertex3fv(const GLfloat *v)          { __gl_vertex(GL_FLOAT,             v, 3); }
void glVertex3dv(const GLdouble *v)         { __gl_vertex(GL_DOUBLE,            v, 3); }
void glVertex3hxvN64(const GLhalfxN64 *v)   { __gl_vertex(GL_HALF_FIXED_N64,    v, 3); }

void glVertex4sv(const GLshort *v)          { __gl_vertex(GL_SHORT,             v, 4); }
void glVertex4iv(const GLint *v)            { __gl_vertex(GL_INT,               v, 4); }
void glVertex4fv(const GLfloat *v)          { __gl_vertex(GL_FLOAT,             v, 4); }
void glVertex4dv(const GLdouble *v)         { __gl_vertex(GL_DOUBLE,            v, 4); }
void glVertex4hxvN64(const GLhalfxN64 *v)   { __gl_vertex(GL_HALF_FIXED_N64,    v, 4); }

void glVertex2s(GLshort x, GLshort y)                                       { __ATTR_IMPL(__gl_vertex, GLshort,     GL_SHORT,           x, y); }
void glVertex2i(GLint x, GLint y)                                           { __ATTR_IMPL(__gl_vertex, GLint,       GL_INT,             x, y); }
void glVertex2f(GLfloat x, GLfloat y)                                       { __ATTR_IMPL(__gl_vertex, GLfloat,     GL_FLOAT,           x, y); }
void glVertex2d(GLdouble x, GLdouble y)                                     { __ATTR_IMPL(__gl_vertex, GLdouble,    GL_DOUBLE,          x, y); }
void glVertex2hxN64(GLhalfxN64 x, GLhalfxN64 y)                             { __ATTR_IMPL(__gl_vertex, GLhalfxN64,  GL_HALF_FIXED_N64,  x, y); }

void glVertex3s(GLshort x, GLshort y, GLshort z)                            { __ATTR_IMPL(__gl_vertex, GLshort,     GL_SHORT,           x, y, z); }
void glVertex3i(GLint x, GLint y, GLint z)                                  { __ATTR_IMPL(__gl_vertex, GLint,       GL_INT,             x, y, z); }
void glVertex3f(GLfloat x, GLfloat y, GLfloat z)                            { __ATTR_IMPL(__gl_vertex, GLfloat,     GL_FLOAT,           x, y, z); }
void glVertex3d(GLdouble x, GLdouble y, GLdouble z)                         { __ATTR_IMPL(__gl_vertex, GLdouble,    GL_DOUBLE,          x, y, z); }
void glVertex3hxN64(GLhalfxN64 x, GLhalfxN64 y, GLhalfxN64 z)               { __ATTR_IMPL(__gl_vertex, GLhalfxN64,  GL_HALF_FIXED_N64,  x, y, z); }

void glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w)                 { __ATTR_IMPL(__gl_vertex, GLshort,     GL_SHORT,           x, y, z, w); }
void glVertex4i(GLint x, GLint y, GLint z, GLint w)                         { __ATTR_IMPL(__gl_vertex, GLint,       GL_INT,             x, y, z, w); }
void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)                 { __ATTR_IMPL(__gl_vertex, GLfloat,     GL_FLOAT,           x, y, z, w); }
void glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w)             { __ATTR_IMPL(__gl_vertex, GLdouble,    GL_DOUBLE,          x, y, z, w); }
void glVertex4hxN64(GLhalfxN64 x, GLhalfxN64 y, GLhalfxN64 z, GLhalfxN64 w) { __ATTR_IMPL(__gl_vertex, GLhalfxN64,  GL_HALF_FIXED_N64,  x, y, z, w); }

void glColor3bv(const GLbyte *v)    { __gl_color(GL_BYTE,           v, 3); }
void glColor3sv(const GLshort *v)   { __gl_color(GL_SHORT,          v, 3); }
void glColor3iv(const GLint *v)     { __gl_color(GL_INT,            v, 3); }
void glColor3fv(const GLfloat *v)   { __gl_color(GL_FLOAT,          v, 3); }
void glColor3dv(const GLdouble *v)  { __gl_color(GL_DOUBLE,         v, 3); }
void glColor3ubv(const GLubyte *v)  { __gl_color(GL_UNSIGNED_BYTE,  v, 3); }
void glColor3usv(const GLushort *v) { __gl_color(GL_UNSIGNED_SHORT, v, 3); }
void glColor3uiv(const GLuint *v)   { __gl_color(GL_UNSIGNED_INT,   v, 3); }

void glColor4bv(const GLbyte *v)    { __gl_color(GL_BYTE,           v, 4); }
void glColor4sv(const GLshort *v)   { __gl_color(GL_SHORT,          v, 4); }
void glColor4iv(const GLint *v)     { __gl_color(GL_INT,            v, 4); }
void glColor4fv(const GLfloat *v)   { __gl_color(GL_FLOAT,          v, 4); }
void glColor4dv(const GLdouble *v)  { __gl_color(GL_DOUBLE,         v, 4); }
void glColor4ubv(const GLubyte *v)  { __gl_color(GL_UNSIGNED_BYTE,  v, 4); }
void glColor4usv(const GLushort *v) { __gl_color(GL_UNSIGNED_SHORT, v, 4); }
void glColor4uiv(const GLuint *v)   { __gl_color(GL_UNSIGNED_INT,   v, 4); }

void glColor3b(GLbyte r, GLbyte g, GLbyte b)                    { __ATTR_IMPL(__gl_color, GLbyte,   GL_BYTE,            r, g, b); }
void glColor3s(GLshort r, GLshort g, GLshort b)                 { __ATTR_IMPL(__gl_color, GLshort,  GL_SHORT,           r, g, b); }
void glColor3i(GLint r, GLint g, GLint b)                       { __ATTR_IMPL(__gl_color, GLint,    GL_INT,             r, g, b); }
void glColor3f(GLfloat r, GLfloat g, GLfloat b)                 { __ATTR_IMPL(__gl_color, GLfloat,  GL_FLOAT,           r, g, b); }
void glColor3d(GLdouble r, GLdouble g, GLdouble b)              { __ATTR_IMPL(__gl_color, GLdouble, GL_DOUBLE,          r, g, b); }
void glColor3ub(GLubyte r, GLubyte g, GLubyte b)                { __ATTR_IMPL(__gl_color, GLubyte,  GL_UNSIGNED_BYTE,   r, g, b); }
void glColor3us(GLushort r, GLushort g, GLushort b)             { __ATTR_IMPL(__gl_color, GLushort, GL_UNSIGNED_SHORT,  r, g, b); }
void glColor3ui(GLuint r, GLuint g, GLuint b)                   { __ATTR_IMPL(__gl_color, GLuint,   GL_UNSIGNED_INT,    r, g, b); }

void glColor4b(GLbyte r, GLbyte g, GLbyte b, GLbyte a)          { __ATTR_IMPL(__gl_color, GLbyte,   GL_BYTE,            r, g, b, a); }
void glColor4s(GLshort r, GLshort g, GLshort b, GLshort a)      { __ATTR_IMPL(__gl_color, GLshort,  GL_SHORT,           r, g, b, a); }
void glColor4i(GLint r, GLint g, GLint b, GLint a)              { __ATTR_IMPL(__gl_color, GLint,    GL_INT,             r, g, b, a); }
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)      { __ATTR_IMPL(__gl_color, GLfloat,  GL_FLOAT,           r, g, b, a); }
void glColor4d(GLdouble r, GLdouble g, GLdouble b, GLdouble a)  { __ATTR_IMPL(__gl_color, GLdouble, GL_DOUBLE,          r, g, b, a); }
void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)     { __ATTR_IMPL(__gl_color, GLubyte,  GL_UNSIGNED_BYTE,   r, g, b, a); }
void glColor4us(GLushort r, GLushort g, GLushort b, GLushort a) { __ATTR_IMPL(__gl_color, GLushort, GL_UNSIGNED_SHORT,  r, g, b, a); }
void glColor4ui(GLuint r, GLuint g, GLuint b, GLuint a)         { __ATTR_IMPL(__gl_color, GLuint,   GL_UNSIGNED_INT,    r, g, b, a); }

void glTexCoord1sv(const GLshort *v)        { __gl_tex_coord(GL_SHORT,          v, 1); }
void glTexCoord1iv(const GLint *v)          { __gl_tex_coord(GL_INT,            v, 1); }
void glTexCoord1fv(const GLfloat *v)        { __gl_tex_coord(GL_FLOAT,          v, 1); }
void glTexCoord1dv(const GLdouble *v)       { __gl_tex_coord(GL_DOUBLE,         v, 1); }
void glTexCoord1hxvN64(const GLhalfxN64 *v) { __gl_tex_coord(GL_HALF_FIXED_N64, v, 1); }

void glTexCoord2sv(const GLshort *v)        { __gl_tex_coord(GL_SHORT,          v, 2); }
void glTexCoord2iv(const GLint *v)          { __gl_tex_coord(GL_INT,            v, 2); }
void glTexCoord2fv(const GLfloat *v)        { __gl_tex_coord(GL_FLOAT,          v, 2); }
void glTexCoord2dv(const GLdouble *v)       { __gl_tex_coord(GL_DOUBLE,         v, 2); }
void glTexCoord2hxvN64(const GLhalfxN64 *v) { __gl_tex_coord(GL_HALF_FIXED_N64, v, 2); }

void glTexCoord3sv(const GLshort *v)        { __gl_tex_coord(GL_SHORT,          v, 3); }
void glTexCoord3iv(const GLint *v)          { __gl_tex_coord(GL_INT,            v, 3); }
void glTexCoord3fv(const GLfloat *v)        { __gl_tex_coord(GL_FLOAT,          v, 3); }
void glTexCoord3dv(const GLdouble *v)       { __gl_tex_coord(GL_DOUBLE,         v, 3); }
void glTexCoord3hxvN64(const GLhalfxN64 *v) { __gl_tex_coord(GL_HALF_FIXED_N64, v, 3); }

void glTexCoord4sv(const GLshort *v)        { __gl_tex_coord(GL_SHORT,          v, 4); }
void glTexCoord4iv(const GLint *v)          { __gl_tex_coord(GL_INT,            v, 4); }
void glTexCoord4fv(const GLfloat *v)        { __gl_tex_coord(GL_FLOAT,          v, 4); }
void glTexCoord4dv(const GLdouble *v)       { __gl_tex_coord(GL_DOUBLE,         v, 4); }
void glTexCoord4hxvN64(const GLhalfxN64 *v) { __gl_tex_coord(GL_HALF_FIXED_N64, v, 4); }

void glTexCoord1s(GLshort s)                                                    { __ATTR_IMPL(__gl_tex_coord, GLshort,      GL_SHORT,           s); }
void glTexCoord1i(GLint s)                                                      { __ATTR_IMPL(__gl_tex_coord, GLint,        GL_INT,             s); }
void glTexCoord1f(GLfloat s)                                                    { __ATTR_IMPL(__gl_tex_coord, GLfloat,      GL_FLOAT,           s); }
void glTexCoord1d(GLdouble s)                                                   { __ATTR_IMPL(__gl_tex_coord, GLdouble,     GL_DOUBLE,          s); }
void glTexCoord1hxN64(GLhalfxN64 s)                                             { __ATTR_IMPL(__gl_tex_coord, GLhalfxN64,   GL_HALF_FIXED_N64,  s); }

void glTexCoord2s(GLshort s, GLshort t)                                         { __ATTR_IMPL(__gl_tex_coord, GLshort,      GL_SHORT,           s, t); }
void glTexCoord2i(GLint s, GLint t)                                             { __ATTR_IMPL(__gl_tex_coord, GLint,        GL_INT,             s, t); }
void glTexCoord2f(GLfloat s, GLfloat t)                                         { __ATTR_IMPL(__gl_tex_coord, GLfloat,      GL_FLOAT,           s, t); }
void glTexCoord2d(GLdouble s, GLdouble t)                                       { __ATTR_IMPL(__gl_tex_coord, GLdouble,     GL_DOUBLE,          s, t); }
void glTexCoord2hxN64(GLhalfxN64 s, GLhalfxN64 t)                               { __ATTR_IMPL(__gl_tex_coord, GLhalfxN64,   GL_HALF_FIXED_N64,  s, t); }

void glTexCoord3s(GLshort s, GLshort t, GLshort r)                              { __ATTR_IMPL(__gl_tex_coord, GLshort,      GL_SHORT,           s, t, r); }
void glTexCoord3i(GLint s, GLint t, GLint r)                                    { __ATTR_IMPL(__gl_tex_coord, GLint,        GL_INT,             s, t, r); }
void glTexCoord3f(GLfloat s, GLfloat t, GLfloat r)                              { __ATTR_IMPL(__gl_tex_coord, GLfloat,      GL_FLOAT,           s, t, r); }
void glTexCoord3d(GLdouble s, GLdouble t, GLdouble r)                           { __ATTR_IMPL(__gl_tex_coord, GLdouble,     GL_DOUBLE,          s, t, r); }
void glTexCoord3hxN64(GLhalfxN64 s, GLhalfxN64 t, GLhalfxN64 r)                 { __ATTR_IMPL(__gl_tex_coord, GLhalfxN64,   GL_HALF_FIXED_N64,  s, t, r); }

void glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q)                   { __ATTR_IMPL(__gl_tex_coord, GLshort,      GL_SHORT,           s, t, r, q); }
void glTexCoord4i(GLint s, GLint t, GLint r, GLint q)                           { __ATTR_IMPL(__gl_tex_coord, GLint,        GL_INT,             s, t, r, q); }
void glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q)                   { __ATTR_IMPL(__gl_tex_coord, GLfloat,      GL_FLOAT,           s, t, r, q); }
void glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q)               { __ATTR_IMPL(__gl_tex_coord, GLdouble,     GL_DOUBLE,          s, t, r, q); }
void glTexCoord4hxN64(GLhalfxN64 s, GLhalfxN64 t, GLhalfxN64 r, GLhalfxN64 q)   { __ATTR_IMPL(__gl_tex_coord, GLhalfxN64,   GL_HALF_FIXED_N64,  s, t, r, q); }

void glNormal3bv(const GLbyte *v)   { __gl_normal(GL_BYTE,      v, 3); }
void glNormal3sv(const GLshort *v)  { __gl_normal(GL_SHORT,     v, 3); }
void glNormal3iv(const GLint *v)    { __gl_normal(GL_INT,       v, 3); }
void glNormal3fv(const GLfloat *v)  { __gl_normal(GL_FLOAT,     v, 3); }
void glNormal3dv(const GLdouble *v) { __gl_normal(GL_DOUBLE,    v, 3); }

void glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz)        { __ATTR_IMPL(__gl_normal, GLbyte,      GL_BYTE,    nx, ny, nz); }
void glNormal3s(GLshort nx, GLshort ny, GLshort nz)     { __ATTR_IMPL(__gl_normal, GLshort,     GL_SHORT,   nx, ny, nz); }
void glNormal3i(GLint nx, GLint ny, GLint nz)           { __ATTR_IMPL(__gl_normal, GLint,       GL_INT,     nx, ny, nz); }
void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)     { __ATTR_IMPL(__gl_normal, GLfloat,     GL_FLOAT,   nx, ny, nz); }
void glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz)  { __ATTR_IMPL(__gl_normal, GLdouble,    GL_DOUBLE,  nx, ny, nz); }

void glMatrixIndexubvARB(GLint size, const GLubyte *v)  { __gl_mtx_index(GL_UNSIGNED_BYTE,  v, size); }
void glMatrixIndexusvARB(GLint size, const GLushort *v) { __gl_mtx_index(GL_UNSIGNED_SHORT, v, size); }
void glMatrixIndexuivARB(GLint size, const GLuint *v)   { __gl_mtx_index(GL_UNSIGNED_INT,   v, size); }

static void set_precision_bits(gl_fixed_precision_t *dst, GLuint bits)
{
    // One bit is reserved for the sign
    static const GLuint max_bits = sizeof(GLhalfxN64) * 8 - 1;

    if (bits > max_bits) {
        gl_set_error(GL_INVALID_VALUE, "Bits must not be greater than %ld", max_bits);
        return;
    }

    dst->precision = bits;
    dst->shift_amount = dst->target_precision - bits;
    dst->to_float_factor = 1.0f / (1<<bits);
}

void glVertexHalfFixedPrecisionN64(GLuint bits) { set_precision_bits(&state->vertex_halfx_precision, bits); }
void glTexCoordHalfFixedPrecisionN64(GLuint bits) { set_precision_bits(&state->texcoord_halfx_precision, bits); }

#define __RECT_IMPL(vertex, x1, y1, x2, y2) ({ \
    if (!gl_ensure_no_begin_end()) return; \
    glBegin(GL_POLYGON); \
    vertex(x1, y1); \
    vertex(x2, y1); \
    vertex(x2, y2); \
    vertex(x1, y2); \
    glEnd(); \
})

void glRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2)        { __RECT_IMPL(glVertex2s, x1, y1, x2, y2); }
void glRecti(GLint x1, GLint y1, GLint x2, GLint y2)                { __RECT_IMPL(glVertex2i, x1, y1, x2, y2); }
void glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)        { __RECT_IMPL(glVertex2f, x1, y1, x2, y2); }
void glRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)    { __RECT_IMPL(glVertex2d, x1, y1, x2, y2); }

void glRectsv(const GLshort *v1, const GLshort *v2)     { __RECT_IMPL(glVertex2s, v1[0], v1[1], v2[0], v2[1]); }
void glRectiv(const GLint *v1, const GLint *v2)         { __RECT_IMPL(glVertex2s, v1[0], v1[1], v2[0], v2[1]); }
void glRectfv(const GLfloat *v1, const GLfloat *v2)     { __RECT_IMPL(glVertex2s, v1[0], v1[1], v2[0], v2[1]); }
void glRectdv(const GLdouble *v1, const GLdouble *v2)   { __RECT_IMPL(glVertex2s, v1[0], v1[1], v2[0], v2[1]); }

void glPointSize(GLfloat size)
{
    if (!gl_ensure_no_begin_end()) return;
    
    if (size <= 0.0f) {
        gl_set_error(GL_INVALID_VALUE, "Point size must not be negative");
        return;
    }

    state->point_size = size;
    gl_set_short(GL_UPDATE_NONE, offsetof(gl_server_state_t, point_size), size*4);
}

void glLineWidth(GLfloat width)
{
    if (!gl_ensure_no_begin_end()) return;
    
    if (width <= 0.0f) {
        gl_set_error(GL_INVALID_VALUE, "Line width must not be negative");
        return;
    }

    state->line_width = width;
    gl_set_short(GL_UPDATE_NONE, offsetof(gl_server_state_t, line_width), width*4);
}

void glPolygonMode(GLenum face, GLenum mode)
{
    if (!gl_ensure_no_begin_end()) return;
    
    switch (face) {
    case GL_FRONT:
    case GL_BACK:
        assertf(0, "Separate polygon modes for front and back faces are not supported!");
        break;
    case GL_FRONT_AND_BACK:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid face target", face);
        return;
    }

    switch (mode) {
    case GL_POINT:
    case GL_LINE:
    case GL_FILL:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid polygon mode", mode);
        return;
    }

    gl_set_short(GL_UPDATE_NONE, offsetof(gl_server_state_t, polygon_mode), (uint16_t)mode);
    state->polygon_mode = mode;
    set_can_use_rsp_dirty();
}

void glDepthRange(GLclampd n, GLclampd f)
{
    if (!gl_ensure_no_begin_end()) return;

    state->current_viewport.n = n;
    state->current_viewport.f = f;
    
    state->current_viewport.scale[2] = (f - n) * 0.5f;
    state->current_viewport.offset[2] = n + (f - n) * 0.5f;

    gl_set_short(GL_UPDATE_NONE, 
        offsetof(gl_server_state_t, viewport_scale) + sizeof(int16_t) * 2, 
        state->current_viewport.scale[2] * 4);
    gl_set_short(GL_UPDATE_NONE, 
        offsetof(gl_server_state_t, viewport_offset) + sizeof(int16_t) * 2, 
        state->current_viewport.offset[2] * 4);
}

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    if (!gl_ensure_no_begin_end()) return;

    state->current_viewport.x = x;
    state->current_viewport.y = y;
    state->current_viewport.w = w;
    state->current_viewport.h = h;
    
    uint32_t fbh = state->color_buffer->height;

    state->current_viewport.scale[0] = w * 0.5f;
    state->current_viewport.scale[1] = h * -0.5f;
    state->current_viewport.offset[0] = x + w * 0.5f;
    state->current_viewport.offset[1] = fbh - y - h * 0.5f;

    // Screen coordinates are s13.2
    #define SCREEN_XY_SCALE   4.0f
    #define SCREEN_Z_SCALE    32767.0f

    // * 2.0f to compensate for RSP reciprocal missing 1 bit
    uint16_t scale_x = state->current_viewport.scale[0] * SCREEN_XY_SCALE * 2.0f;
    uint16_t scale_y = state->current_viewport.scale[1] * SCREEN_XY_SCALE * 2.0f;
    uint16_t scale_z = state->current_viewport.scale[2] * SCREEN_Z_SCALE * 2.0f;
    uint16_t offset_x = state->current_viewport.offset[0] * SCREEN_XY_SCALE;
    uint16_t offset_y = state->current_viewport.offset[1] * SCREEN_XY_SCALE;
    uint16_t offset_z = state->current_viewport.offset[2] * SCREEN_Z_SCALE;

    gl_set_long(GL_UPDATE_NONE, 
        offsetof(gl_server_state_t, viewport_scale), 
        ((uint64_t)scale_x << 48) | ((uint64_t)scale_y << 32) | ((uint64_t)scale_z << 16));
    gl_set_long(GL_UPDATE_NONE, 
        offsetof(gl_server_state_t, viewport_offset), 
        ((uint64_t)offset_x << 48) | ((uint64_t)offset_y << 32) | ((uint64_t)offset_z << 16));
}

gl_tex_gen_t *gl_get_tex_gen(GLenum coord)
{
    switch (coord) {
    case GL_S:
        return &state->tex_gen[0];
    case GL_T:
        return &state->tex_gen[1];
    case GL_R:
        return &state->tex_gen[2];
    case GL_Q:
        return &state->tex_gen[3];
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid tex gen coordinate", coord);
        return NULL;
    }
}

void gl_tex_gen_set_mode(gl_tex_gen_t *gen, GLenum coord, GLint param)
{
    switch (param) {
    case GL_OBJECT_LINEAR:
    case GL_EYE_LINEAR:
        break;
    case GL_SPHERE_MAP:
        if (coord == GL_R || coord == GL_Q) {
            gl_set_error(GL_INVALID_ENUM, "Sphere mapping can only be applied to S or T coordinates");
            return;
        }
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid tex gen mode", param);
        return;
    }

    uint32_t coord_offset = (coord & 0x3) * sizeof(uint16_t);

    gl_set_short(GL_UPDATE_NONE, offsetof(gl_server_state_t, tex_gen) + offsetof(gl_tex_gen_soa_t, mode) + coord_offset, param);
    gen->mode = param;
    
    set_can_use_rsp_dirty();
}

void gl_tex_gen_i(GLenum coord, GLenum pname, GLint param)
{
    gl_tex_gen_t *gen = gl_get_tex_gen(coord);
    if (gen == NULL) {
        return;
    }

    if (pname != GL_TEXTURE_GEN_MODE) {
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }

    gl_tex_gen_set_mode(gen, coord, param);
}

void glTexGeni(GLenum coord, GLenum pname, GLint param)
{
    if (!gl_ensure_no_begin_end()) return;
    gl_tex_gen_i(coord, pname, param);
}

void glTexGenf(GLenum coord, GLenum pname, GLfloat param)
{
    if (!gl_ensure_no_begin_end()) return;
    gl_tex_gen_i(coord, pname, param);
}

void glTexGend(GLenum coord, GLenum pname, GLdouble param)
{
    if (!gl_ensure_no_begin_end()) return;
    gl_tex_gen_i(coord, pname, param);
}

void gl_tex_gen_set_plane(GLenum coord, GLenum pname, const GLfloat *plane)
{
    uint32_t plane_offset = (pname - GL_OBJECT_PLANE) * TEX_GEN_COUNT * sizeof(uint16_t);
    uint32_t gen_offset = (coord & 0x3) * sizeof(uint16_t);
    uint32_t offset = offsetof(gl_server_state_t, tex_gen) + plane_offset + gen_offset;

    uint32_t coord_size = TEX_GEN_COUNT * TEX_GEN_PLANE_COUNT * sizeof(uint16_t);

    for (uint32_t i = 0; i < TEX_COORD_COUNT; i++)
    {
        int32_t fixed = plane[i] * (1 << 16);
        uint16_t integer = (fixed & 0xFFFF0000) >> 16;
        uint16_t fraction = fixed & 0x0000FFFF;

        uint32_t coord_offset = offset + coord_size * i;

        gl_set_short(GL_UPDATE_NONE, coord_offset + offsetof(gl_tex_gen_soa_t, integer), integer);
        gl_set_short(GL_UPDATE_NONE, coord_offset + offsetof(gl_tex_gen_soa_t, fraction), fraction);
    }
}

void glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params)
{
    if (!gl_ensure_no_begin_end()) return;

    gl_tex_gen_t *gen = gl_get_tex_gen(coord);
    if (gen == NULL) {
        return;
    }

    switch (pname) {
    case GL_TEXTURE_GEN_MODE:
        gl_tex_gen_set_mode(gen, coord, params[0]);
        break;
    case GL_OBJECT_PLANE:
        gen->object_plane[0] = params[0];
        gen->object_plane[1] = params[1];
        gen->object_plane[2] = params[2];
        gen->object_plane[3] = params[3];
        gl_tex_gen_set_plane(coord, pname, gen->object_plane);
        break;
    case GL_EYE_PLANE:
        gen->eye_plane[0] = params[0];
        gen->eye_plane[1] = params[1];
        gen->eye_plane[2] = params[2];
        gen->eye_plane[3] = params[3];
        gl_tex_gen_set_plane(coord, pname, gen->eye_plane);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void glTexGeniv(GLenum coord, GLenum pname, const GLint *params)
{
    if (!gl_ensure_no_begin_end()) return;

    gl_tex_gen_t *gen = gl_get_tex_gen(coord);
    if (gen == NULL) {
        return;
    }

    switch (pname) {
    case GL_TEXTURE_GEN_MODE:
        gl_tex_gen_set_mode(gen, coord, params[0]);
        break;
    case GL_OBJECT_PLANE:
        gen->object_plane[0] = params[0];
        gen->object_plane[1] = params[1];
        gen->object_plane[2] = params[2];
        gen->object_plane[3] = params[3];
        gl_tex_gen_set_plane(coord, pname, gen->object_plane);
        break;
    case GL_EYE_PLANE:
        gen->eye_plane[0] = params[0];
        gen->eye_plane[1] = params[1];
        gen->eye_plane[2] = params[2];
        gen->eye_plane[3] = params[3];
        gl_tex_gen_set_plane(coord, pname, gen->eye_plane);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void glTexGendv(GLenum coord, GLenum pname, const GLdouble *params)
{
    if (!gl_ensure_no_begin_end()) return;

    gl_tex_gen_t *gen = gl_get_tex_gen(coord);
    if (gen == NULL) {
        return;
    }

    switch (pname) {
    case GL_TEXTURE_GEN_MODE:
        gl_tex_gen_set_mode(gen, coord, params[0]);
        break;
    case GL_OBJECT_PLANE:
        gen->object_plane[0] = params[0];
        gen->object_plane[1] = params[1];
        gen->object_plane[2] = params[2];
        gen->object_plane[3] = params[3];
        gl_tex_gen_set_plane(coord, pname, gen->object_plane);
        break;
    case GL_EYE_PLANE:
        gen->eye_plane[0] = params[0];
        gen->eye_plane[1] = params[1];
        gen->eye_plane[2] = params[2];
        gen->eye_plane[3] = params[3];
        gl_tex_gen_set_plane(coord, pname, gen->eye_plane);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void glCullFace(GLenum mode)
{
    if (!gl_ensure_no_begin_end()) return;

    switch (mode) {
    case GL_BACK:
    case GL_FRONT:
    case GL_FRONT_AND_BACK:
        state->cull_face_mode = mode;
        gl_set_short(GL_UPDATE_NONE, offsetof(gl_server_state_t, cull_mode), mode);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid face culling mode", mode);
        return;
    }
}

void glFrontFace(GLenum dir)
{
    if (!gl_ensure_no_begin_end()) return;

    switch (dir) {
    case GL_CW:
    case GL_CCW:
        state->front_face = dir;
        gl_set_short(GL_UPDATE_NONE, offsetof(gl_server_state_t, front_face), dir);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid front face winding direction", dir);
        return;
    }
}
