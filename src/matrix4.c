/**
 * @file matrix4.c
 * @brief Matrix 16 (4*float4) vector mathematics
 * @ingroup matrix4
 */
#include <matrix4.h>
#include <math.h>


/** @brief Create an identity matrix */
matrix4 m4_identity()
{
    matrix4 out;

    for(int i=0; i<4; ++i)
        for(int j=0; j<4; ++j)
            out.m[i][j] = 0;

    out.m[0][0] = 1;
    out.m[1][1] = 1;
    out.m[2][2] = 1;
    out.m[3][3] = 1;

    return out;
}


/** @brief Add a float4 transformation to the matrix */
void m4_translate(matrix4* m, float4 translate)
{
    m->m[0][3] = translate.x;
    m->m[1][3] = translate.y;
    m->m[2][3] = translate.z;
}

/** @brief Add a float4 scale to the matrix */
void m4_scale(matrix4* m, float4 scale)
{
    m->m[0][0] *= scale.x;
    m->m[1][1] *= scale.y;
    m->m[2][2] *= scale.z;
}

/** @brief Add a float4 rotation to the matrix */
void m4_rotate(matrix4* m, float4 ang)
{
    float  angle;
    float  sr, sp, sy, cr, cp, cy;

    angle = ang.z;
    sy = sinf(angle);
    cy = cosf(angle);
    angle = ang.y;
    sp = sinf(angle);
    cp = cosf(angle);
    angle = ang.x;
    sr = sinf(angle);
    cr = cosf(angle);

    m->m[0][0] = cp*cy;
    m->m[1][0] = cp*sy;
    m->m[2][0] = -sp;
    m->m[3][0] = 0.0f;
    m->m[0][1] = sr*sp*cy + cr*-sy;
    m->m[1][1] = sr*sp*sy + cr*cy;
    m->m[2][1] = sr*cp;
    m->m[3][1] = 0.0f;
    m->m[0][2] = (cr*sp*cy + -sr*-sy);
    m->m[1][2] = (cr*sp*sy + -sr*cy);
    m->m[2][2] = cr*cp;
    m->m[3][2] = 0.0f;
    m->m[0][3] = 0.0;
    m->m[1][3] = 0.0;
    m->m[2][3] = 0.0;
    m->m[3][3] = 1.0f;
}


/** @brief Transform a float4 object by a Matrix */
float4 m4_mul_f(matrix4 m, float4 vec)
{
    float4 temp;
    temp.x = m.m[0][0] * vec.x + m.m[0][1] * vec.y + m.m[0][2] * vec.z + m.m[0][3] * vec.w;// - mat[0][3];
    temp.y = m.m[1][0] * vec.x + m.m[1][1] * vec.y + m.m[1][2] * vec.z + m.m[1][3] * vec.w;// - mat[1][3];
    temp.z = m.m[2][0] * vec.x + m.m[2][1] * vec.y + m.m[2][2] * vec.z + m.m[2][3] * vec.w;// - mat[2][3];
    temp.w = m.m[3][0] * vec.x + m.m[3][1] * vec.y + m.m[3][2] * vec.z + m.m[3][3] * vec.w;

    return temp;
}


/** @brief Multiply two matrix objects together */
matrix4 m4_mul_m(matrix4 m1, matrix4 m2)
{
    matrix4 ret;

#define MatrixMul(i,j) ret.m[i][j]=m1.m[i][0]*m2.m[0][j]+ \
                                    m1.m[i][1]*m2.m[1][j]+ \
                                    m1.m[i][2]*m2.m[2][j]+ \
                                    m1.m[i][3]*m2.m[3][j];

#define MatrixMulLoop(i) MatrixMul(i,0) \
                                MatrixMul(i,1) \
                                MatrixMul(i,2) \
                                MatrixMul(i,3)

    MatrixMulLoop(0)
    MatrixMulLoop(1)
    MatrixMulLoop(2)
    MatrixMulLoop(3)


#undef MatrixMul
#undef MatrixMulLoop

    return ret;
}


/** @brief Transpose a matrix object */
matrix4 m4_transpose(matrix4 m)
{
    matrix4 r;
    r.m[0][0] = m.m[0][0];
    r.m[0][1] = m.m[1][0];
    r.m[0][2] = m.m[2][0];
    r.m[0][3] = m.m[3][0];
    r.m[1][0] = m.m[0][1];
    r.m[1][1] = m.m[1][1];
    r.m[1][2] = m.m[2][1];
    r.m[1][3] = m.m[3][1];
    r.m[2][0] = m.m[0][2];
    r.m[2][1] = m.m[1][2];
    r.m[2][2] = m.m[2][2];
    r.m[2][3] = m.m[3][2];
    r.m[3][0] = m.m[0][3];
    r.m[3][1] = m.m[1][3];
    r.m[3][2] = m.m[2][3];
    r.m[3][3] = m.m[3][3];
    return r;
}


/** @brief Generate a Projection matrix  */
matrix4 m4_projection(float FOV, float aspect, float zNear, float zFar)
{
    matrix4 ret;
    float yscale, xscale;
    yscale = cos(FOV / 2) / sin(FOV / 2);
    xscale = yscale / aspect;

    ret.m[0][0] = xscale;
    ret.m[0][1] = 0.0f;
    ret.m[0][2] = 0.0f;
    ret.m[0][3] = 0.0f;

    ret.m[1][0] = 0.0f;
    ret.m[1][1] = yscale;
    ret.m[1][2] = 0.0f;
    ret.m[1][3] = 0.0f;

    ret.m[2][0] = 0.0f;
    ret.m[2][1] = 0.0f;

    ret.m[2][2] = (zFar + zNear) / (zNear - zFar);
    ret.m[2][3] = (2 * zFar * zNear) / (zNear - zFar);

    ret.m[3][0] = 0.0f;
    ret.m[3][1] = 0.0f;
    ret.m[3][2] = -1.0f;
    ret.m[3][3] = 0.0f;
    return ret;
}

/** @brief Genreate a camera Look At view matrix */
matrix4 m4_lookAt(float4 Eye, float4 At, float4 Up)
{
    matrix4 ret;

    At.x=Eye.x-At.x;
    At.y=Eye.y-At.y;
    At.z=Eye.z-At.z;
    float4 zaxis = f4_normal(At);
    float4 cross = f4_cross(Up,zaxis);
    float4 xaxis = f4_normal(cross);
    float4 yaxis = f4_cross(xaxis,zaxis);

    ret.m[0][0] = xaxis.x;
    ret.m[1][0] = yaxis.x;
    ret.m[2][0] = zaxis.x;
    ret.m[3][0] = 0.0f;

    ret.m[0][1] = xaxis.y;
    ret.m[1][1] = yaxis.y;
    ret.m[2][1] = zaxis.y;
    ret.m[3][1] = 0.0f;

    ret.m[0][2] = xaxis.z;
    ret.m[1][2] = yaxis.z;
    ret.m[2][2] = zaxis.z;
    ret.m[3][2] = 0.0f;

    ret.m[0][3] = -f4_dot3(xaxis, Eye);
    ret.m[1][3] = -f4_dot3(yaxis, Eye);
    ret.m[2][3] = -f4_dot3(zaxis, Eye);
    ret.m[3][3] = 1.0f;

    return ret;
}