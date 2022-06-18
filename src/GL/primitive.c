#include "gl_internal.h"
#include "utils.h"
#include "rdpq.h"

extern gl_state_t state;

static const float clip_planes[CLIPPING_PLANE_COUNT][4] = {
    { 1, 0, 0, 1 },
    { 0, 1, 0, 1 },
    { 0, 0, 1, 1 },
    { 1, 0, 0, -1 },
    { 0, 1, 0, -1 },
    { 0, 0, 1, -1 },
};

void glBegin(GLenum mode)
{
    if (state.immediate_mode) {
        gl_set_error(GL_INVALID_OPERATION);
        return;
    }

    switch (mode) {
    case GL_TRIANGLES:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
        state.immediate_mode = mode;
        state.next_vertex = 0;
        state.triangle_progress = 0;
        state.triangle_counter = 0;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    if (gl_is_invisible()) {
        return;
    }

    gl_update_scissor();
    gl_update_render_mode();
    gl_update_texture();
}

void glEnd(void)
{
    if (!state.immediate_mode) {
        gl_set_error(GL_INVALID_OPERATION);
    }

    state.immediate_mode = 0;
}

void gl_draw_triangle(gl_vertex_t *v0, gl_vertex_t *v1, gl_vertex_t *v2)
{
    if (state.cull_face_mode == GL_FRONT_AND_BACK) {
        return;
    }

    if (state.cull_face)
    {
        float winding = v0->screen_pos[0] * (v1->screen_pos[1] - v2->screen_pos[1]) +
                        v1->screen_pos[0] * (v2->screen_pos[1] - v0->screen_pos[1]) +
                        v2->screen_pos[0] * (v0->screen_pos[1] - v1->screen_pos[1]);
        
        bool is_front = (state.front_face == GL_CCW) ^ (winding > 0.0f);
        GLenum face = is_front ? GL_FRONT : GL_BACK;

        if (state.cull_face_mode == face) {
            return;
        }
    }

    int32_t tex_offset = state.texture_2d ? 6 : -1;
    int32_t z_offset = state.depth_test ? 9 : -1;

    rdpq_triangle(0, 0, 0, 2, tex_offset, z_offset, v0->screen_pos, v1->screen_pos, v2->screen_pos);
}

float dot_product4(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

void gl_vertex_calc_screenspace(gl_vertex_t *v)
{
    float inverse_w = 1.0f / v->position[3];

    v->screen_pos[0] = v->position[0] * inverse_w * state.current_viewport.scale[0] + state.current_viewport.offset[0];
    v->screen_pos[1] = v->position[1] * inverse_w * state.current_viewport.scale[1] + state.current_viewport.offset[1];

    v->depth = v->position[2] * inverse_w * state.current_viewport.scale[2] + state.current_viewport.offset[2];

    v->inverse_w = inverse_w;

    v->clip = 0;
    for (uint32_t i = 0; i < 3; i++)
    {
        if (v->position[i] < - v->position[3]) {
            v->clip |= 1 << i;
        } else if (v->position[i] > v->position[3]) {
            v->clip |= 1 << (i + 3);
        }
    }
}

void gl_clip_triangle(gl_vertex_t *v0, gl_vertex_t *v1, gl_vertex_t *v2)
{
    if (v0->clip & v1->clip & v2->clip) {
        return;
    }

    uint8_t any_clip = v0->clip | v1->clip | v2->clip;

    if (!any_clip) {
        gl_draw_triangle(v0, v1, v2);
        return;
    }

    // Polygon clipping using the Sutherland-Hodgman algorithm
    // See https://en.wikipedia.org/wiki/Sutherland%E2%80%93Hodgman_algorithm

    // Intersection points are stored in the clipping cache
    gl_vertex_t clipping_cache[CLIPPING_CACHE_SIZE];
    uint32_t cache_used = 0;

    gl_clipping_list_t lists[2];

    gl_clipping_list_t *in_list = &lists[0];
    gl_clipping_list_t *out_list = &lists[1];

    out_list->vertices[0] = v0;
    out_list->vertices[1] = v1;
    out_list->vertices[2] = v2;
    out_list->count = 3;
    
    for (uint32_t c = 0; c < CLIPPING_PLANE_COUNT; c++)
    {
        // If nothing clips this plane, skip it entirely
        if ((any_clip & (1<<c)) == 0) {
            continue;
        }

        const float *clip_plane = clip_planes[c];

        SWAP(in_list, out_list);
        out_list->count = 0;

        uint32_t cache_unused = 0;

        for (uint32_t i = 0; i < in_list->count; i++)
        {
            gl_vertex_t *cur_point = in_list->vertices[i];
            gl_vertex_t *prev_point = in_list->vertices[(i + in_list->count - 1) % in_list->count];

            bool cur_inside = (cur_point->clip & (1<<c)) == 0;
            bool prev_inside = (prev_point->clip & (1<<c)) == 0;

            if (cur_inside ^ prev_inside) {
                gl_vertex_t *intersection = NULL;

                for (uint32_t n = 0; n < CLIPPING_CACHE_SIZE; n++)
                {
                    if ((cache_used & (1<<n)) == 0) {
                        intersection = &clipping_cache[n];
                        cache_used |= (1<<n);
                        break;
                    }
                }

                assertf(intersection, "clipping cache full!");
                assertf(intersection != cur_point, "invalid intersection");
                assertf(intersection != prev_point, "invalid intersection");

                float d0 = dot_product4(prev_point->position, clip_plane);
                float d1 = dot_product4(cur_point->position, clip_plane);
                
                float a = d0 / (d0 - d1);

                assertf(a >= 0.f && a <= 1.f, "invalid a: %f", a);

                intersection->position[0] = lerp(prev_point->position[0], cur_point->position[0], a);
                intersection->position[1] = lerp(prev_point->position[1], cur_point->position[1], a);
                intersection->position[2] = lerp(prev_point->position[2], cur_point->position[2], a);
                intersection->position[3] = lerp(prev_point->position[3], cur_point->position[3], a);

                gl_vertex_calc_screenspace(intersection);

                intersection->color[0] = lerp(prev_point->color[0], cur_point->color[0], a);
                intersection->color[1] = lerp(prev_point->color[1], cur_point->color[1], a);
                intersection->color[2] = lerp(prev_point->color[2], cur_point->color[2], a);
                intersection->color[3] = lerp(prev_point->color[3], cur_point->color[3], a);

                intersection->texcoord[0] = lerp(prev_point->texcoord[0], cur_point->texcoord[0], a);
                intersection->texcoord[1] = lerp(prev_point->texcoord[1], cur_point->texcoord[1], a);

                out_list->vertices[out_list->count++] = intersection;
            }

            if (cur_inside) {
                out_list->vertices[out_list->count++] = cur_point;
            } else {
                // If the point is in the clipping cache, remember it as unused
                uint32_t diff = cur_point - clipping_cache;
                if (diff >= 0 && diff < CLIPPING_CACHE_SIZE) {
                    cache_unused |= (1<<diff);
                }
            }
        }

        // Mark all points that were discarded as unused
        cache_used &= ~cache_unused;
    }

    for (uint32_t i = 2; i < out_list->count; i++)
    {
        gl_draw_triangle(out_list->vertices[0], out_list->vertices[i-1], out_list->vertices[i]);
    }
}

void gl_vertex_cache_changed()
{
    if (state.triangle_progress < 3) {
        return;
    }

    gl_vertex_t *v0 = &state.vertex_cache[state.triangle_indices[0]];
    gl_vertex_t *v1 = &state.vertex_cache[state.triangle_indices[1]];
    gl_vertex_t *v2 = &state.vertex_cache[state.triangle_indices[2]];

    switch (state.immediate_mode) {
    case GL_TRIANGLES:
        state.triangle_progress = 0;
        break;
    case GL_TRIANGLE_STRIP:
        state.triangle_progress = 2;
        state.triangle_indices[state.triangle_counter % 2] = state.triangle_indices[2];
        break;
    case GL_TRIANGLE_FAN:
        state.triangle_progress = 2;
        state.triangle_indices[1] = state.triangle_indices[2];
        break;
    }

    state.triangle_counter++;

    // Flat shading
    if (state.shade_model == GL_FLAT) {
        v0->color[0] = v1->color[0] = v2->color[0];
        v0->color[1] = v1->color[1] = v2->color[1];
        v0->color[2] = v1->color[2] = v2->color[2];
        v0->color[3] = v1->color[3] = v2->color[3];
    }

    gl_clip_triangle(v0, v1, v2);
}

void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) 
{
    if (gl_is_invisible()) {
        return;
    }

    gl_vertex_t *v = &state.vertex_cache[state.next_vertex];

    GLfloat tmp[] = {x, y, z, w};

    if (state.lighting) {
        // TODO: Back face material?
        gl_perform_lighting(v->color, tmp, &state.materials[0]);
    } else {
        v->color[0] = state.current_color[0];
        v->color[1] = state.current_color[1];
        v->color[2] = state.current_color[2];
        v->color[3] = state.current_color[3];
    }

    v->color[0] = CLAMP01(v->color[0]) * 255.f;
    v->color[1] = CLAMP01(v->color[1]) * 255.f;
    v->color[2] = CLAMP01(v->color[2]) * 255.f;
    v->color[3] = CLAMP01(v->color[3]) * 255.f;

    gl_matrix_mult(v->position, &state.final_matrix, tmp);
    gl_vertex_calc_screenspace(v);

    if (state.texture_2d) {
        v->texcoord[0] = state.current_texcoord[0] * state.texture_2d_object.width;
        v->texcoord[1] = state.current_texcoord[1] * state.texture_2d_object.height;

        if (state.texture_2d_object.mag_filter == GL_LINEAR) {
            v->texcoord[0] -= 0.5f;
            v->texcoord[1] -= 0.5f;
        }

        v->texcoord[0] *= 32.f;
        v->texcoord[1] *= 32.f;
    }

    state.triangle_indices[state.triangle_progress] = state.next_vertex;

    state.next_vertex = (state.next_vertex + 1) % 3;
    state.triangle_progress++;

    gl_vertex_cache_changed();
}

void glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w)     { glVertex4f(x, y, z, w); }
void glVertex4i(GLint x, GLint y, GLint z, GLint w)             { glVertex4f(x, y, z, w); }
void glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) { glVertex4f(x, y, z, w); }

void glVertex3f(GLfloat x, GLfloat y, GLfloat z)    { glVertex4f(x, y, z, 1); }
void glVertex3s(GLshort x, GLshort y, GLshort z)    { glVertex3f(x, y, z); }
void glVertex3i(GLint x, GLint y, GLint z)          { glVertex3f(x, y, z); }
void glVertex3d(GLdouble x, GLdouble y, GLdouble z) { glVertex3f(x, y, z); }

void glVertex2f(GLfloat x, GLfloat y)   { glVertex4f(x, y, 0, 1); }
void glVertex2s(GLshort x, GLshort y)   { glVertex2f(x, y); }
void glVertex2i(GLint x, GLint y)       { glVertex2f(x, y); }
void glVertex2d(GLdouble x, GLdouble y) { glVertex2f(x, y); }

void glVertex2sv(const GLshort *v)  { glVertex2s(v[0], v[1]); }
void glVertex2iv(const GLint *v)    { glVertex2i(v[0], v[1]); }
void glVertex2fv(const GLfloat *v)  { glVertex2f(v[0], v[1]); }
void glVertex2dv(const GLdouble *v) { glVertex2d(v[0], v[1]); }

void glVertex3sv(const GLshort *v)  { glVertex3s(v[0], v[1], v[2]); }
void glVertex3iv(const GLint *v)    { glVertex3i(v[0], v[1], v[2]); }
void glVertex3fv(const GLfloat *v)  { glVertex3f(v[0], v[1], v[2]); }
void glVertex3dv(const GLdouble *v) { glVertex3d(v[0], v[1], v[2]); }

void glVertex4sv(const GLshort *v)  { glVertex4s(v[0], v[1], v[2], v[3]); }
void glVertex4iv(const GLint *v)    { glVertex4i(v[0], v[1], v[2], v[3]); }
void glVertex4fv(const GLfloat *v)  { glVertex4f(v[0], v[1], v[2], v[3]); }
void glVertex4dv(const GLdouble *v) { glVertex4d(v[0], v[1], v[2], v[3]); }

void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    state.current_color[0] = r;
    state.current_color[1] = g;
    state.current_color[2] = b;
    state.current_color[3] = a;
}

void glColor4d(GLdouble r, GLdouble g, GLdouble b, GLdouble a)  { glColor4f(r, g, b, a); }
void glColor4b(GLbyte r, GLbyte g, GLbyte b, GLbyte a)          { glColor4f(I8_TO_FLOAT(r), I8_TO_FLOAT(g), I8_TO_FLOAT(b), I8_TO_FLOAT(a)); }
void glColor4s(GLshort r, GLshort g, GLshort b, GLshort a)      { glColor4f(I16_TO_FLOAT(r), I16_TO_FLOAT(g), I16_TO_FLOAT(b), I16_TO_FLOAT(a)); }
void glColor4i(GLint r, GLint g, GLint b, GLint a)              { glColor4f(I32_TO_FLOAT(r), I32_TO_FLOAT(g), I32_TO_FLOAT(b), I32_TO_FLOAT(a)); }
void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)     { glColor4f(U8_TO_FLOAT(r), U8_TO_FLOAT(g), U8_TO_FLOAT(b), U8_TO_FLOAT(a)); }
void glColor4us(GLushort r, GLushort g, GLushort b, GLushort a) { glColor4f(U16_TO_FLOAT(r), U16_TO_FLOAT(g), U16_TO_FLOAT(b), U16_TO_FLOAT(a)); }
void glColor4ui(GLuint r, GLuint g, GLuint b, GLuint a)         { glColor4f(U32_TO_FLOAT(r), U32_TO_FLOAT(g), U32_TO_FLOAT(b), U32_TO_FLOAT(a)); }

void glColor3f(GLfloat r, GLfloat g, GLfloat b)     { glColor4f(r, g, b, 1.f); }
void glColor3d(GLdouble r, GLdouble g, GLdouble b)  { glColor3f(r, g, b); }
void glColor3b(GLbyte r, GLbyte g, GLbyte b)        { glColor3f(I8_TO_FLOAT(r), I8_TO_FLOAT(g), I8_TO_FLOAT(b)); }
void glColor3s(GLshort r, GLshort g, GLshort b)     { glColor3f(I16_TO_FLOAT(r), I16_TO_FLOAT(g), I16_TO_FLOAT(b)); }
void glColor3i(GLint r, GLint g, GLint b)           { glColor3f(I32_TO_FLOAT(r), I32_TO_FLOAT(g), I32_TO_FLOAT(b)); }
void glColor3ub(GLubyte r, GLubyte g, GLubyte b)    { glColor3f(U8_TO_FLOAT(r), U8_TO_FLOAT(g), U8_TO_FLOAT(b)); }
void glColor3us(GLushort r, GLushort g, GLushort b) { glColor3f(U16_TO_FLOAT(r), U16_TO_FLOAT(g), U16_TO_FLOAT(b)); }
void glColor3ui(GLuint r, GLuint g, GLuint b)       { glColor3f(U32_TO_FLOAT(r), U32_TO_FLOAT(g), U32_TO_FLOAT(b)); }

void glColor3bv(const GLbyte *v)    { glColor3b(v[0], v[1], v[2]); }
void glColor3sv(const GLshort *v)   { glColor3s(v[0], v[1], v[2]); }
void glColor3iv(const GLint *v)     { glColor3i(v[0], v[1], v[2]); }
void glColor3fv(const GLfloat *v)   { glColor3f(v[0], v[1], v[2]); }
void glColor3dv(const GLdouble *v)  { glColor3d(v[0], v[1], v[2]); }
void glColor3ubv(const GLubyte *v)  { glColor3ub(v[0], v[1], v[2]); }
void glColor3usv(const GLushort *v) { glColor3us(v[0], v[1], v[2]); }
void glColor3uiv(const GLuint *v)   { glColor3ui(v[0], v[1], v[2]); }

void glColor4bv(const GLbyte *v)    { glColor4b(v[0], v[1], v[2], v[3]); }
void glColor4sv(const GLshort *v)   { glColor4s(v[0], v[1], v[2], v[3]); }
void glColor4iv(const GLint *v)     { glColor4i(v[0], v[1], v[2], v[3]); }
void glColor4fv(const GLfloat *v)   { glColor4f(v[0], v[1], v[2], v[3]); }
void glColor4dv(const GLdouble *v)  { glColor4d(v[0], v[1], v[2], v[3]); }
void glColor4ubv(const GLubyte *v)  { glColor4ub(v[0], v[1], v[2], v[3]); }
void glColor4usv(const GLushort *v) { glColor4us(v[0], v[1], v[2], v[3]); }
void glColor4uiv(const GLuint *v)   { glColor4ui(v[0], v[1], v[2], v[3]); }

void glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
    state.current_texcoord[0] = s;
    state.current_texcoord[1] = t;
    state.current_texcoord[2] = r;
    state.current_texcoord[3] = q;
}

void glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q)       { glTexCoord4f(s, t, r, q); }
void glTexCoord4i(GLint s, GLint t, GLint r, GLint q)               { glTexCoord4f(s, t, r, q); }
void glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q)   { glTexCoord4f(s, t, r, q); }

void glTexCoord3f(GLfloat s, GLfloat t, GLfloat r)      { glTexCoord4f(s, t, r, 1.0f); }
void glTexCoord3s(GLshort s, GLshort t, GLshort r)      { glTexCoord3f(s, t, r); }
void glTexCoord3i(GLint s, GLint t, GLint r)            { glTexCoord3f(s, t, r); }
void glTexCoord3d(GLdouble s, GLdouble t, GLdouble r)   { glTexCoord3f(s, t, r); }

void glTexCoord2f(GLfloat s, GLfloat t)     { glTexCoord4f(s, t, 0.0f, 1.0f); }
void glTexCoord2s(GLshort s, GLshort t)     { glTexCoord2f(s, t); }
void glTexCoord2i(GLint s, GLint t)         { glTexCoord2f(s, t); }
void glTexCoord2d(GLdouble s, GLdouble t)   { glTexCoord2f(s, t); }

void glTexCoord1f(GLfloat s)    { glTexCoord4f(s, 0.0f, 0.0f, 1.0f); }
void glTexCoord1s(GLshort s)    { glTexCoord1f(s); }
void glTexCoord1i(GLint s)      { glTexCoord1f(s); }
void glTexCoord1d(GLdouble s)   { glTexCoord1f(s); }

void glTexCoord1sv(const GLshort *v)    { glTexCoord1s(v[0]); }
void glTexCoord1iv(const GLint *v)      { glTexCoord1i(v[0]); }
void glTexCoord1fv(const GLfloat *v)    { glTexCoord1f(v[0]); }
void glTexCoord1dv(const GLdouble *v)   { glTexCoord1d(v[0]); }

void glTexCoord2sv(const GLshort *v)    { glTexCoord2s(v[0], v[1]); }
void glTexCoord2iv(const GLint *v)      { glTexCoord2i(v[0], v[1]); }
void glTexCoord2fv(const GLfloat *v)    { glTexCoord2f(v[0], v[1]); }
void glTexCoord2dv(const GLdouble *v)   { glTexCoord2d(v[0], v[1]); }

void glTexCoord3sv(const GLshort *v)    { glTexCoord3s(v[0], v[1], v[2]); }
void glTexCoord3iv(const GLint *v)      { glTexCoord3i(v[0], v[1], v[2]); }
void glTexCoord3fv(const GLfloat *v)    { glTexCoord3f(v[0], v[1], v[2]); }
void glTexCoord3dv(const GLdouble *v)   { glTexCoord3d(v[0], v[1], v[2]); }

void glTexCoord4sv(const GLshort *v)    { glTexCoord4s(v[0], v[1], v[2], v[3]); }
void glTexCoord4iv(const GLint *v)      { glTexCoord4i(v[0], v[1], v[2], v[3]); }
void glTexCoord4fv(const GLfloat *v)    { glTexCoord4f(v[0], v[1], v[2], v[3]); }
void glTexCoord4dv(const GLdouble *v)   { glTexCoord4d(v[0], v[1], v[2], v[3]); }

void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
    state.current_normal[0] = nx;
    state.current_normal[1] = ny;
    state.current_normal[2] = nz;
}

void glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz)        { glNormal3f(I8_TO_FLOAT(nx), I8_TO_FLOAT(ny), I8_TO_FLOAT(nz)); }
void glNormal3s(GLshort nx, GLshort ny, GLshort nz)     { glNormal3f(I16_TO_FLOAT(nx), I16_TO_FLOAT(ny), I16_TO_FLOAT(nz)); }
void glNormal3i(GLint nx, GLint ny, GLint nz)           { glNormal3f(I32_TO_FLOAT(nx), I32_TO_FLOAT(ny), I32_TO_FLOAT(nz)); }
void glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz)  { glNormal3f(nx, ny, nz); }

void glNormal3bv(const GLbyte *v)   { glNormal3b(v[0], v[1], v[2]); }
void glNormal3sv(const GLshort *v)  { glNormal3s(v[0], v[1], v[2]); }
void glNormal3iv(const GLint *v)    { glNormal3i(v[0], v[1], v[2]); }
void glNormal3fv(const GLfloat *v)  { glNormal3f(v[0], v[1], v[2]); }
void glNormal3dv(const GLdouble *v) { glNormal3d(v[0], v[1], v[2]); }

void glDepthRange(GLclampd n, GLclampd f)
{
    state.current_viewport.scale[2] = ((f - n) * -0.5f) * 0x7FE0;
    state.current_viewport.offset[2] = (n + (f - n) * 0.5f) * 0x7FE0;
}

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    uint32_t fbh = state.cur_framebuffer->color_buffer->height;

    state.current_viewport.scale[0] = w * 0.5f;
    state.current_viewport.scale[1] = h * -0.5f;
    state.current_viewport.offset[0] = x + w * 0.5f;
    state.current_viewport.offset[1] = fbh - y - h * 0.5f;
}

void glCullFace(GLenum mode)
{
    switch (mode) {
    case GL_BACK:
    case GL_FRONT:
    case GL_FRONT_AND_BACK:
        state.cull_face_mode = mode;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glFrontFace(GLenum dir)
{
    switch (dir) {
    case GL_CW:
    case GL_CCW:
        state.front_face = dir;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}
