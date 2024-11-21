#ifndef LIBDRAGON_FGEOM_H
#define LIBDRAGON_FGEOM_H

#include <math.h>
#include <stdbool.h>
#include <memory.h>
#include "fmath.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef union {
    struct {
        float x, y, z;
    };
    float v[3];
} fm_vec3_t;

typedef union {
    struct {
        float x, y, z, w;
    };
    float v[4];
} fm_vec4_t;

typedef union {
    struct {
        float x, y, z, w;
    };
    float v[4];
} fm_quat_t;

typedef struct {
    float m[4][4];
} fm_mat4_t;

/******** VEC3 **********/

inline void fm_vec3_negate(fm_vec3_t *out, const fm_vec3_t *v)
{
    for (int i = 0; i < 3; i++) out->v[i] = -v->v[i];
}

inline void fm_vec3_add(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b)
{
    for (int i = 0; i < 3; i++) out->v[i] = a->v[i] + b->v[i];
}

inline void fm_vec3_sub(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b)
{
    for (int i = 0; i < 3; i++) out->v[i] = a->v[i] - b->v[i];
}

inline void fm_vec3_mul(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b)
{
    for (int i = 0; i < 3; i++) out->v[i] = a->v[i] * b->v[i];
}

inline void fm_vec3_div(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b)
{
    for (int i = 0; i < 3; i++) out->v[i] = a->v[i] / b->v[i];
}

inline void fm_vec3_scale(fm_vec3_t *out, const fm_vec3_t *a, float s)
{
    for (int i = 0; i < 3; i++) out->v[i] = a->v[i] * s;
}

inline float fm_vec3_dot(const fm_vec3_t *a, const fm_vec3_t *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

inline float fm_vec3_len2(const fm_vec3_t *a)
{
    return fm_vec3_dot(a, a);
}

inline float fm_vec3_len(const fm_vec3_t *a)
{
    return sqrtf(fm_vec3_len2(a));
}

inline float fm_vec3_distance2(const fm_vec3_t *a, const fm_vec3_t *b)
{
    fm_vec3_t diff;
    fm_vec3_sub(&diff, a, b);
    return fm_vec3_len2(&diff);
}

inline float fm_vec3_distance(const fm_vec3_t *a, const fm_vec3_t *b)
{
    return sqrtf(fm_vec3_distance2(a, b));
}

inline void fm_vec3_norm(fm_vec3_t *out, const fm_vec3_t *a)
{
    float len = fm_vec3_len(a);
    if (len < FM_EPSILON) {
        *out = (fm_vec3_t){};
        return;
    }
    *out = (fm_vec3_t){{ a->x / len, a->y / len, a->z / len }};
}

inline void fm_vec3_cross(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b)
{
    *out = (fm_vec3_t){{ a->y * b->z - a->z * b->y,
                         a->z * b->x - a->x * b->z,
                         a->x * b->y - a->y * b->x }};
}

inline void fm_vec3_lerp(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b, float t)
{
    for (int i = 0; i < 3; i++) out->v[i] = a->v[i] + (b->v[i] - a->v[i]) * t;
}

void fm_vec3_reflect(fm_vec3_t *out, const fm_vec3_t *i, const fm_vec3_t *n);
bool fm_vec3_refract(fm_vec3_t *out, const fm_vec3_t *i, const fm_vec3_t *n, float eta);

/******** QUAT **********/

inline void fm_quat_identity(fm_quat_t *out)
{
    *out = (fm_quat_t){{ 0, 0, 0, 1 }};
}

void fm_quat_from_euler(fm_quat_t *out, const float rot[3]);

inline void fm_quat_from_rotation(fm_quat_t *out, const float axis[3], float angle)
{
    float s, c;
    fm_sincosf(angle * 0.5f, &s, &c);

    *out = (fm_quat_t){{ axis[0] * s, axis[1] * s, axis[2] * s, c }};
}

void fm_quat_mul(fm_quat_t *out, const fm_quat_t *a, const fm_quat_t *b);
void fm_quat_rotate(fm_quat_t *out, const fm_quat_t *q, const float axis[3], float angle);

inline float fm_quat_dot(const fm_quat_t *a, const fm_quat_t *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z + a->w * b->w;
}

inline void fm_quat_norm(fm_quat_t *out, const fm_quat_t *q)
{
    float len = sqrtf(fm_quat_dot(q, q));
    if (len < FM_EPSILON) {
        fm_quat_identity(out);
        return;
    }
    *out = (fm_quat_t){{ q->x / len, q->y / len, q->z / len, q->w / len }};
}

void fm_quat_nlerp(fm_quat_t *out, const fm_quat_t *a, const fm_quat_t *b, float t);
void fm_quat_slerp(fm_quat_t *out, const fm_quat_t *a, const fm_quat_t *b, float t);


/******** MAT4 **********/

inline void fm_mat4_identity(fm_mat4_t *out)
{
    *out = (fm_mat4_t){};
    out->m[0][0] = 1;
    out->m[1][1] = 1;
    out->m[2][2] = 1;
    out->m[3][3] = 1;
}

inline void fm_mat4_scale(fm_mat4_t *out, const float v[3])
{
    for (int i=0; i<4; i++) out->m[0][i] *= v[0];
    for (int i=0; i<4; i++) out->m[1][i] *= v[1];
    for (int i=0; i<4; i++) out->m[2][i] *= v[2];
}

inline void fm_mat4_translate(fm_mat4_t *out, const float v[3])
{
    for (int i=0; i<3; i++) out->m[3][i] += v[i];
}

void fm_mat4_rotate(fm_mat4_t *out, const float axis[3], float angle);
void fm_mat4_from_srt(fm_mat4_t *out, const float scale[3], const float quat[4], const float translate[3]);
void fm_mat4_from_srt_euler(fm_mat4_t *out, const float scale[3], const float rot[3], const float translate[3]);


inline void fm_mat4_mul(fm_mat4_t *out, const fm_mat4_t *a, const fm_mat4_t *b)
{
    fm_mat4_t tmp;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            tmp.m[i][j] = a->m[i][0] * b->m[0][j] +
                          a->m[i][1] * b->m[1][j] +
                          a->m[i][2] * b->m[2][j] +
                          a->m[i][3] * b->m[3][j];
        }
    }
    *out = tmp;
}

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

float fm_mat4_det(const fm_mat4_t *m);

void fm_mat4_inverse(fm_mat4_t *out, const fm_mat4_t *m);

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

#ifdef __cplusplus
}
#endif

#endif