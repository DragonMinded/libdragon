#include "GL/gl.h"
#include "rdpq.h"
#include "rspq.h"
#include "display.h"
#include "rdp.h"
#include "utils.h"
#include <string.h>
#include <math.h>

#define MODELVIEW_STACK_SIZE 32
#define PROJECTION_STACK_SIZE 2

#define CLAMP(x, min, max) (MIN(MAX((x), (min)), (max)))

#define CLAMPF_TO_BOOL(x)  ((x)!=0.0)

#define CLAMPF_TO_U8(x)  ((x)*0xFF)
#define CLAMPF_TO_I8(x)  ((x)*0x7F)
#define CLAMPF_TO_U16(x) ((x)*0xFFFF)
#define CLAMPF_TO_I16(x) ((x)*0x7FFF)
#define CLAMPF_TO_U32(x) ((x)*0xFFFFFFFF)
#define CLAMPF_TO_I32(x) ((x)*0x7FFFFFFF)

#define FLOAT_TO_U8(x)  (CLAMP((x), 0.f, 1.f)*0xFF)

#define U8_TO_FLOAT(x) ((x)/(float)(0xFF))
#define U16_TO_FLOAT(x) ((x)/(float)(0xFFFF))
#define U32_TO_FLOAT(x) ((x)/(float)(0xFFFFFFFF))
#define I8_TO_FLOAT(x) MAX((x)/(float)(0x7F),-1.f)
#define I16_TO_FLOAT(x) MAX((x)/(float)(0x7FFF),-1.f)
#define I32_TO_FLOAT(x) MAX((x)/(float)(0x7FFFFFFF),-1.f)


typedef struct {
    surface_t *color_buffer;
    // TODO
    //void *depth_buffer;
} framebuffer_t;

typedef struct {
    GLfloat position[4];
    GLfloat color[4];
} gl_vertex_t;

typedef struct {
    GLfloat m[4][4];
} gl_matrix_t;

typedef struct {
    GLfloat scale[3];
    GLfloat offset[3];
} gl_viewport_t;

typedef struct {
    gl_matrix_t *storage;
    int32_t size;
    int32_t cur_depth;
} gl_matrix_stack_t;

static framebuffer_t default_framebuffer;
static framebuffer_t *cur_framebuffer;
static GLenum current_error;
static GLenum immediate_mode;
static GLclampf clear_color[4];

static gl_vertex_t vertex_cache[3];
static uint32_t triangle_indices[3];
static uint32_t next_vertex;
static uint32_t triangle_progress;
static uint32_t triangle_counter;

static GLfloat current_color[4];

static gl_viewport_t current_viewport;

static GLenum matrix_mode = GL_MODELVIEW;
static gl_matrix_t final_matrix;
static gl_matrix_t *current_matrix;

static gl_matrix_t modelview_stack_storage[MODELVIEW_STACK_SIZE];
static gl_matrix_t projection_stack_storage[PROJECTION_STACK_SIZE];

static gl_matrix_stack_t modelview_stack = (gl_matrix_stack_t) {
    .storage = modelview_stack_storage,
    .size = MODELVIEW_STACK_SIZE,
    .cur_depth = 0,
};

static gl_matrix_stack_t projection_stack = (gl_matrix_stack_t) {
    .storage = projection_stack_storage,
    .size = PROJECTION_STACK_SIZE,
    .cur_depth = 0,
};

static gl_matrix_stack_t *current_matrix_stack;

#define assert_framebuffer() ({ \
    assertf(cur_framebuffer != NULL, "GL: No target is set!"); \
})

void gl_set_framebuffer(framebuffer_t *framebuffer)
{
    cur_framebuffer = framebuffer;
    glViewport(0, 0, framebuffer->color_buffer->width, framebuffer->color_buffer->height);
    rdpq_set_color_image_surface(cur_framebuffer->color_buffer);
}

void gl_set_default_framebuffer()
{
    display_context_t ctx;
    while (!(ctx = display_lock()));

    default_framebuffer.color_buffer = ctx;

    gl_set_framebuffer(&default_framebuffer);
}

gl_matrix_t * gl_matrix_stack_get_matrix(gl_matrix_stack_t *stack)
{
    return &stack->storage[stack->cur_depth];
}

void gl_update_current_matrix()
{
    current_matrix = gl_matrix_stack_get_matrix(current_matrix_stack);
}

void gl_matrix_mult(GLfloat *d, const gl_matrix_t *m, const GLfloat *v)
{
    d[0] = m->m[0][0] * v[0] + m->m[1][0] * v[1] + m->m[2][0] * v[2] + m->m[3][0] * v[3];
    d[1] = m->m[0][1] * v[0] + m->m[1][1] * v[1] + m->m[2][1] * v[2] + m->m[3][1] * v[3];
    d[2] = m->m[0][2] * v[0] + m->m[1][2] * v[1] + m->m[2][2] * v[2] + m->m[3][2] * v[3];
    d[3] = m->m[0][3] * v[0] + m->m[1][3] * v[1] + m->m[2][3] * v[2] + m->m[3][3] * v[3];
}

void gl_matrix_mult_full(gl_matrix_t *d, const gl_matrix_t *l, const gl_matrix_t *r)
{
    gl_matrix_mult(d->m[0], l, r->m[0]);
    gl_matrix_mult(d->m[1], l, r->m[1]);
    gl_matrix_mult(d->m[2], l, r->m[2]);
    gl_matrix_mult(d->m[3], l, r->m[3]);
}

void gl_update_final_matrix()
{
    gl_matrix_mult_full(&final_matrix, gl_matrix_stack_get_matrix(&projection_stack), gl_matrix_stack_get_matrix(&modelview_stack));
}

void gl_init()
{
    rdpq_init();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    rdpq_set_other_modes(0);
    gl_set_default_framebuffer();
    glDepthRange(0, 1);
}

void gl_close()
{
    rdpq_close();
}

GLenum glGetError(void)
{
    GLenum error = current_error;
    current_error = GL_NO_ERROR;
    return error;
}

void gl_set_error(GLenum error)
{
    current_error = error;
}

void gl_swap_buffers()
{
    rdpq_sync_full((void(*)(void*))display_show, default_framebuffer.color_buffer);
    rspq_flush();
    gl_set_default_framebuffer();
}

void glBegin(GLenum mode)
{
    if (immediate_mode) {
        gl_set_error(GL_INVALID_OPERATION);
        return;
    }

    switch (mode) {
    case GL_TRIANGLES:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
        immediate_mode = mode;
        next_vertex = 0;
        triangle_progress = 0;
        triangle_counter = 0;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    rdpq_set_other_modes(SOM_CYCLE_1);
    rdpq_set_combine_mode(Comb_Rgb(ZERO, ZERO, ZERO, SHADE) | Comb_Alpha(ZERO, ZERO, ZERO, ONE));
}

void glEnd(void)
{
    if (!immediate_mode) {
        gl_set_error(GL_INVALID_OPERATION);
    }

    immediate_mode = 0;
}

void gl_vertex_cache_changed()
{
    if (triangle_progress < 3) {
        return;
    }

    gl_vertex_t *v0 = &vertex_cache[triangle_indices[0]];
    gl_vertex_t *v1 = &vertex_cache[triangle_indices[1]];
    gl_vertex_t *v2 = &vertex_cache[triangle_indices[2]];

    rdpq_triangle_shade(
        v0->position[0], 
        v0->position[1], 
        v1->position[0], 
        v1->position[1], 
        v2->position[0], 
        v2->position[1],
        v0->color[0]*255.f,
        v0->color[1]*255.f,
        v0->color[2]*255.f,
        v1->color[0]*255.f,
        v1->color[1]*255.f,
        v1->color[2]*255.f,
        v2->color[0]*255.f,
        v2->color[1]*255.f,
        v2->color[2]*255.f);

    switch (immediate_mode)
    {
    case GL_TRIANGLES:
        triangle_progress = 0;
        break;
    case GL_TRIANGLE_STRIP:
        triangle_progress = 2;
        triangle_indices[triangle_counter % 2] = triangle_indices[2];
        break;
    case GL_TRIANGLE_FAN:
        triangle_progress = 2;
        triangle_indices[1] = triangle_indices[2];
        break;
    }

    triangle_counter++;
}

void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) 
{
    memcpy(vertex_cache[next_vertex].color, current_color, sizeof(current_color));

    GLfloat tmp[] = {x, y, z, w};
    GLfloat *pos = vertex_cache[next_vertex].position;

    gl_matrix_mult(pos, &final_matrix, tmp);

    pos[0] = (pos[0] / pos[3]) * current_viewport.scale[0] + current_viewport.offset[0];
    pos[1] = (pos[1] / pos[3]) * current_viewport.scale[1] + current_viewport.offset[1];
    pos[2] = (pos[2] / pos[3]) * current_viewport.scale[2] + current_viewport.offset[2];

    triangle_indices[triangle_progress] = next_vertex;

    next_vertex = (next_vertex + 1) % 3;
    triangle_progress++;

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
    current_color[0] = r;
    current_color[1] = g;
    current_color[2] = b;
    current_color[3] = a;
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

void glDepthRange(GLclampd n, GLclampd f)
{
    current_viewport.scale[2] = (f - n) * 0.5f;
    current_viewport.offset[2] = n + (f - n) * 0.5f;
}

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    current_viewport.scale[0] = w * 0.5f;
    current_viewport.scale[1] = h * 0.5f;
    current_viewport.offset[0] = x + w * 0.5f;
    current_viewport.offset[1] = y + h * 0.5f;
}

void glMatrixMode(GLenum mode)
{
    switch (mode)
    {
    case GL_MODELVIEW:
        current_matrix_stack = &modelview_stack;
        break;
    case GL_PROJECTION:
        current_matrix_stack = &projection_stack;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    matrix_mode = mode;

    gl_update_current_matrix();
}

void glLoadMatrixf(const GLfloat *m)
{
    memcpy(current_matrix, m, sizeof(gl_matrix_t));
    gl_update_final_matrix();
}

void glLoadMatrixd(const GLdouble *m)
{
    for (size_t i = 0; i < 16; i++)
    {
        current_matrix->m[i/4][i%4] = m[i];
    }
    gl_update_final_matrix();
}

void glMultMatrixf(const GLfloat *m)
{
    gl_matrix_t tmp = *current_matrix;
    gl_matrix_mult_full(current_matrix, &tmp, (gl_matrix_t*)m);
    gl_update_final_matrix();
}

void glMultMatrixd(const GLdouble *m);

void glLoadIdentity(void)
{
    *current_matrix = (gl_matrix_t){ .m={
        {1,0,0,0},
        {0,1,0,0},
        {0,0,1,0},
        {0,0,0,1},
    }};

    gl_update_final_matrix();
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    float c = cosf(angle);
    float s = sinf(angle);
    float ic = 1.f - c;

    float mag = sqrtf(x*x + y*y + z*z);
    x /= mag;
    y /= mag;
    z /= mag;

    gl_matrix_t rotation = (gl_matrix_t){ .m={
        {x*x*ic+c,   y*x*ic+z*s, z*x*ic-y*s, 0.f},
        {x*y*ic-z*s, y*y*ic+c,   z*y*ic+x*s, 0.f},
        {x*z*ic+y*s, y*z*ic-x*s, z*z*ic+c,   0.f},
        {0.f,        0.f,        0.f,        1.f},
    }};

    glMultMatrixf(rotation.m[0]);
}
void glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
    gl_matrix_t translation = (gl_matrix_t){ .m={
        {1.f, 0.f, 0.f, 0.f},
        {0.f, 1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f, 0.f},
        {x,   y,   z,   1.f},
    }};

    glMultMatrixf(translation.m[0]);
}
void glTranslated(GLdouble x, GLdouble y, GLdouble z);

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
    gl_matrix_t scale = (gl_matrix_t){ .m={
        {x,   0.f, 0.f, 0.f},
        {0.f, y,   0.f, 0.f},
        {0.f, 0.f, z,   0.f},
        {0.f, 0.f, 0.f, 1.f},
    }};

    glMultMatrixf(scale.m[0]);
}
void glScaled(GLdouble x, GLdouble y, GLdouble z);

void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);

void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
    gl_matrix_t ortho = (gl_matrix_t){ .m={
        {2.0f/(r-l),   0.f,          0.f,          0.f},
        {0.f,          2.0f/(t-b),   0.f,          0.f},
        {0.f,          0.f,          2.0f/(f-n),   0.f},
        {-(r+l)/(r-l), -(t+b)/(t-b), -(f+n)/(f-n), 1.f},
    }};

    glMultMatrixf(ortho.m[0]);
}

void glPushMatrix(void)
{
    int32_t new_depth = current_matrix_stack->cur_depth + 1;
    if (new_depth >= current_matrix_stack->size) {
        gl_set_error(GL_STACK_OVERFLOW);
        return;
    }

    current_matrix_stack->cur_depth = new_depth;
    memcpy(&current_matrix_stack->storage[new_depth], &current_matrix_stack->storage[new_depth-1], sizeof(gl_matrix_t));

    gl_update_current_matrix();
}

void glPopMatrix(void)
{
    int32_t new_depth = current_matrix_stack->cur_depth - 1;
    if (new_depth < 0) {
        gl_set_error(GL_STACK_UNDERFLOW);
        return;
    }

    current_matrix_stack->cur_depth = new_depth;

    gl_update_current_matrix();
}

void glScissor(GLint left, GLint bottom, GLsizei width, GLsizei height)
{
    rdpq_set_scissor(left, bottom, left + width, bottom + height);
}

void glClear(GLbitfield buf)
{
    assert_framebuffer();

    rdpq_set_cycle_mode(SOM_CYCLE_FILL);

    if (buf & GL_COLOR_BUFFER_BIT) {
        rdpq_set_fill_color(RGBA32(
            CLAMPF_TO_U8(clear_color[0]), 
            CLAMPF_TO_U8(clear_color[1]), 
            CLAMPF_TO_U8(clear_color[2]), 
            CLAMPF_TO_U8(clear_color[3])));
        rdpq_fill_rectangle(0, 0, cur_framebuffer->color_buffer->width, cur_framebuffer->color_buffer->height);
    }
}

void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a)
{
    clear_color[0] = r;
    clear_color[1] = g;
    clear_color[2] = b;
    clear_color[3] = a;
}

void glFlush(void)
{
    rspq_flush();
}

void glFinish(void)
{
    rspq_wait();
}

void glGetBooleanv(GLenum value, GLboolean *data)
{
    switch (value) {
    case GL_COLOR_CLEAR_VALUE:
        data[0] = CLAMPF_TO_BOOL(clear_color[0]);
        data[1] = CLAMPF_TO_BOOL(clear_color[1]);
        data[2] = CLAMPF_TO_BOOL(clear_color[2]);
        data[3] = CLAMPF_TO_BOOL(clear_color[3]);
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
        data[0] = CLAMPF_TO_I32(clear_color[0]);
        data[1] = CLAMPF_TO_I32(clear_color[1]);
        data[2] = CLAMPF_TO_I32(clear_color[2]);
        data[3] = CLAMPF_TO_I32(clear_color[3]);
        break;
    case GL_CURRENT_COLOR:
        data[0] = CLAMPF_TO_I32(current_color[0]);
        data[1] = CLAMPF_TO_I32(current_color[1]);
        data[2] = CLAMPF_TO_I32(current_color[2]);
        data[3] = CLAMPF_TO_I32(current_color[3]);
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
        data[0] = clear_color[0];
        data[1] = clear_color[1];
        data[2] = clear_color[2];
        data[3] = clear_color[3];
        break;
    case GL_CURRENT_COLOR:
        data[0] = current_color[0];
        data[1] = current_color[1];
        data[2] = current_color[2];
        data[3] = current_color[3];
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
        data[0] = clear_color[0];
        data[1] = clear_color[1];
        data[2] = clear_color[2];
        data[3] = clear_color[3];
        break;
    case GL_CURRENT_COLOR:
        data[0] = current_color[0];
        data[1] = current_color[1];
        data[2] = current_color[2];
        data[3] = current_color[3];
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
        return (GLubyte*)"";
    default:
        gl_set_error(GL_INVALID_ENUM);
        return NULL;
    }
}
