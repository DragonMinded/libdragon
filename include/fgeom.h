/**
 * @file fgeom.h
 * @brief Structs and functions for common 3D geometry calculations.
 * @ingroup fastmath
 */

#ifndef LIBDRAGON_FGEOM_H
#define LIBDRAGON_FGEOM_H

#include <math.h>
#include <stdbool.h>
#include <memory.h>
#include "fmath.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Three dimensional vector with single precision floating point components.
 * 
 * The components can either be accessed via named fields (x, y, z),
 * or by index using the v field.
 */
typedef union {
    struct {
        float x, y, z;  ///< Named fields to access individual vector components directly.
    };
    float v[3];         ///< Array field to access the vector components via index.
} fm_vec3_t;

/**
 * @brief Four dimensional vector with single precision floating point components.
 * 
 * The components can either be accessed via named fields (x, y, z, w),
 * or by index using the v field.
 */
typedef union {
    struct {
        float x, y, z, w;   ///< Named fields to access individual vector components directly.
    };
    float v[4];             ///< Array field to access the vector components via index.
} fm_vec4_t;

/**
 * @brief A quaternion with single precision floating point components.
 * 
 * The components can either be accessed via named fields (x, y, z, w),
 * or by index using the v field.
 */
typedef union {
    struct {
        float x, y, z, w;   ///< Named fields to access individual quaternion components directly.
    };
    float v[4];             ///< Array field to access the quaternion components via index.
} fm_quat_t;

/**
 * @brief 4x4 Matrix with single precision floating point components.
 */
typedef struct {
    float m[4][4];  ///< Two-dimensional array that contains the matrix coefficients in column-major order.
} fm_mat4_t;

/******** VEC3 **********/

/**
 * @brief Negate a 3D vector.
 * @note  This function still works if input and output are in the same memory location.
 */
inline void fm_vec3_negate(fm_vec3_t *out, const fm_vec3_t *v)
{
    for (int i = 0; i < 3; i++) out->v[i] = -v->v[i];
}

/**
 * @brief Add two 3D vectors component-wise.
 * @note  This function still works if input and output are in the same memory location.
 */
inline void fm_vec3_add(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b)
{
    for (int i = 0; i < 3; i++) out->v[i] = a->v[i] + b->v[i];
}

/**
 * @brief Subtract two 3D vectors component-wise.
 * @note  This function still works if input and output are in the same memory location.
 */
inline void fm_vec3_sub(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b)
{
    for (int i = 0; i < 3; i++) out->v[i] = a->v[i] - b->v[i];
}

/**
 * @brief Multiply two 3D vectors component-wise.
 * @note  If you need the dot product, use #fm_vec3_dot instead. 
 *        If you need the cross product, use #fm_vec3_cross instead.
 * @note  This function still works if input and output are in the same memory location.
 */
inline void fm_vec3_mul(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b)
{
    for (int i = 0; i < 3; i++) out->v[i] = a->v[i] * b->v[i];
}

/**
 * @brief Divide two 3D vectors component-wise.
 * @note  This function still works if input and output are in the same memory location.
 */
inline void fm_vec3_div(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b)
{
    for (int i = 0; i < 3; i++) out->v[i] = a->v[i] / b->v[i];
}

/**
 * @brief Scale a 3D vector by a factor.
 * @note  This function still works if input and output are in the same memory location.
 */
inline void fm_vec3_scale(fm_vec3_t *out, const fm_vec3_t *a, float s)
{
    for (int i = 0; i < 3; i++) out->v[i] = a->v[i] * s;
}

/** 
 * @brief Compute the dot product of two 3D vectors.
 */
inline float fm_vec3_dot(const fm_vec3_t *a, const fm_vec3_t *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

/**
 * @brief Compute the square magnitude of a 3D vector.
 */
inline float fm_vec3_len2(const fm_vec3_t *a)
{
    return fm_vec3_dot(a, a);
}

/**
 * @brief Compute the magnitude of a 3D vector.
 */
inline float fm_vec3_len(const fm_vec3_t *a)
{
    return sqrtf(fm_vec3_len2(a));
}

/**
 * @brief Compute the square distance between two 3D vectors.
 */
inline float fm_vec3_distance2(const fm_vec3_t *a, const fm_vec3_t *b)
{
    fm_vec3_t diff;
    fm_vec3_sub(&diff, a, b);
    return fm_vec3_len2(&diff);
}

/**
 * @brief Compute the distance between two 3D vectors.
 */
inline float fm_vec3_distance(const fm_vec3_t *a, const fm_vec3_t *b)
{
    return sqrtf(fm_vec3_distance2(a, b));
}

/**
 * @brief Normalize a 3D vector.
 * @note  This function still works if input and output are in the same memory location.
 */
inline void fm_vec3_norm(fm_vec3_t *out, const fm_vec3_t *a)
{
    float len = fm_vec3_len(a);
    if (len < FM_EPSILON) {
        *out = (fm_vec3_t){};
        return;
    }
    *out = (fm_vec3_t){{ a->x / len, a->y / len, a->z / len }};
}

/**
 * @brief Compute the cross product of two 3D vectors.
 */
inline void fm_vec3_cross(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b)
{
    *out = (fm_vec3_t){{ a->y * b->z - a->z * b->y,
                         a->z * b->x - a->x * b->z,
                         a->x * b->y - a->y * b->x }};
}

/**
 * @brief Linearly interpolate between two 3D vectors.
 */
inline void fm_vec3_lerp(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b, float t)
{
    for (int i = 0; i < 3; i++) out->v[i] = a->v[i] + (b->v[i] - a->v[i]) * t;
}

/**
 * @brief Compute the reflection of an incident vector off a surface.
 * 
 * @param[out] out  Will contain the reflected vector.
 * @param[in]  i    The incident vector.
 * @param[in]  n    The surface's normal vector. Must be normalized.
 */
void fm_vec3_reflect(fm_vec3_t *out, const fm_vec3_t *i, const fm_vec3_t *n);

/**
 * @brief Compute the refraction of an incident vector through a surface.
 * 
 * @param[out] out  Will contain the refracted vector.
 * @param[in]  i    The incident vector. Must be normalized.
 * @param[in]  n    The surface's normal vector. Must be normalized.
 * @param[in]  eta  The ratio of indices of refraction (indicent/transmitted)
 * @return          True if refraction occurs; false if total internal reflection occurs.
 */
bool fm_vec3_refract(fm_vec3_t *out, const fm_vec3_t *i, const fm_vec3_t *n, float eta);

/******** QUAT **********/

/**
 * @brief Create an identity quaternion.
 */
inline void fm_quat_identity(fm_quat_t *out)
{
    *out = (fm_quat_t){{ 0, 0, 0, 1 }};
}

/**
 * @brief Create a quaternion from euler angles.
 * 
 * @param[out] out  Will contain the created quaternion.
 * @param[in]  euler  Array containing euler angles in radians.
 */
void fm_quat_from_euler(fm_quat_t *out, const float euler[3]);

/**
 * @brief Create a quaternion from euler angles, applying the rotations in ZYX order.
 * 
 * @param[out] out  Will contain the created quaternion.
 * @param[in]  x    Rotation around the X axis in radians.
 * @param[in]  y    Rotation around the Y axis in radians.
 * @param[in]  z    Rotation around the Z axis in radians.
 */
void fm_quat_from_euler_zyx(fm_quat_t *out, float x, float y, float z);

/**
 * @brief Create a quaternion from an axis of rotation and an angle.
 * @param[out] out      Will contain the created quaternion.
 * @param[in]  axis     The axis of rotation. Must be normalized.
 * @param[in]  angle    The angle in radians.
 */
inline void fm_quat_from_axis_angle(fm_quat_t *out, const fm_vec3_t *axis, float angle)
{
    float s, c;
    fm_sincosf(angle * 0.5f, &s, &c);

    *out = (fm_quat_t){{ axis->x * s, axis->y * s, axis->z * s, c }};
}

/**
 * @brief Multiply two quaternions.
 */
void fm_quat_mul(fm_quat_t *out, const fm_quat_t *a, const fm_quat_t *b);

/**
 * @brief Rotate a quaternion around an axis by some angle.
 * @note This is just a shorthand for #fm_quat_from_axis_angle and #fm_quat_mul.
 */
void fm_quat_rotate(fm_quat_t *out, const fm_quat_t *q, const fm_vec3_t *axis, float angle);

/**
 * @brief Compute the dot product of two quaternions.
 */
inline float fm_quat_dot(const fm_quat_t *a, const fm_quat_t *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z + a->w * b->w;
}

/**
 * @brief Compute the inverse of a quaternion.
 */
inline void fm_quat_inverse(fm_quat_t *out, const fm_quat_t *q)
{
    float inv_mag2 = 1.0f / fm_quat_dot(q, q);
    for (int i = 0; i < 3; i++) out->v[i] = -q->v[i] * inv_mag2;
    out->w = q->w * inv_mag2;
}

/**
 * @brief Normalize a quaternion.
 */
inline void fm_quat_norm(fm_quat_t *out, const fm_quat_t *q)
{
    float len = sqrtf(fm_quat_dot(q, q));
    if (len < FM_EPSILON) {
        fm_quat_identity(out);
        return;
    }
    *out = (fm_quat_t){{ q->x / len, q->y / len, q->z / len, q->w / len }};
}

/**
 * @brief Compute normalized linear interpolation between two quaternions.
 * @note This function is faster than #fm_quat_slerp, but produces a non-constant angular velocity when used for animation.
 */
void fm_quat_nlerp(fm_quat_t *out, const fm_quat_t *a, const fm_quat_t *b, float t);


/**
 * @brief Compute spherical linear interpolation between two quaternions.
 * @note As opposed to #fm_quat_nlerp, this function produces constant angular velocity when used for animation, but is slower.
 */
void fm_quat_slerp(fm_quat_t *out, const fm_quat_t *a, const fm_quat_t *b, float t);


/******** MAT4 **********/

/**
 * @brief Create a 4x4 identity matrix.
 */
inline void fm_mat4_identity(fm_mat4_t *out)
{
    *out = (fm_mat4_t){};
    out->m[0][0] = 1;
    out->m[1][1] = 1;
    out->m[2][2] = 1;
    out->m[3][3] = 1;
}

/**
 * @brief Apply scale to a 4x4 matrix.
 */
inline void fm_mat4_scale(fm_mat4_t *out, const fm_vec3_t *scale)
{
    for (int i=0; i<4; i++) out->m[0][i] *= scale->x;
    for (int i=0; i<4; i++) out->m[1][i] *= scale->y;
    for (int i=0; i<4; i++) out->m[2][i] *= scale->z;
}

/**
 * @brief Apply translation to a 4x4 matrix.
 */
inline void fm_mat4_translate(fm_mat4_t *out, const fm_vec3_t *translate)
{
    for (int i=0; i<3; i++) out->m[3][i] += translate->v[i];
}

/**
 * @brief Apply rotation to a 4x4 matrix.
 */
void fm_mat4_rotate(fm_mat4_t *out, const fm_quat_t *rotation);

/**
 * @brief Create a rotation matrix from an axis of rotation and an angle.
 */
void fm_mat4_from_axis_angle(fm_mat4_t *out, const fm_vec3_t *axis, float angle);

/**
 * @brief Create an affine transformation matrix from scale, rotation and translation.
 * 
 * The rotation is accepted as a quaternion.
 */
void fm_mat4_from_srt(fm_mat4_t *out, const fm_vec3_t *scale, const fm_quat_t *quat, const fm_vec3_t *translate);

/**
 * @brief Create an affine transformation matrix from scale, rotation and translation.
 * 
 * The rotation is accepted as euler angles.
 */
void fm_mat4_from_srt_euler(fm_mat4_t *out, const fm_vec3_t *scale, const float euler[3], const fm_vec3_t *translate);

/**
 * @brief Create a rigid transformation matrix from rotation and translation.
 * 
 * The rotation is accepted as a quaternion.
 */
inline void fm_mat4_from_rt(fm_mat4_t *out, const fm_quat_t *quat, const fm_vec3_t *translate)
{
    const fm_vec3_t scale = {{1, 1, 1}};
    fm_mat4_from_srt(out, &scale, quat, translate);
}

/**
 * @brief Create a rigid transformation matrix from rotation and translation.
 * 
 * The rotation is accepted as euler angles.
 */
inline void fm_mat4_from_rt_euler(fm_mat4_t *out, const float euler[3], const fm_vec3_t *translate)
{
    const fm_vec3_t scale = {{1, 1, 1}};
    fm_mat4_from_srt_euler(out, &scale, euler, translate);
}

/**
 * @brief Create a translation matrix.
 */
inline void fm_mat4_from_translation(fm_mat4_t *out, const fm_vec3_t *translate)
{
    fm_mat4_identity(out);
    fm_mat4_translate(out, translate);
}

/**
 * @brief Create a rotation matrix from a quaternion.
 */
inline void fm_mat4_from_rotation(fm_mat4_t *out, const fm_quat_t *rotation)
{
    const fm_vec3_t translate = {{0, 0, 0}};
    fm_mat4_from_rt(out, rotation, &translate);
}

/**
 * @brief Create a scale matrix.
 */
inline void fm_mat4_from_scale(fm_mat4_t *out, const fm_vec3_t *scale)
{
    fm_mat4_identity(out);
    fm_mat4_scale(out, scale);
}

/**
 * @brief Multiply two 4x4 matrices.
 */
inline void fm_mat4_mul(fm_mat4_t *out, const fm_mat4_t *a, const fm_mat4_t *b)
{
    fm_mat4_t tmp;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            tmp.m[i][j] = a->m[0][j] * b->m[i][0] +
                          a->m[1][j] * b->m[i][1] +
                          a->m[2][j] * b->m[i][2] +
                          a->m[3][j] * b->m[i][3];
        }
    }
    *out = tmp;
}

/**
 * @brief Transpose a 4x4 matrix.
 */
inline void fm_mat4_transpose(fm_mat4_t *out, const fm_mat4_t *m)
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            out->m[i][j] = m->m[j][i];
        }
    }
}

/**
 * @brief Compute the determinant of a 4x4 matrix.
 */
float fm_mat4_det(const fm_mat4_t *m);

/**
 * @brief Compute the inverse of a 4x4 matrix.
 */
void fm_mat4_inverse(fm_mat4_t *out, const fm_mat4_t *m);

/**
 * @brief Compute a normal matrix from an affine 4x4 matrix.
 * 
 * The resulting matrix can be used to transform face normal vectors while preserving
 * their property of being perpendicular to the surface.
 * In general, such a matrix is obtained by computing the transpose inverse of the
 * matrix in question. If the input matrix is affine, however, it is possible to
 * take a shortcut and only compute the transpose inverse of the upper left 3x3 part.
 * Accordingly, this function only produces valid results if the input matrix is affine.
 * 
 * Also note that computing the transpose inverse is technically only necessary if
 * the input matrix represents any non-rigid transformations (scaling, shearing etc.).
 * If your matrix is rigid (only translation and rotation), then the transpose inverse
 * is in fact the same as the input and this function can be skipped.
 * Additionally, if your matrix contains only uniform scaling (the same scale factor on all 3 axes)
 * you may skip computing the inverse transpose as well, provided you re-normalize
 * the normal vectors after transformation.
 * 
 * @param[out]  out The resulting matrix.
 * @param[in]   m   The input matrix. Must be affine.
 */
void fm_mat4_affine_to_normal_mat(fm_mat4_t *out, const fm_mat4_t *m);

/**
 * @brief Create a view matrix that represents a camera looking in a certain direction from some position.
 */
void fm_mat4_look(fm_mat4_t *out, const fm_vec3_t *eye, const fm_vec3_t *dir, const fm_vec3_t *up);

/**
 * @brief Create a view matrix that represents a camera looking at a target point from some position.
 */
void fm_mat4_lookat(fm_mat4_t *out, const fm_vec3_t *eye, const fm_vec3_t *target, const fm_vec3_t *up);

/**
 * @brief Multiply a 3D vector by a 4x4 matrix by assuming 1 as the hypothetical 4th component of the vector.
 */
inline void fm_mat4_mul_vec3(fm_vec4_t *out, const fm_mat4_t *m, const fm_vec3_t *v)
{
    for (int i = 0; i < 4; i++)
    {
        out->v[i] = m->m[0][i] * v->x +
                    m->m[1][i] * v->y +
                    m->m[2][i] * v->z +
                    m->m[3][i];
    }
}

/**
 * @brief Multiply a 4D vector by a 4x4 matrix.
 */
inline void fm_mat4_mul_vec4(fm_vec4_t *out, const fm_mat4_t *m, const fm_vec4_t *v)
{
    fm_vec4_t tmp;
    for (int i = 0; i < 4; i++)
    {
        tmp.v[i] = m->m[0][i] * v->x +
                   m->m[1][i] * v->y +
                   m->m[2][i] * v->z +
                   m->m[3][i] * v->w;
    }
    *out = tmp;
}

#ifdef __cplusplus
}
#endif

#endif