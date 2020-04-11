#ifndef __LIBDRAGON_MATRIX_H_
#define	__LIBDRAGON_MATRIX_H_

#include <float4.h>

/**
 * @addtogroup matrix4
 * @{
 */


/**
 * @brief Structure representing a float 16 matrix
 */
typedef struct 
{
    float m[4][4];
} matrix4;


#ifdef __cplusplus
extern "C" {
#endif

matrix4 m4_identity();

void m4_translate(matrix4* m, float4 translate);
void m4_scale(matrix4* m, float4 scale);
void m4_rotate(matrix4* m, float4 ang);
matrix4 m4_transpose(matrix4 m);

float4 m4_mul_f(matrix4 m, float4 vec);
matrix4 m4_mul_m(matrix4 m1, matrix4 m2);

matrix4 m4_projection(float FOV, float aspect, float zNear, float zFar);
matrix4 m4_lookAt(float4 Eye, float4 At, float4 Up);

#ifdef __cplusplus
}
#endif

#endif //__LIBDRAGON_VECTOR_H_
