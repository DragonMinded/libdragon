#ifndef __LIBDRAGON_FLOAT4_H_
#define	__LIBDRAGON_FLOAT4_H_

/**
 * @addtogroup float4
 * @{
 */


/**
 * @brief Structure representing a float 4 vector
 */
typedef struct 
{
    float x,y,z,w;
} float4;

#ifdef __cplusplus
extern "C" {
#endif

#define f4_add_fast(outv, a, b) outv.x=a.x+b.x;outv.y=a.y+b.y;outv.z=a.z+b.z;outv.w=a.w+b.w;

float4 f4_set3(float x, float y, float z);
float4 f4_zero();
float4 f4_add(float4 a, float4 b);
float4 f4_sub(float4 a, float4 b);
float4 f4_mul(float4 a, float4 b);
float4 f4_mul_f(float4 a, float b);
float4 f4_div_f(float4 a, float b);
float4 ang2f4(float ang_x, float ang_y);

float f4_dot3(float4 a, float4 b);
float f4_dot4(float4 a, float4 b);

float4 f4_create_plane(float4 normal, float4 position);
float f4_plane_test(float4 plane, float4 point);

float f4_distance(float4 a, float4 b);
float f4_length(float4 v);
float4 f4_normal(float4 v);
float4 f4_cross(float4 a, float4 b);
float4 f4_calcnormal(float4 v1, float4 v2, float4 v3);

float4 f4_persp(float4 v);

#ifdef __cplusplus
}
#endif

#endif //__LIBDRAGON_FLOAT4_H_
