#include "gl_internal.h"
#include "rdpq_attach.h"

#include <limits.h>

void update_culling();

void gl_primitive_init()
{
    state->viewport.n = 0;
    state->viewport.f = 1;

    glNormal3f(0, 0, 1);
    glColor4f(1, 1, 1, 1);
    glTexCoord4f(0, 0, 0, 1);
    uint8_t index = 0;
    glMatrixIndexubvARB(1, &index);

    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
    glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
    glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);

    GLfloat s_plane[] = {1, 0, 0, 0};
    glTexGenfv(GL_S, GL_OBJECT_PLANE, s_plane);
    glTexGenfv(GL_S, GL_EYE_PLANE, s_plane);

    GLfloat t_plane[] = {0, 1, 0, 0};
    glTexGenfv(GL_T, GL_OBJECT_PLANE, t_plane);
    glTexGenfv(GL_T, GL_EYE_PLANE, t_plane);

    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    state->vertex_halfx_precision.target_precision = MGFX_VTX_POS_SHIFT;
    state->texcoord_halfx_precision.target_precision = MGFX_VTX_TEX_SHIFT;

    glVertexHalfFixedPrecisionN64(MGFX_VTX_POS_SHIFT);
    glTexCoordHalfFixedPrecisionN64(MGFX_VTX_TEX_SHIFT);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glLineWidth(1);
    glPointSize(1);

    vertex_layout_init(&state->begin_end_layout);
    vertex_layout_add(&state->begin_end_layout, MGFX_ATTRIBUTE_POS_NORM, offsetof(native_vertex_t, position), sizeof(int16_t)*4);
    vertex_layout_add(&state->begin_end_layout, MGFX_ATTRIBUTE_COLOR, offsetof(native_vertex_t, color), sizeof(uint32_t));
    vertex_layout_add(&state->begin_end_layout, MGFX_ATTRIBUTE_TEXCOORD, offsetof(native_vertex_t, texcoord), sizeof(int16_t)*2);
    state->begin_end_layout.vertex_layout.stride = sizeof(native_vertex_t);
}

void gl_primitive_close()
{
    if (state->begin_end_buffer.buffer != NULL) {
        ringbuffer_free(&state->begin_end_buffer);
    }
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
    case GL_POLYGON:
        return MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;

    case GL_POINTS:
    case GL_LINES:
    case GL_LINE_LOOP:
    case GL_LINE_STRIP:
    case GL_QUADS:
    case GL_QUAD_STRIP:
        assertf(0, "Draw mode %ld is not supported", mode);
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid primitive mode", mode);
        return -1;
    }
}

void update_geom_flags()
{
    if (!gl_check_and_clear_dirty_flags(DIRTY_GEOM_FLAGS)) return;

    mg_geometry_flags_t flags = 0;
    if (gl_is_shade_active()) flags |= MG_GEOMETRY_FLAGS_SHADE_ENABLED;
    if (gl_is_depth_active()) flags |= MG_GEOMETRY_FLAGS_Z_ENABLED;
    if (gl_is_texture_active()) flags |= MG_GEOMETRY_FLAGS_TEX_ENABLED;

    mg_set_geometry_flags(flags);
}

static void update_vertex_buffer(uint32_t first, uint32_t count)
{
    array_object_update(state->array_object, first, count);

    // It's possible that we are now accessing a sub-range of a previously cached buffer.
    // In that case we need to apply an offset, since the draw command expects the first vertex at offset 0.
    uint32_t buffer_offset = first - state->array_object->cached_first;
    mg_bind_vertex_buffer(((uint8_t*)state->array_object->buffer) + buffer_offset * state->array_object->layout.vertex_layout.stride);
}

static void update_uniforms()
{
    gl_upload_fog(state->fog_uniform);
    gl_upload_lighting(state->lighting_uniform);
    gl_upload_texturing(state->texturing_uniform);
    gl_upload_matrices(state->matrices_uniform);
}

static void prepare_drawing()
{
    update_active_texture();
    update_pipeline();
    update_uniforms();
    update_culling();
    update_z_planes();
    update_viewport();
    update_geom_flags();
    apply_texture();
    apply_rendermode();
}

static void prepare_draw_call(uint32_t first, uint32_t count)
{
    update_vertex_buffer(first, count);
    prepare_drawing();
}

bool is_drawing_anything()
{
    return !gl_is_enabled(ENABLE_CULL_FACE) || state->cull_face != GL_FRONT_AND_BACK;
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    if (!is_drawing_anything() || count == 0) return;
    
    prepare_draw_call(first, count);
    mg_draw_begin(); // TODO: detect if modes have actually changed to batch draw commands
    // TODO: record into block?
    mg_draw(&(mg_input_assembly_parms_t) {
        .primitive_topology = get_primitive_topology(mode)
    }, count, first);
    mg_draw_end();
}

static bool input_assembly_parms_equal(const mg_input_assembly_parms_t *lh, const mg_input_assembly_parms_t *rh)
{
    return lh->primitive_topology == rh->primitive_topology && lh->primitive_restart_enabled == rh->primitive_restart_enabled;
}

static void find_index_bounds(const uint16_t *indices, uint32_t count, uint16_t *min_index, uint16_t *max_index)
{
    uint16_t min = USHRT_MAX;
    uint16_t max = 0;

    for (size_t i = 0; i < count; i++)
    {
        if (indices[i] < min) min = indices[i];
        if (indices[i] > max) max = indices[i];
    }

    *min_index = min;
    *max_index = max;
}

static void update_element_array_cache(gl_buffer_object_t *element_buffer, uint32_t count, uint32_t offset, const mg_input_assembly_parms_t *input_assembly_parms, uint16_t *min_index, uint16_t *max_index)
{
    // TODO: throw INVALID OPERATION if buffer is currently mapped

    if (element_buffer->element_cache == NULL) {
        element_buffer->element_cache = calloc(1, sizeof(gl_element_array_cache_t));
    }

    const uint16_t *indices_i16 = (const uint16_t*)((const uint8_t*)element_buffer->storage.data + offset);

    gl_element_array_cache_t *cache = element_buffer->element_cache;
    bool is_dirty = false;
    if (cache->is_data_dirty || cache->count != count || cache->offset != offset) {
        find_index_bounds(indices_i16, count, &cache->min_index, &cache->max_index);
        cache->count = count;
        cache->offset = offset;
        is_dirty = true;
    }
    if (is_dirty || !input_assembly_parms_equal(&cache->parms, input_assembly_parms)) {
        cache->parms = *input_assembly_parms;

        if (cache->block != NULL) rspq_call_deferred((void(*)(void*))rspq_block_free, cache->block);

        rspq_block_begin();
        mg_draw_indexed(input_assembly_parms, indices_i16, count, -cache->min_index);
        cache->block = rspq_block_end();
        cache->is_data_dirty = false;
    }

    *min_index = cache->min_index;
    *max_index = cache->max_index;
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    assertf(type == GL_UNSIGNED_SHORT, "Index type must be GL_UNSIGNED_SHORT");

    if (!is_drawing_anything() || count == 0) return;

    uint16_t min_index, max_index;
    mg_input_assembly_parms_t input_assembly_parms = {
        .primitive_topology = get_primitive_topology(mode)
    };

    gl_buffer_object_t *element_buffer = state->array_object->element_array_buffer;
    if (element_buffer != NULL) {
        update_element_array_cache(element_buffer, count, (uint32_t)indices, &input_assembly_parms, &min_index, &max_index);
    } else {
        find_index_bounds(indices, count, &min_index, &max_index);
    }

    prepare_draw_call(min_index, max_index - min_index + 1);

    mg_draw_begin();
    if (element_buffer != NULL) {
        rspq_block_run(element_buffer->element_cache->block);
    } else {
        mg_draw_indexed(&input_assembly_parms, indices, count, -min_index);
    }
    mg_draw_end();
}

void begin_end_next_buffer()
{
    state->begin_end_current_buffer = ringbuffer_alloc_next(&state->begin_end_buffer);
    state->begin_end_index = 0;
}

native_vertex_t *begin_end_get_current_vertex()
{
    return state->begin_end_current_buffer + state->begin_end_index;
}

uint32_t get_begin_end_multiple(GLenum mode)
{
    switch (mode)
    {
    case GL_POINTS:
    case GL_LINE_LOOP:
    case GL_LINE_STRIP:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_POLYGON:
        return 1;
    case GL_LINES:
    case GL_QUAD_STRIP:
        return 2;
    case GL_TRIANGLES:
        return 3;
    case GL_QUADS:
        return 4;
    default:
        return 0;
    }
}

bool get_begin_end_need_save(GLenum mode)
{
    switch (mode)
    {
    case GL_LINE_LOOP:
    case GL_TRIANGLE_FAN:
    case GL_POLYGON:
        return true;
    case GL_POINTS:
    case GL_LINES:
    case GL_LINE_STRIP:
    case GL_TRIANGLES:
    case GL_TRIANGLE_STRIP:
    case GL_QUADS:
    case GL_QUAD_STRIP:
    default:
        return false;
    }
}

void set_begin_end_active(bool active)
{
    state->begin_end_active = active;
    gl_set_state(STATE_BEGIN_END);
}

void glBegin(GLenum mode)
{
    if (!gl_ensure_no_begin_end()) return;

    set_begin_end_active(true);
    state->begin_end_mode = mode;
    state->begin_end_topology = get_primitive_topology(mode);
    state->begin_end_multiple = get_begin_end_multiple(mode);
    state->begin_end_need_save = get_begin_end_need_save(mode);

    prepare_drawing();
    mg_draw_begin();

    if (state->begin_end_buffer.buffer == NULL) {
        ringbuffer_init(&state->begin_end_buffer, sizeof(native_vertex_t) * BEGIN_END_BUFFER_SIZE, BEGIN_END_BUFFER_COUNT);
    }

    begin_end_next_buffer();
    gl_update_array_pointers(state->array_object);
}

void begin_end_draw_current_buffer()
{
    if (is_drawing_anything()) {
        mg_bind_vertex_buffer(state->begin_end_current_buffer);
        mg_draw(&(mg_input_assembly_parms_t) {
            .primitive_topology = state->begin_end_topology
        }, state->begin_end_index, 0);
    }
    ringbuffer_release_current(&state->begin_end_buffer);
}

void glEnd(void)
{
    if (!state->begin_end_active) {
        gl_set_error(GL_INVALID_OPERATION, "glEnd must be called after glBegin");
    }

    // TODO: line loops will need special handling (insert saved vtx at the end)

    if (state->begin_end_index > 0) {
        begin_end_draw_current_buffer();
    }

    mg_draw_end();

    set_begin_end_active(false);
}

void begin_end_append_vtx(const native_vertex_t *vtx)
{
    memcpy(begin_end_get_current_vertex(), vtx, sizeof(native_vertex_t));
    state->begin_end_index++;
}

void begin_end_prep_next_buffer(const native_vertex_t *prev_end)
{
    // Appending these vertices is guaranteed to not overflow the buffer since we just started a fresh one
    switch (state->begin_end_mode) {
    case GL_TRIANGLE_STRIP:
    {
        // The two previous vertices
        begin_end_append_vtx(prev_end - 2);
        begin_end_append_vtx(prev_end - 1);
        break;
    }
    case GL_TRIANGLE_FAN:
    case GL_POLYGON:
    {
        // The "hub" of the fan
        begin_end_append_vtx(&state->begin_end_saved_vtx);
        // The previous vertex
        begin_end_append_vtx(prev_end - 1);
        break;
    }
    }
}

void begin_end_advance()
{
    begin_end_append_vtx(&state->current_attribs);

    // In some cases, we need to save the very first vertex for later (for example triangle fan, line loop)
    if (state->begin_end_need_save) {
        memcpy(&state->begin_end_saved_vtx, &state->current_attribs, sizeof(native_vertex_t));
        state->begin_end_need_save = false;
    }

    // Check if we have reached the required multiple of vertices and the next multiple would overflow the current buffer
    if (state->begin_end_index % state->begin_end_multiple == 0 && 
        state->begin_end_index + state->begin_end_multiple > BEGIN_END_BUFFER_SIZE) {
        begin_end_draw_current_buffer();
        native_vertex_t *prev_end = begin_end_get_current_vertex();
        begin_end_next_buffer();
        begin_end_prep_next_buffer(prev_end);
    }
}

void glArrayElement(GLint i)
{
    // Calling glArrayElement while the vertex array is enabled has, among other things,
    // the same effect as glVertex. See __gl_vertex for that function's behavior.
    assertf(!state->array_object->arrays[ATTRIB_VERTEX].enabled || state->begin_end_active, 
        "glArrayElement was called outside of glBegin/glEnd while vertex array was enabled");

    if (i < 0) {
        gl_set_error(GL_INVALID_VALUE, "Index must not be negative");
        return;
    }

    static const uint32_t out_offsets[ATTRIB_COUNT] = {
        offsetof(native_vertex_t, position),
        offsetof(native_vertex_t, normal),
        offsetof(native_vertex_t, color),
        offsetof(native_vertex_t, texcoord),
        offsetof(native_vertex_t, mtx_index),
    };
    array_convert(state->array_object, out_offsets, &state->current_attribs, i, 1, sizeof(native_vertex_t));
    if (state->array_object->arrays[ATTRIB_COLOR].enabled) {
        gl_set_state(STATE_COLOR);
    }

    if (state->array_object->arrays[ATTRIB_VERTEX].enabled) {
        begin_end_advance();
    }
}

void *get_attrib_dst(gl_array_type_t array_type)
{
    switch (array_type)
    {
    case ATTRIB_VERTEX:
        return state->current_attribs.position;
    case ATTRIB_NORMAL:
        return &state->current_attribs.normal;
    case ATTRIB_COLOR:
        return &state->current_attribs.color;
    case ATTRIB_TEXCOORD:
        return state->current_attribs.texcoord;
    case ATTRIB_MTX_INDEX:
        return state->current_attribs.mtx_index;
    default:
        return NULL;
    }
}

void read_attrib(gl_array_type_t array_type, GLenum type, const void *value, uint32_t size)
{
    read_attrib_func read_func = get_read_func(array_type, type);
    assertf(read_func != NULL, "Could not find read func");
    void *dst = get_attrib_dst(array_type);
    assertf(dst != NULL, "Array type not supported");

    read_func(dst, value, size);
}

void __gl_vertex(GLenum type, const void *value, uint32_t size)
{
    // According to the spec, calling glVertex outside of glBegin/glEnd 
    // specifically results in UB instead of generating an error, so just assert.
    assertf(state->begin_end_active, "glVertex was called outside of glBegin/glEnd");

    read_attrib(ATTRIB_VERTEX, type, value, size);
    begin_end_advance();
}

void __gl_normal(GLenum type, const void *value, uint32_t size)
{
    read_attrib(ATTRIB_NORMAL, type, value, size);
}

void __gl_color(GLenum type, const void *value, uint32_t size)
{
    read_attrib(ATTRIB_COLOR, type, value, size);
    gl_set_state(STATE_COLOR);
}

void __gl_tex_coord(GLenum type, const void *value, uint32_t size)
{
    read_attrib(ATTRIB_TEXCOORD, type, value, size);
}

void __gl_mtx_index(GLenum type, const void *value, uint32_t size)
{
    if (size > VERTEX_UNIT_COUNT) {
        gl_set_error(GL_INVALID_VALUE, "Size must not be greater than %d", VERTEX_UNIT_COUNT);
        return;
    }

    read_attrib(ATTRIB_MTX_INDEX, type, value, size);
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

void glMatrixIndexubvARB(GLint size, const GLubyte *v)  { __gl_mtx_index(GL_UNSIGNED_BYTE,  v, size); }
void glMatrixIndexusvARB(GLint size, const GLushort *v) { __gl_mtx_index(GL_UNSIGNED_SHORT, v, size); }
void glMatrixIndexuivARB(GLint size, const GLuint *v)   { __gl_mtx_index(GL_UNSIGNED_INT,   v, size); }

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
}

void glLineWidth(GLfloat width)
{
    if (!gl_ensure_no_begin_end()) return;
    
    if (width <= 0.0f) {
        gl_set_error(GL_INVALID_VALUE, "Line width must not be negative");
        return;
    }

    state->line_width = width;
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

    state->polygon_mode = mode;
}

void update_viewport()
{
    if (!gl_check_and_clear_dirty_flags(DIRTY_VIEWPORT)) return;

    const surface_t *fb = gl_require_color_buffer();

    mg_set_viewport(&(mg_viewport_t) {
        .x = state->viewport.x,
        .y = fb->height - state->viewport.y,
        .width = state->viewport.w,
        .height = -state->viewport.h,
        .minDepth = state->viewport.n,
        .maxDepth = state->viewport.f,
        .z_near = state->near_plane,
        .z_far = state->far_plane
    });
}

void glDepthRange(GLclampd n, GLclampd f)
{
    state->viewport.n = n;
    state->viewport.f = f;
    gl_set_state(STATE_DEPTH_RANGE);
}

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    state->viewport.x = x;
    state->viewport.y = y;
    state->viewport.w = w;
    state->viewport.h = h;
    gl_set_state(STATE_VIEWPORT);
}

mg_cull_mode_t get_cull_mode()
{
    if (!gl_is_enabled(ENABLE_CULL_FACE)) {
        return MG_CULL_MODE_NONE;
    }

    switch (state->cull_face)
    {
    case GL_BACK:
        return MG_CULL_MODE_BACK;
    case GL_FRONT:
        return MG_CULL_MODE_FRONT;
    default:
        return -1;
    }
}

mg_front_face_t get_front_face()
{
    switch (state->front_face)
    {
    case GL_CW:
        return MG_FRONT_FACE_CLOCKWISE;
    case GL_CCW:
        return MG_FRONT_FACE_COUNTER_CLOCKWISE;
    default:
        return -1;
    }
}

void update_culling()
{
    if (!gl_check_and_clear_dirty_flags(DIRTY_CULLING)) return;

    mg_set_culling(&(mg_culling_parms_t) {
        .cull_mode = get_cull_mode(),
        .front_face = get_front_face()
    });
}

void glCullFace(GLenum mode)
{
    if (!gl_ensure_no_begin_end()) return;

    switch (mode) {
    case GL_BACK:
    case GL_FRONT:
    case GL_FRONT_AND_BACK:
        state->cull_face = mode;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid face culling mode", mode);
        return;
    }

    gl_set_state(STATE_CULL_FACE);
}

void glFrontFace(GLenum dir)
{
    if (!gl_ensure_no_begin_end()) return;

    switch (dir) {
    case GL_CW:
    case GL_CCW:
        state->front_face = dir;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid front face winding direction", dir);
        return;
    }

    gl_set_state(STATE_CULL_FACE);
}

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

    gen->mode = param;
    gl_set_state(STATE_TEX_GEN);
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
        break;
    case GL_EYE_PLANE:
        gen->eye_plane[0] = params[0];
        gen->eye_plane[1] = params[1];
        gen->eye_plane[2] = params[2];
        gen->eye_plane[3] = params[3];
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
        break;
    case GL_EYE_PLANE:
        gen->eye_plane[0] = params[0];
        gen->eye_plane[1] = params[1];
        gen->eye_plane[2] = params[2];
        gen->eye_plane[3] = params[3];
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
        break;
    case GL_EYE_PLANE:
        gen->eye_plane[0] = params[0];
        gen->eye_plane[1] = params[1];
        gen->eye_plane[2] = params[2];
        gen->eye_plane[3] = params[3];
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

bool gl_is_env_map_enabled()
{
    return gl_is_enabled(ENABLE_TEX_GEN_S) && state->tex_gen[0].mode == GL_SPHERE_MAP
        && gl_is_enabled(ENABLE_TEX_GEN_T) && state->tex_gen[1].mode == GL_SPHERE_MAP;
}
