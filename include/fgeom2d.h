/**
 * @file fgeom2d.h
 * @brief Structs and functions for common 2D geometry calculations.
 * @ingroup fastmath
 */

#ifndef LIBDRAGON_FGEOM2D_H
#define LIBDRAGON_FGEOM2D_H

#include <math.h>
#include <stdbool.h>
#include <memory.h>
#include "fmath.h"
#include "fgeom.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Two dimensional vector with single precision floating point components.
 * 
 * The components can either be accessed via named fields (x, y),
 * or by index using the v field.
 */
typedef union {
    struct {
        float x, y;  ///< Named fields to access individual vector components directly.
    };
    float v[2];         ///< Array field to access the vector components via index.
} fm_vec2_t;

/**
 * @brief 3x3 Matrix with single precision floating point components.
 */
typedef struct {
    float m[3][3];  ///< Two-dimensional array that contains the matrix coefficients in column-major order.
} fm_mat3_t;


/******** VEC2 **********/

/**
 * @brief Negate a 2D vector.
 * @note  This function still works if input and output are in the same memory location.
 */
inline void fm_vec2_negate(fm_vec2_t *out, const fm_vec2_t *v)
{
    for (int i = 0; i < 2; i++) out->v[i] = -v->v[i];
}

/**
 * @brief Add two 2D vectors component-wise.
 * @note  This function still works if input and output are in the same memory location.
 */
inline void fm_vec2_add(fm_vec2_t *out, const fm_vec2_t *a, const fm_vec2_t *b)
{
    for (int i = 0; i < 2; i++) out->v[i] = a->v[i] + b->v[i];
}

/**
 * @brief Subtract two 2D vectors component-wise.
 * @note  This function still works if input and output are in the same memory location.
 */
inline void fm_vec2_sub(fm_vec2_t *out, const fm_vec2_t *a, const fm_vec2_t *b)
{
    for (int i = 0; i < 2; i++) out->v[i] = a->v[i] - b->v[i];
}

/**
 * @brief Multiply two 2D vectors component-wise.
 * @note  If you need the dot product, use #fm_vec2_dot instead. 
 * @note  This function still works if input and output are in the same memory location.
 */
inline void fm_vec2_mul(fm_vec2_t *out, const fm_vec2_t *a, const fm_vec2_t *b)
{
    for (int i = 0; i < 2; i++) out->v[i] = a->v[i] * b->v[i];
}

/**
 * @brief Divide two 2D vectors component-wise.
 * @note  This function still works if input and output are in the same memory location.
 */
inline void fm_vec2_div(fm_vec2_t *out, const fm_vec2_t *a, const fm_vec2_t *b)
{
    for (int i = 0; i < 2; i++) out->v[i] = a->v[i] / b->v[i];
}

/**
 * @brief Scale a 2D vector by a factor.
 * @note  This function still works if input and output are in the same memory location.
 */
inline void fm_vec2_scale(fm_vec2_t *out, const fm_vec2_t *a, float s)
{
    for (int i = 0; i < 2; i++) out->v[i] = a->v[i] * s;
}

/** 
 * @brief Compute the dot product of two 2D vectors.
 */
inline float fm_vec2_dot(const fm_vec2_t *a, const fm_vec2_t *b)
{
    return a->x * b->x + a->y * b->y;
}

/**
 * @brief Compute the square magnitude of a 2D vector.
 */
inline float fm_vec2_len2(const fm_vec2_t *a)
{
    return fm_vec2_dot(a, a);
}

/**
 * @brief Compute the magnitude of a 2D vector.
 */
inline float fm_vec2_len(const fm_vec2_t *a)
{
    return sqrtf(fm_vec2_len2(a));
}

/**
 * @brief Compute the square distance between two 2D vectors.
 */
inline float fm_vec2_distance2(const fm_vec2_t *a, const fm_vec2_t *b)
{
    fm_vec2_t diff;
    fm_vec2_sub(&diff, a, b);
    return fm_vec2_len2(&diff);
}

/**
 * @brief Compute the distance between two 2D vectors.
 */
inline float fm_vec2_distance(const fm_vec2_t *a, const fm_vec2_t *b)
{
    return sqrtf(fm_vec2_distance2(a, b));
}

/**
 * @brief Normalize a 2D vector.
 * @note  This function still works if input and output are in the same memory location.
 */
inline void fm_vec2_norm(fm_vec2_t *out, const fm_vec2_t *a)
{
    float len = fm_vec2_len(a);
    if (len < FM_EPSILON) {
        *out = (fm_vec2_t){};
        return;
    }
    *out = (fm_vec2_t){{ a->x / len, a->y / len }};
}

/**
 * @brief Linearly interpolate between two 2D vectors.
 */
inline void fm_vec2_lerp(fm_vec2_t *out, const fm_vec2_t *a, const fm_vec2_t *b, float t)
{
    for (int i = 0; i < 2; i++) out->v[i] = a->v[i] + (b->v[i] - a->v[i]) * t;
}

/******** MAT3 **********/

/**
 * @brief Create a 3x3 identity matrix.
 */
inline void fm_mat3_identity(fm_mat3_t *out)
{
    *out = (fm_mat3_t){};
    out->m[0][0] = 1;
    out->m[1][1] = 1;
    out->m[2][2] = 1;
}

/**
 * @brief Apply scale to a 3x3 matrix.
 */
inline void fm_mat3_scale(fm_mat3_t *out, const fm_vec2_t *scale)
{
    for (int i=0; i<3; i++) out->m[0][i] *= scale->x;
    for (int i=0; i<3; i++) out->m[1][i] *= scale->y;
}

/**
 * @brief Apply translation to a 3x3 matrix.
 */
inline void fm_mat3_translate(fm_mat3_t *out, const fm_vec2_t *translate)
{
    for (int i=0; i<2; i++) out->m[2][i] += translate->v[i];
}

/**
 * @brief Apply a counterclockwise rotation to a 3x3 matrix.
 */
void fm_mat3_rotate(fm_mat3_t *out, float rotation);

/**
 * @brief Create a 3x3 affine transformation matrix from scale, rotation and translation.
 * 
 * The rotation is accepted as a counterclockwise angle in radians.
 * It creates an equivalent matrix to multiplying an identity matrix by a scale,
 * a rotation, and a translation matrix in that order.
 */
void fm_mat3_from_srt(fm_mat3_t *out, const fm_vec2_t *scale, float rotation, const fm_vec2_t *translate);

/**
 * @brief Create a 3x3 affine transformation matrix from rotation, scale, and translation.
 * 
 * The rotation is accepted as a counterclockwise angle in radians.
 * It creates an equivalent matrix to multiplying an identity matrix by a rotation,
 * a scale, and a translation matrix in that order.
 */
void fm_mat3_from_rst(fm_mat3_t *out, float rotation, const fm_vec2_t *scale, const fm_vec2_t *translate);

/**
 * @brief Create a 3x3 rigid transformation matrix from rotation and translation.
 * 
 * The rotation is accepted as an angle in radians.
 */
inline void fm_mat3_from_rt(fm_mat3_t *out, float rotation, const fm_vec2_t *translate)
{
    const fm_vec2_t scale = {{1, 1}};
    fm_mat3_from_srt(out, &scale, rotation, translate);
}

/**
 * @brief Create a 3x3 translation matrix.
 */
inline void fm_mat3_from_translation(fm_mat3_t *out, const fm_vec2_t *translate)
{
    fm_mat3_identity(out);
    fm_mat3_translate(out, translate);
}

/**
 * @brief Create a 3x3 rotation matrix from angle.
 */
inline void fm_mat3_from_rotation(fm_mat3_t *out, float theta)
{
    const fm_vec2_t translate = {{0, 0}};
    fm_mat3_from_rt(out, theta, &translate);
}

/**
 * @brief Create a 3x3 scale matrix.
 */
inline void fm_mat3_from_scale(fm_mat3_t *out, const fm_vec2_t *scale)
{
    fm_mat3_identity(out);
    fm_mat3_scale(out, scale);
}

/**
 * @brief Multiply two 3x3 matrices.
 */
inline void fm_mat3_mul(fm_mat3_t *out, const fm_mat3_t *a, const fm_mat3_t *b)
{
    fm_mat3_t tmp;
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            tmp.m[i][j] = a->m[0][j] * b->m[i][0] +
                          a->m[1][j] * b->m[i][1] +
                          a->m[2][j] * b->m[i][2];
        }
    }
    *out = tmp;
}

/**
 * @brief Transpose a 3x3 matrix.
 */
inline void fm_mat3_transpose(fm_mat3_t *out, const fm_mat3_t *m)
{
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            out->m[i][j] = m->m[j][i];
        }
    }
}

/**
 * @brief Compute the determinant of a 3x3 matrix.
 */
float fm_mat3_det(const fm_mat3_t *m);

/**
 * @brief Compute the inverse of a 3x3 matrix.
 */
void fm_mat3_inverse(fm_mat3_t *out, const fm_mat3_t *m);

/**
 * @brief Multiply a 2D vector by a 3x3 matrix by assuming 1 as the hypothetical 3rd component of the vector.
 */
inline void fm_mat3_mul_vec2(fm_vec3_t *out, const fm_mat3_t *m, const fm_vec2_t *v)
{
    for (int i = 0; i < 3; i++)
    {
        out->v[i] = m->m[0][i] * v->x +
                    m->m[1][i] * v->y +
                    m->m[2][i];
    }
}

/**
 * @brief Multiply a 3D vector by a 3x3 matrix.
 */
inline void fm_mat3_mul_vec3(fm_vec3_t *out, const fm_mat3_t *m, const fm_vec3_t *v)
{
    fm_vec3_t tmp;
    for (int i = 0; i < 3; i++)
    {
        tmp.v[i] = m->m[0][i] * v->x +
                   m->m[1][i] * v->y +
                   m->m[2][i] * v->z;
    }
    *out = tmp;
}

#ifdef __cplusplus
}
#endif

#endif