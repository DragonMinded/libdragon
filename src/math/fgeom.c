#include "fgeom.h"

void fm_vec3_reflect(fm_vec3_t *out, const fm_vec3_t *i, const fm_vec3_t *n)
{
    fm_vec3_t tmp;
    fm_vec3_scale(&tmp, n, 2.0f * fm_vec3_dot(i, n));
    fm_vec3_sub(out, i, &tmp);
}

bool fm_vec3_refract(fm_vec3_t *out, const fm_vec3_t *i, const fm_vec3_t *n, float eta)
{
    float ndi = fm_vec3_dot(n, i);
    float eni = eta * ndi;
    float k = 1.0f - eta*eta + eni*eni;

    if (k < 0.0f) {
        *out = (fm_vec3_t){};
        return false;
    }

    fm_vec3_t tmp;
    fm_vec3_scale(&tmp, n, eni + sqrtf(k));
    fm_vec3_scale(out, i, eta);
    fm_vec3_sub(out, out, &tmp);
    return true;
}

// Create a quaternion 
void fm_quat_from_euler(fm_quat_t *out, const float euler[3])
{
    float c1, c2, c3, s1, s2, s3;
    fm_sincosf(euler[0] * 0.5f, &s1, &c1);
    fm_sincosf(euler[1] * 0.5f, &s2, &c2);
    fm_sincosf(euler[2] * 0.5f, &s3, &c3);

    *out = (fm_quat_t){{ c1 * c2 * c3 + s1 * s2 * s3,
                         s1 * c2 * c3 - c1 * s2 * s3,
                         c1 * s2 * c3 + s1 * c2 * s3,
                         c1 * c2 * s3 - s1 * s2 * c3 }};
}

void fm_quat_from_euler_zyx(fm_quat_t *out, float x, float y, float z)
{
    float xs, xc, ys, yc, zs, zc;

    fm_sincosf(x * 0.5f, &xs, &xc);
    fm_sincosf(y * 0.5f, &ys, &yc);
    fm_sincosf(z * 0.5f, &zs, &zc);

    out->v[0] =  xs * yc * zc - xc * ys * zs;
    out->v[1] =  xc * ys * zc + xs * yc * zs;
    out->v[2] = -xs * ys * zc + xc * yc * zs;
    out->v[3] =  xc * yc * zc + xs * ys * zs;
}

inline void fm_quat_mul(fm_quat_t *out, const fm_quat_t *a, const fm_quat_t *b)
{
    *out = (fm_quat_t){{ a->v[3] * b->v[0] + a->v[0] * b->v[3] + a->v[1] * b->v[2] - a->v[2] * b->v[1],
                         a->v[3] * b->v[1] - a->v[0] * b->v[2] + a->v[1] * b->v[3] + a->v[2] * b->v[0],
                         a->v[3] * b->v[2] + a->v[0] * b->v[1] - a->v[1] * b->v[0] + a->v[2] * b->v[3],
                         a->v[3] * b->v[3] - a->v[0] * b->v[0] - a->v[1] * b->v[1] - a->v[2] * b->v[2] }};
}

void fm_quat_rotate(fm_quat_t *out, const fm_quat_t *q, const fm_vec3_t *axis, float angle)
{
    fm_quat_t r;
    fm_quat_from_axis_angle(&r, axis, angle);
    fm_quat_mul(out, q, &r);
}

void fm_quat_nlerp(fm_quat_t *out, const fm_quat_t *a, const fm_quat_t *b, float t)
{
    float blend = 1.0f - t;
    if (fm_quat_dot(a, b) < 0.0f) blend = -blend;

    *out = (fm_quat_t){{ a->v[0] * blend + b->v[0] * t,
                         a->v[1] * blend + b->v[1] * t,
                         a->v[2] * blend + b->v[2] * t,
                         a->v[3] * blend + b->v[3] * t }};
    fm_quat_norm(out, out);
}

void fm_quat_slerp(fm_quat_t *out, const fm_quat_t *a, const fm_quat_t *b, float t)
{
    float dot = fm_quat_dot(a, b);
    float negated = 1.0f;
    if (dot < 0.0f) {
        dot = -dot;
        negated = -1.0f;
    }

    float theta = acosf(dot);
    float sin_theta = sinf(theta);
    float inv_sin_theta = 1.0f / sin_theta;
    float ws = sinf((1.0f - t) * theta) * inv_sin_theta;
    float we = sinf(t * theta) * inv_sin_theta * negated;
    *out = (fm_quat_t){{ a->v[0] * ws + b->v[0] * we,
                         a->v[1] * ws + b->v[1] * we,
                         a->v[2] * ws + b->v[2] * we,
                         a->v[3] * ws + b->v[3] * we }};
}

void fm_mat4_rotate(fm_mat4_t *out, const fm_quat_t *rotation)
{
    fm_mat4_t rotation_matrix;
    fm_mat4_from_rotation(&rotation_matrix, rotation);
    fm_mat4_mul(out, &rotation_matrix, out);
}

void fm_mat4_from_axis_angle(fm_mat4_t *out, const fm_vec3_t *axis, float angle)
{
    float s, c;
    fm_sincosf(angle, &s, &c);

    float t = 1.0f - c;
    float x = axis->x, y = axis->y, z = axis->z;

    *out = (fm_mat4_t){{
        { t * x * x + c,     t * x * y - s * z, t * x * z + s * y, 0 },
        { t * x * y + s * z, t * y * y + c,     t * y * z - s * x, 0 },
        { t * x * z - s * y, t * y * z + s * x, t * z * z + c,     0 },
        { 0,                 0,                 0,                 1 }
    }};
}

void fm_mat4_from_srt(fm_mat4_t *out, const fm_vec3_t *scale, const fm_quat_t *quat, const fm_vec3_t *translate)
{
    float x = quat->x, y = quat->y, z = quat->z, w = quat->w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;
    float sx = scale->x, sy = scale->y, sz = scale->z;

    *out = (fm_mat4_t){{
        { (1.0f - (yy + zz)) * sx, (xy + wz) * sx, (xz - wy) * sx, 0 },
        { (xy - wz) * sy, (1.0f - (xx + zz)) * sy, (yz + wx) * sy, 0 },
        { (xz + wy) * sz, (yz - wx) * sz, (1.0f - (xx + yy)) * sz, 0 },
        { translate->x, translate->y, translate->z, 1 }
    }};
}

void fm_mat4_from_srt_euler(fm_mat4_t *out, const fm_vec3_t *scale, const float euler[3], const fm_vec3_t *translate)
{
    float s0, c0, s1, c1, s2, c2;
    fm_sincosf(euler[0], &s0, &c0);
    fm_sincosf(euler[1], &s1, &c1);
    fm_sincosf(euler[2], &s2, &c2);

    *out = (fm_mat4_t){{
        { scale->x * c2 * c1, scale->x * (c2 * s1 * s0 - s2 * c0), scale->x * (c2 * s1 * c0 + s2 * s0), 0 },
        { scale->y * s2 * c1, scale->y * (s2 * s1 * s0 + c2 * c0), scale->y * (s2 * s1 * c0 - c2 * s0), 0 },
        {-scale->z * s1,      scale->z * c1 * s0,                  scale->z * c1 * c0,                  0 },
        { translate->x,       translate->y,                        translate->z,                        1 }
    }};
}

float fm_mat4_det(const fm_mat4_t *m)
{
    float m00 = m->m[0][0], m01 = m->m[0][1], m02 = m->m[0][2], m03 = m->m[0][3],
          m10 = m->m[1][0], m11 = m->m[1][1], m12 = m->m[1][2], m13 = m->m[1][3],
          m20 = m->m[2][0], m21 = m->m[2][1], m22 = m->m[2][2], m23 = m->m[2][3],
          m30 = m->m[3][0], m31 = m->m[3][1], m32 = m->m[3][2], m33 = m->m[3][3];

    float t0 = m22 * m33 - m32 * m23;
    float t1 = m21 * m33 - m31 * m23;
    float t2 = m21 * m32 - m31 * m22;
    float t3 = m20 * m33 - m30 * m23;
    float t4 = m20 * m32 - m30 * m22;
    float t5 = m20 * m31 - m30 * m21;

    return m00 * (m11 * t0 - m12 * t1 + m13 * t2)
         - m01 * (m10 * t0 - m12 * t3 + m13 * t4)
         + m02 * (m10 * t1 - m11 * t3 + m13 * t5)
         - m03 * (m10 * t2 - m11 * t4 + m12 * t5);
}

void fm_mat4_inverse(fm_mat4_t *out, const fm_mat4_t *m)
{
    fm_mat4_t tmp;
    
    float m00 = m->m[0][0], m01 = m->m[0][1], m02 = m->m[0][2], m03 = m->m[0][3],
          m10 = m->m[1][0], m11 = m->m[1][1], m12 = m->m[1][2], m13 = m->m[1][3],
          m20 = m->m[2][0], m21 = m->m[2][1], m22 = m->m[2][2], m23 = m->m[2][3],
          m30 = m->m[3][0], m31 = m->m[3][1], m32 = m->m[3][2], m33 = m->m[3][3];

    float t0 = m22 * m33 - m32 * m23; 
    float t1 = m21 * m33 - m31 * m23; 
    float t2 = m21 * m32 - m31 * m22;
    float t3 = m20 * m33 - m30 * m23; 
    float t4 = m20 * m32 - m30 * m22; 
    float t5 = m20 * m31 - m30 * m21;

    tmp.m[0][0] =  m11 * t0 - m12 * t1 + m13 * t2;
    tmp.m[1][0] =-(m10 * t0 - m12 * t3 + m13 * t4);
    tmp.m[2][0] =  m10 * t1 - m11 * t3 + m13 * t5;
    tmp.m[3][0] =-(m10 * t2 - m11 * t4 + m12 * t5);

    tmp.m[0][1] =-(m01 * t0 - m02 * t1 + m03 * t2);
    tmp.m[1][1] =  m00 * t0 - m02 * t3 + m03 * t4;
    tmp.m[2][1] =-(m00 * t1 - m01 * t3 + m03 * t5);
    tmp.m[3][1] =  m00 * t2 - m01 * t4 + m02 * t5;

    t0 = m12 * m33 - m32 * m13; 
    t1 = m11 * m33 - m31 * m13;
    t2 = m11 * m32 - m31 * m12;
    t3 = m10 * m33 - m30 * m13; 
    t4 = m10 * m32 - m30 * m12;
    t5 = m10 * m31 - m30 * m11;

    tmp.m[0][2] =  m01 * t0 - m02 * t1 + m03 * t2;
    tmp.m[1][2] =-(m00 * t0 - m02 * t3 + m03 * t4);
    tmp.m[2][2] =  m00 * t1 - m01 * t3 + m03 * t5;
    tmp.m[3][2] =-(m00 * t2 - m01 * t4 + m02 * t5);

    t0 = m12 * m23 - m22 * m13;
    t1 = m11 * m23 - m21 * m13;
    t2 = m11 * m22 - m21 * m12;
    t3 = m10 * m23 - m20 * m13;
    t4 = m10 * m22 - m20 * m12;
    t5 = m10 * m21 - m20 * m11;

    tmp.m[0][3] =-(m01 * t0 - m02 * t1 + m03 * t2);
    tmp.m[1][3] =  m00 * t0 - m02 * t3 + m03 * t4;
    tmp.m[2][3] =-(m00 * t1 - m01 * t3 + m03 * t5);
    tmp.m[3][3] =  m00 * t2 - m01 * t4 + m02 * t5;

    float det = 1.0f / (m00 * tmp.m[0][0] + m01 * tmp.m[1][0]
                      + m02 * tmp.m[2][0] + m03 * tmp.m[3][0]);

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            out->m[i][j] = tmp.m[i][j] * det;
        }
    }    
}

void fm_mat4_affine_to_normal_mat(fm_mat4_t *out, const fm_mat4_t *m)
{
    float m00 = m->m[0][0], m01 = m->m[0][1], m02 = m->m[0][2],
          m10 = m->m[1][0], m11 = m->m[1][1], m12 = m->m[1][2],
          m20 = m->m[2][0], m21 = m->m[2][1], m22 = m->m[2][2];
    
    float tmp[3][3];

    tmp[0][0] =   m11 * m22 - m12 * m21;
    tmp[0][1] = -(m01 * m22 - m21 * m02);
    tmp[0][2] =   m01 * m12 - m11 * m02;
    tmp[1][0] = -(m10 * m22 - m20 * m12);
    tmp[1][1] =   m00 * m22 - m02 * m20;
    tmp[1][2] = -(m00 * m12 - m10 * m02);
    tmp[2][0] =   m10 * m21 - m20 * m11;
    tmp[2][1] = -(m00 * m21 - m20 * m01);
    tmp[2][2] =   m00 * m11 - m01 * m10;

    float det = 1.0f / (m00 * tmp[0][0] + m01 * tmp[1][0] + m02 * tmp[2][0]);

    *out = (fm_mat4_t){{
        {tmp[0][0] * det, tmp[1][0] * det, tmp[2][0] * det, 0},
        {tmp[0][1] * det, tmp[1][1] * det, tmp[2][1] * det, 0},
        {tmp[0][2] * det, tmp[1][2] * det, tmp[2][2] * det, 0},
        {0,               0,               0,               1},
    }};
}

void fm_mat4_look(fm_mat4_t *out, const fm_vec3_t *eye, const fm_vec3_t *dir, const fm_vec3_t *up)
{
    fm_vec3_t s, u;

    fm_vec3_cross(&s, dir, up);
    fm_vec3_norm(&s, &s);

    fm_vec3_cross(&u, &s, dir);

    *out = (fm_mat4_t){{
        {s.x, u.x, -dir->x, 0},
        {s.y, u.y, -dir->y, 0},
        {s.z, u.z, -dir->z, 0},
        {-fm_vec3_dot(&s, eye), -fm_vec3_dot(&u, eye), fm_vec3_dot(dir, eye), 1},
    }};
}

void fm_mat4_lookat(fm_mat4_t *out, const fm_vec3_t *eye, const fm_vec3_t *target, const fm_vec3_t *up)
{
    fm_vec3_t dir;
    fm_vec3_sub(&dir, target, eye);
    fm_vec3_norm(&dir, &dir);
    fm_mat4_look(out, eye, &dir, up);
}

extern inline void fm_vec3_negate(fm_vec3_t *out, const fm_vec3_t *v);
extern inline void fm_vec3_add(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b);
extern inline void fm_vec3_sub(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b);
extern inline void fm_vec3_mul(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b);
extern inline void fm_vec3_div(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b);
extern inline void fm_vec3_scale(fm_vec3_t *out, const fm_vec3_t *a, float s);
extern inline float fm_vec3_dot(const fm_vec3_t *a, const fm_vec3_t *b);
extern inline float fm_vec3_len2(const fm_vec3_t *a);
extern inline float fm_vec3_len(const fm_vec3_t *a);
extern inline float fm_vec3_distance2(const fm_vec3_t *a, const fm_vec3_t *b);
extern inline float fm_vec3_distance(const fm_vec3_t *a, const fm_vec3_t *b);
extern inline void fm_vec3_norm(fm_vec3_t *out, const fm_vec3_t *a);
extern inline void fm_vec3_cross(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b);
extern inline void fm_vec3_lerp(fm_vec3_t *out, const fm_vec3_t *a, const fm_vec3_t *b, float t);
extern inline void fm_quat_identity(fm_quat_t *out);
extern inline void fm_quat_from_axis_angle(fm_quat_t *out, const fm_vec3_t *axis, float angle);
extern inline float fm_quat_dot(const fm_quat_t *a, const fm_quat_t *b);
extern inline void fm_quat_inverse(fm_quat_t *out, const fm_quat_t *q);
extern inline void fm_quat_norm(fm_quat_t *out, const fm_quat_t *q);
extern inline void fm_mat4_identity(fm_mat4_t *out);
extern inline void fm_mat4_scale(fm_mat4_t *out, const fm_vec3_t *scale);
extern inline void fm_mat4_translate(fm_mat4_t *out, const fm_vec3_t *translate);
extern inline void fm_mat4_from_rt(fm_mat4_t *out, const fm_quat_t *quat, const fm_vec3_t *translate);
extern inline void fm_mat4_from_rt_euler(fm_mat4_t *out, const float euler[3], const fm_vec3_t *translate);
extern inline void fm_mat4_from_translation(fm_mat4_t *out, const fm_vec3_t *translate);
extern inline void fm_mat4_from_rotation(fm_mat4_t *out, const fm_quat_t *rotation);
extern inline void fm_mat4_from_scale(fm_mat4_t *out, const fm_vec3_t *scale);
extern inline void fm_mat4_mul(fm_mat4_t *out, const fm_mat4_t *a, const fm_mat4_t *b);
extern inline void fm_mat4_transpose(fm_mat4_t *out, const fm_mat4_t *m);
extern inline void fm_mat4_mul_vec3(fm_vec4_t *out, const fm_mat4_t *m, const fm_vec3_t *v);
extern inline void fm_mat4_mul_vec4(fm_vec4_t *out, const fm_mat4_t *m, const fm_vec4_t *v);
