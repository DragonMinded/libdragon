/**
 * Some helper structs and functions for common vector/matrix operations.
 */

#ifndef VECTOR_HELPER_H
#define VECTOR_HELPER_H

#include <math.h>

typedef struct {
    float v[4];
} vec4_t;

typedef struct {
    float m[4][4];
} mtx4x4_t;

void matrix_identity(mtx4x4_t *dest)
{
    *dest = (mtx4x4_t){ .m={
        {1.f, 0.f, 0.f, 0.f},
        {0.f, 1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f, 0.f},
        {0.f, 0.f, 0.f, 1.f},
    }};
}

void matrix_translate(mtx4x4_t *dest, float x, float y, float z)
{
    *dest = (mtx4x4_t){ .m={
        {1.f, 0.f, 0.f, 0.f},
        {0.f, 1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f, 0.f},
        {x,   y,   z,   1.f},
    }};
}

void matrix_scale(mtx4x4_t *dest, float x, float y, float z)
{
    *dest = (mtx4x4_t){ .m={
        {x,   0.f, 0.f, 0.f},
        {0.f, y,   0.f, 0.f},
        {0.f, 0.f, z,   0.f},
        {0.f, 0.f, 0.f, 1.f},
    }};
}

void matrix_rotate_x(mtx4x4_t *dest, float angle)
{
    float c = cosf(angle);
    float s = sinf(angle);

    *dest = (mtx4x4_t){ .m={
        {1.f, 0.f, 0.f, 0.f},
        {0.f, c,   s,   0.f},
        {0.f, -s,  c,   0.f},
        {0.f, 0.f, 0.f, 1.f},
    }};
}

void matrix_rotate_y(mtx4x4_t *dest, float angle)
{
    float c = cosf(angle);
    float s = sinf(angle);

    *dest = (mtx4x4_t){ .m={
        {c,   0.f, -s,  0.f},
        {0.f, 1.f, 0.f, 0.f},
        {s,   0.f, c,   0.f},
        {0.f, 0.f, 0.f, 1.f},
    }};
}

void matrix_rotate_z(mtx4x4_t *dest, float angle)
{
    float c = cosf(angle);
    float s = sinf(angle);

    *dest = (mtx4x4_t){ .m={
        {c,   s,   0.f, 0.f},
        {-s,  c,   0.f, 0.f},
        {0.f, 0.f, 1.f, 0.f},
        {0.f, 0.f, 0.f, 1.f},
    }};
}

#endif
