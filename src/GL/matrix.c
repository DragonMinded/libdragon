#include "gl_internal.h"
#include <string.h>

extern gl_state_t state;

void gl_matrix_init()
{
    state.modelview_stack = (gl_matrix_stack_t) {
        .storage = state.modelview_stack_storage,
        .size = MODELVIEW_STACK_SIZE,
    };

    state.projection_stack = (gl_matrix_stack_t) {
        .storage = state.projection_stack_storage,
        .size = PROJECTION_STACK_SIZE,
    };

    state.texture_stack = (gl_matrix_stack_t) {
        .storage = state.texture_stack_storage,
        .size = TEXTURE_STACK_SIZE,
    };

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

gl_matrix_t * gl_matrix_stack_get_matrix(gl_matrix_stack_t *stack)
{
    return &stack->storage[stack->cur_depth];
}

void gl_update_current_matrix()
{
    state.current_matrix = gl_matrix_stack_get_matrix(state.current_matrix_stack);
}

void gl_matrix_mult(GLfloat *d, const gl_matrix_t *m, const GLfloat *v)
{
    d[0] = m->m[0][0] * v[0] + m->m[1][0] * v[1] + m->m[2][0] * v[2] + m->m[3][0] * v[3];
    d[1] = m->m[0][1] * v[0] + m->m[1][1] * v[1] + m->m[2][1] * v[2] + m->m[3][1] * v[3];
    d[2] = m->m[0][2] * v[0] + m->m[1][2] * v[1] + m->m[2][2] * v[2] + m->m[3][2] * v[3];
    d[3] = m->m[0][3] * v[0] + m->m[1][3] * v[1] + m->m[2][3] * v[2] + m->m[3][3] * v[3];
}

void gl_matrix_mult3x3(GLfloat *d, const gl_matrix_t *m, const GLfloat *v)
{
    d[0] = m->m[0][0] * v[0] + m->m[1][0] * v[1] + m->m[2][0] * v[2];
    d[1] = m->m[0][1] * v[0] + m->m[1][1] * v[1] + m->m[2][1] * v[2];
    d[2] = m->m[0][2] * v[0] + m->m[1][2] * v[1] + m->m[2][2] * v[2];
}

void gl_matrix_mult4x2(GLfloat *d, const gl_matrix_t *m, const GLfloat *v)
{
    d[0] = m->m[0][0] * v[0] + m->m[1][0] * v[1] + m->m[2][0] * v[2] + m->m[3][0] * v[3];
    d[1] = m->m[0][1] * v[0] + m->m[1][1] * v[1] + m->m[2][1] * v[2] + m->m[3][1] * v[3];
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
    if (state.final_matrix_dirty) {
        gl_matrix_mult_full(&state.final_matrix, gl_matrix_stack_get_matrix(&state.projection_stack), gl_matrix_stack_get_matrix(&state.modelview_stack));
        state.final_matrix_dirty = false;
    }
}

void glMatrixMode(GLenum mode)
{
    switch (mode) {
    case GL_MODELVIEW:
        state.current_matrix_stack = &state.modelview_stack;
        break;
    case GL_PROJECTION:
        state.current_matrix_stack = &state.projection_stack;
        break;
    case GL_TEXTURE:
        state.current_matrix_stack = &state.texture_stack;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    gl_set_short(GL_UPDATE_NONE, offsetof(gl_server_state_t, matrix_mode), mode);
    state.matrix_mode = mode;

    gl_update_current_matrix();
}

static inline void write_shorts(rspq_write_t *w, const uint16_t *s, uint32_t count)
{
    for (uint32_t i = 0; i < count; i += 2)
    {
        uint32_t packed = ((uint32_t)s[i] << 16) | (uint32_t)s[i+1];
        rspq_write_arg(w, packed);
    }
}

static inline void gl_matrix_write(rspq_write_t *w, const GLfloat *m)
{
    uint16_t integer[16];
    uint16_t fraction[16];

    for (uint32_t i = 0; i < 16; i++)
    {
        int32_t fixed = m[i] * (1<<16);
        integer[i] = (uint16_t)((fixed & 0xFFFF0000) >> 16);
        fraction[i] = (uint16_t)(fixed & 0x0000FFFF);
    }

    write_shorts(w, integer, 16);
    write_shorts(w, fraction, 16);
}

static inline void gl_matrix_load(const GLfloat *m, bool multiply)
{
    rspq_write_t w = rspq_write_begin(gl_overlay_id, GL_CMD_MATRIX_LOAD, 17);
    rspq_write_arg(&w, multiply ? 1 : 0);
    gl_matrix_write(&w, m);
    rspq_write_end(&w);
}

void glLoadMatrixf(const GLfloat *m)
{
    memcpy(state.current_matrix, m, sizeof(gl_matrix_t));
    state.final_matrix_dirty = true;
    gl_matrix_load(m, false);
}

void glLoadMatrixd(const GLdouble *m)
{
    for (size_t i = 0; i < 16; i++)
    {
        state.current_matrix->m[i/4][i%4] = m[i];
    }
    state.final_matrix_dirty = true;

    gl_matrix_load(state.current_matrix->m[0], false);
}

void glMultMatrixf(const GLfloat *m)
{
    gl_matrix_t tmp = *state.current_matrix;
    gl_matrix_mult_full(state.current_matrix, &tmp, (gl_matrix_t*)m);
    state.final_matrix_dirty = true;

    gl_matrix_load(m, true);
}

void glMultMatrixd(const GLdouble *m);

void glLoadIdentity(void)
{
    gl_matrix_t identity = (gl_matrix_t){ .m={
        {1,0,0,0},
        {0,1,0,0},
        {0,0,1,0},
        {0,0,0,1},
    }};

    glLoadMatrixf(identity.m[0]);
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    float a = angle * (M_PI / 180.0f);
    float c = cosf(a);
    float s = sinf(a);
    float ic = 1.f - c;

    float mag = sqrtf(x*x + y*y + z*z);
    x /= mag;
    y /= mag;
    z /= mag;

    float r00 = x*x*ic+c;
    float r10 = y*x*ic+z*s;
    float r20 = z*x*ic-y*s;
    float r01 = x*y*ic-z*s;
    float r11 = y*y*ic+c;
    float r21 = z*y*ic+x*s;
    float r02 = x*z*ic+y*s;
    float r12 = y*z*ic-x*s;
    float r22 = z*z*ic+c;

    gl_matrix_t tmp = *state.current_matrix;
    float* o = tmp.m[0];
    float* m = state.current_matrix->m[0];

    m[ 0] = o[ 0] * r00 + o[ 4] * r10 + o[ 8] * r20;
    m[ 1] = o[ 1] * r00 + o[ 5] * r10 + o[ 9] * r20;
    m[ 2] = o[ 2] * r00 + o[ 6] * r10 + o[10] * r20;
    m[ 3] = o[ 3] * r00 + o[ 7] * r10 + o[11] * r20;

    m[ 4] = o[ 0] * r01 + o[ 4] * r11 + o[ 8] * r21;
    m[ 5] = o[ 1] * r01 + o[ 5] * r11 + o[ 9] * r21;
    m[ 6] = o[ 2] * r01 + o[ 6] * r11 + o[10] * r21;
    m[ 7] = o[ 3] * r01 + o[ 7] * r11 + o[11] * r21;

    m[ 8] = o[ 0] * r02 + o[ 4] * r12 + o[ 8] * r22;
    m[ 9] = o[ 1] * r02 + o[ 5] * r12 + o[ 9] * r22;
    m[10] = o[ 2] * r02 + o[ 6] * r12 + o[10] * r22;
    m[11] = o[ 3] * r02 + o[ 7] * r12 + o[11] * r22;

    state.final_matrix_dirty = true;
}
void glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
    float* m = state.current_matrix->m[0];

    m[12] += m[0] * x + m[4] * y + m[ 8] * z;
    m[13] += m[1] * x + m[5] * y + m[ 9] * z;
    m[14] += m[2] * x + m[6] * y + m[10] * z;
    m[15] += m[3] * x + m[7] * y + m[11] * z;

    state.final_matrix_dirty = true;
}
void glTranslated(GLdouble x, GLdouble y, GLdouble z);

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
    float* m = state.current_matrix->m[0];

    m[0] *= x; m[1] *= x; m[2] *= x; m[3] *= x;
    m[4] *= y; m[5] *= y; m[6] *= y; m[7] *= y;
    m[8] *= z; m[9] *= z; m[10] *= z; m[11] *= z;

    state.final_matrix_dirty = true;
}
void glScaled(GLdouble x, GLdouble y, GLdouble z);

void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
    gl_matrix_t frustum = (gl_matrix_t){ .m={
        {(2*n)/(r-l), 0.f,           0.f,            0.f},
        {0.f,         (2.f*n)/(t-b), 0.f,            0.f},
        {(r+l)/(r-l), (t+b)/(t-b),   -(f+n)/(f-n),   -1.f},
        {0.f,         0.f,           -(2*f*n)/(f-n), 0.f},
    }};

    glMultMatrixf(frustum.m[0]);
}

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
    gl_matrix_stack_t *stack = state.current_matrix_stack;

    int32_t new_depth = stack->cur_depth + 1;
    if (new_depth >= stack->size) {
        gl_set_error(GL_STACK_OVERFLOW);
        return;
    }

    stack->cur_depth = new_depth;
    memcpy(&stack->storage[new_depth], &stack->storage[new_depth-1], sizeof(gl_matrix_t));

    gl_update_current_matrix();

    gl_write(GL_CMD_MATRIX_PUSH);
}

void glPopMatrix(void)
{
    gl_matrix_stack_t *stack = state.current_matrix_stack;

    int32_t new_depth = stack->cur_depth - 1;
    if (new_depth < 0) {
        gl_set_error(GL_STACK_UNDERFLOW);
        return;
    }

    stack->cur_depth = new_depth;

    gl_update_current_matrix();
    state.final_matrix_dirty = true;

    gl_write(GL_CMD_MATRIX_POP);
}
