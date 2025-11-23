/**
 * @file fgeom2d.c
 */
 
#include "fgeom2d.h"


void fm_mat3_rotate(fm_mat3_t *out, float rotation)
{
    fm_mat3_t rotation_matrix;
    fm_mat3_from_rotation(&rotation_matrix, rotation);
    fm_mat3_mul(out, &rotation_matrix, out);
}

void fm_mat3_from_srt(fm_mat3_t *out, const fm_vec2_t *scale, float rotation, const fm_vec2_t *translate)
{
    float s, c;

    fm_sincosf(rotation, &s, &c);
    float sx = scale->x, sy = scale->y;

    *out = (fm_mat3_t){{
        { c*sx, -s*sy, 0 },
        { s*sx, c*sy, 0 },
        { translate->x, translate->y, 1 }
    }};
}

float fm_mat3_det(const fm_mat3_t *m)
{
    float m00 = m->m[0][0], m01 = m->m[0][1], m02 = m->m[0][2],
          m10 = m->m[1][0], m11 = m->m[1][1], m12 = m->m[1][2],
          m20 = m->m[2][0], m21 = m->m[2][1], m22 = m->m[2][2];

    return m00 * (m11 * m22 - m21 * m12) -
             m01 * (m10 * m22 - m12 * m20) +
             m02 * (m10 * m21 - m11 * m20);
}

void fm_mat3_inverse(fm_mat3_t *out, const fm_mat3_t *m)
{
    fm_mat3_t tmp;
    float m00 = m->m[0][0], m01 = m->m[0][1], m02 = m->m[0][2],
          m10 = m->m[1][0], m11 = m->m[1][1], m12 = m->m[1][2],
          m20 = m->m[2][0], m21 = m->m[2][1], m22 = m->m[2][2];
    tmp.m[0][0] = (m11*m22)-(m12*m21);
    tmp.m[0][1] = -((m01*m22)-(m02*m21));
    tmp.m[0][2] = (m01*m12)-(m02*m11);
    
    tmp.m[1][0] = (m10*m22)-(m12*m20);
    tmp.m[1][1] = -((m00*m22)-(m02*m20));
    tmp.m[1][2] = (m00*m12)-(m02*m10);
    
    tmp.m[2][0] = (m10*m21)-(m11*m20);
    tmp.m[2][1] = -((m00*m21)-(m01*m20));
    tmp.m[2][2] = (m00*m11)-(m01*m10);
    float det = 1.0f/(m00 * (m11 * m22 - m21 * m12) -
             m01 * (m10 * m22 - m12 * m20) +
             m02 * (m10 * m21 - m11 * m20));
    for(int i=0; i<3; i++) {
        for(int j=0; j<3; j++) {
            out->m[i][j] = tmp.m[i][j]*det;
        }
    }
}

extern inline void fm_vec2_negate(fm_vec2_t *out, const fm_vec2_t *v);
extern inline void fm_vec2_add(fm_vec2_t *out, const fm_vec2_t *a, const fm_vec2_t *b);
extern inline void fm_vec2_sub(fm_vec2_t *out, const fm_vec2_t *a, const fm_vec2_t *b);
extern inline void fm_vec2_mul(fm_vec2_t *out, const fm_vec2_t *a, const fm_vec2_t *b);
extern inline void fm_vec2_div(fm_vec2_t *out, const fm_vec2_t *a, const fm_vec2_t *b);
extern inline void fm_vec2_scale(fm_vec2_t *out, const fm_vec2_t *a, float s);
extern inline float fm_vec2_dot(const fm_vec2_t *a, const fm_vec2_t *b);
extern inline float fm_vec2_len2(const fm_vec2_t *a);
extern inline float fm_vec2_len(const fm_vec2_t *a);
extern inline float fm_vec2_distance2(const fm_vec2_t *a, const fm_vec2_t *b);
extern inline float fm_vec2_distance(const fm_vec2_t *a, const fm_vec2_t *b);
extern inline void fm_vec2_norm(fm_vec2_t *out, const fm_vec2_t *a);
extern inline void fm_vec2_lerp(fm_vec2_t *out, const fm_vec2_t *a, const fm_vec2_t *b, float t);
extern inline void fm_mat3_identity(fm_mat3_t *out);
extern inline void fm_mat3_scale(fm_mat3_t *out, const fm_vec2_t *scale);
extern inline void fm_mat3_translate(fm_mat3_t *out, const fm_vec2_t *translate);
extern inline void fm_mat3_from_rt(fm_mat3_t *out, float rotation, const fm_vec2_t *translate);
extern inline void fm_mat3_from_translation(fm_mat3_t *out, const fm_vec2_t *translate);
extern inline void fm_mat3_from_rotation(fm_mat3_t *out, float rotation);
extern inline void fm_mat3_from_scale(fm_mat3_t *out, const fm_vec2_t *scale);
extern inline void fm_mat3_mul(fm_mat3_t *out, const fm_mat3_t *a, const fm_mat3_t *b);
extern inline void fm_mat3_transpose(fm_mat3_t *out, const fm_mat3_t *m);
extern inline void fm_mat3_mul_vec2(fm_vec3_t *out, const fm_mat3_t *m, const fm_vec2_t *v);
extern inline void fm_mat3_mul_vec3(fm_vec3_t *out, const fm_mat3_t *m, const fm_vec3_t *v);
