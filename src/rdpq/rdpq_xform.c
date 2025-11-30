/**
 * @file rdpq_xform.c
 */
#include "fgeom2d.h"
#include "rdpq_xform_internal.h"
#include "rdpq_internal.h"
#include "rdpq_xform.h"
#include "rdpq_rect.h"
#include "rdpq_tri.h"

#include "debug.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/**
 * @brief Type of the transformation
 * 
 * This affects the type of primitive required to be generated when drawing
 * transformed rectangles (both texture and fill rectangles).
 *
 * Ordering is based on the primitive that must be emitted after the transformation.
 * Earlier values can use more direct paths for emitting the primitive.
 */
typedef enum {
    RDPQ_XFORM_NONE, ///< No transformation
    RDPQ_XFORM_TRANSLATE, ///< Transformation with only translation
    RDPQ_XFORM_SCALE, ///< Any transformation that has no rotation or skew
    RDPQ_XFORM_ALL ///< Any 2D transformation
} rdpq_xform_type;

#define RDPQ_XFORM_STACK_SIZE 16

/**
 * @brief Data for RDPQ transformation
 */
typedef struct rdpq_xform_s {
    fm_mat3_t mtx; ///< 3x3 Transformation matrix associated with transformation
    rdpq_xform_type type; ///< Type of transformation
} rdpq_xform_t;

static rdpq_xform_t *curr_xform;
static rdpq_xform_t xform_stack[RDPQ_XFORM_STACK_SIZE];
static int xform_stack_depth;

void __rdpq_xform_init(void)
{
    //Initialize matrix stack to point to top
    xform_stack_depth = 0;
    curr_xform = &xform_stack[xform_stack_depth];
    rdpq_xform_identity();
}

void __rdpq_xform_close(void)
{
    //Make sure that trying to push/load matrices causes crashes
    curr_xform = NULL;
}

static rdpq_xform_type get_mtx_type(fm_mat3_t *mtx)
{
    if(mtx->m[0][1] != 0.0f || mtx->m[1][0] != 0.0f) { //Check for skew/rotation
        return RDPQ_XFORM_ALL;
    } else if(mtx->m[0][0] != 1.0f || mtx->m[1][1] != 1.0f) { //Check for non-identity scale
        return RDPQ_XFORM_SCALE;
    } else if(mtx->m[2][0] != 0.0f || mtx->m[2][1] != 0.0f) { //Check for translation
        return RDPQ_XFORM_TRANSLATE;
    } else {
        return RDPQ_XFORM_NONE;
    }
}

void rdpq_xform_mtx_load(fm_mat3_t *mtx)
{
    curr_xform->mtx = *mtx;
    curr_xform->type = get_mtx_type(mtx);
}

void rdpq_xform_mtx_mult(fm_mat3_t *mtx)
{
    fm_mat3_mul(&curr_xform->mtx, &curr_xform->mtx, mtx);
    //Prefer least strict transformation type
    int type = get_mtx_type(mtx);
    if(type > curr_xform->type) {
        curr_xform->type = type;
    }
}

void rdpq_xform_identity(void)
{
    fm_mat3_t mtx;
    fm_mat3_identity(&mtx);
    rdpq_xform_mtx_load(&mtx);
}

void rdpq_xform_rotate(float theta)
{
    fm_mat3_t mtx;
    fm_mat3_from_rotation(&mtx, theta);
    rdpq_xform_mtx_mult(&mtx);
}

void rdpq_xform_translate(float x, float y)
{
    fm_mat3_t mtx;
    fm_vec2_t translate = {{ x, y }};
    fm_mat3_from_translation(&mtx, &translate);
    rdpq_xform_mtx_mult(&mtx);
}

void rdpq_xform_scale(float x, float y)
{
    fm_mat3_t mtx;
    fm_vec2_t scale = {{ x, y }};
    fm_mat3_from_scale(&mtx, &scale);
    rdpq_xform_mtx_mult(&mtx);
}

void rdpq_xform_load_rst(float x, float y, float theta, float scale_x, float scale_y)
{
    fm_mat3_t mtx;
    fm_vec2_t scale = {{ scale_x, scale_y }};
    fm_vec2_t translate = {{ x, y }};
    fm_mat3_from_rst(&mtx, theta, &scale, &translate);
    rdpq_xform_mtx_load(&mtx);
}

void rdpq_xform_mult_rst(float x, float y, float theta, float scale_x, float scale_y)
{
    fm_mat3_t mtx;
    fm_vec2_t scale = {{ scale_x, scale_y }};
    fm_vec2_t translate = {{ x, y }};
    fm_mat3_from_rst(&mtx, theta, &scale, &translate);
    rdpq_xform_mtx_mult(&mtx);
}

void rdpq_xform_load_srt(float x, float y, float theta, float scale_x, float scale_y)
{
    fm_mat3_t mtx;
    fm_vec2_t scale = {{ scale_x, scale_y }};
    fm_vec2_t translate = {{ x, y }};
    fm_mat3_from_srt(&mtx, &scale, theta, &translate);
    rdpq_xform_mtx_load(&mtx);
}

void rdpq_xform_mult_srt(float x, float y, float theta, float scale_x, float scale_y)
{
    fm_mat3_t mtx;
    fm_vec2_t scale = {{ scale_x, scale_y }};
    fm_vec2_t translate = {{ x, y }};
    fm_mat3_from_srt(&mtx, &scale, theta, &translate);
    rdpq_xform_mtx_mult(&mtx);
}

void rdpq_xform_push(void)
{
    int new_depth = xform_stack_depth+1;
    assertf(curr_xform, "RDPQ Transform not Initialized");
    assertf(new_depth < RDPQ_XFORM_STACK_SIZE, "RDPQ Transform Stack has reached maximum depth of %d", RDPQ_XFORM_STACK_SIZE);
    //Copy matrix to top of stack
    xform_stack[new_depth] = xform_stack[new_depth-1];
    xform_stack_depth = new_depth;
    //Update current matrix
    curr_xform = &xform_stack[new_depth];
}

void rdpq_xform_pop(void)
{
    int new_depth = xform_stack_depth-1;
    assertf(curr_xform, "RDPQ Transform not Initialized");
    assertf(new_depth >= 0, "RDPQ Transform Stack is already at depth 0");
    //Update current matrix and depth
    curr_xform = &xform_stack[new_depth];
    xform_stack_depth = new_depth;
}

static fm_vec2_t transform_point(float x, float y)
{
    fm_vec2_t in = {{x, y}};
    fm_vec3_t temp;
    fm_mat3_mul_vec2(&temp, &curr_xform->mtx, &in);
    return (fm_vec2_t){{temp.x, temp.y}};
}

void rdpq_xform_fill_rectangle(float x0, float y0, float x1, float y1)
{
    switch(curr_xform->type) {
        case RDPQ_XFORM_NONE:
            rdpq_fill_rectangle(x0, y0, x1, y1);
            break;
        
        case RDPQ_XFORM_TRANSLATE:
            //Apply X Translation to x0 and x1
            x0 += curr_xform->mtx.m[2][0];
            x1 += curr_xform->mtx.m[2][0];
            //Apply Y Translation to y0 and y1
            y0 += curr_xform->mtx.m[2][1];
            y1 += curr_xform->mtx.m[2][1];
            rdpq_fill_rectangle(x0, y0, x1, y1);
            break;
        
        case RDPQ_XFORM_SCALE:
        {
            //Transform corner vertex coordinates
            fm_vec2_t p0 = transform_point(x0, y0);
            fm_vec2_t p1 = transform_point(x1, y1);
            rdpq_fill_rectangle(p0.x, p0.y, p1.x, p1.y);
        }
            break;
        
        case RDPQ_XFORM_ALL:
        {
            //Transform corner vertex coordinates
            fm_vec2_t p0 = transform_point(x0, y0);
            fm_vec2_t p1 = transform_point(x1, y0);
            fm_vec2_t p2 = transform_point(x1, y1);
            fm_vec2_t p3 = transform_point(x0, y1);
            //Draw quadrilateral
            rdpq_triangle(&TRIFMT_FILL, p0.v, p1.v, p2.v);
            rdpq_triangle(&TRIFMT_FILL, p0.v, p2.v, p3.v);
        }
            break;
    }
}

void rdpq_xform_texture_rectangle(rdpq_tile_t tile, float x0, float y0, float x1, float y1, float s, float t)
{
    switch(curr_xform->type) {
        case RDPQ_XFORM_NONE:
            rdpq_texture_rectangle(tile, x0, y0, x1, y1, s, t);
            break;
        
        case RDPQ_XFORM_TRANSLATE:
            //Apply X Translation to x0 and x1
            x0 += curr_xform->mtx.m[2][0];
            x1 += curr_xform->mtx.m[2][0];
            //Apply Y Translation to y0 and y1
            y0 += curr_xform->mtx.m[2][1];
            y1 += curr_xform->mtx.m[2][1];
            rdpq_texture_rectangle(tile, x0, y0, x1, y1, s, t);
            break;
        
        case RDPQ_XFORM_SCALE:
        {
            //Calculate texture coordinates
            //Absolute value of width (x1-x0) and height (y1-y0) are needed to
            //calculate texture coordinates for flipped textures properly
            float s0 = s;
            float t0 = t;
            float s1 = fabsf(x1-x0)+s0;
            float t1 = fabsf(y1-y0)+t0;
            //Transform corner vertex coordinates
            fm_vec2_t p0 = transform_point(x0, y0);
            fm_vec2_t p1 = transform_point(x1, y1);
            rdpq_texture_rectangle_scaled(tile, p0.x, p0.y, p1.x, p1.y, s0, t0, s1, t1);
        }
            break;
        
        case RDPQ_XFORM_ALL:
        {
            //Calculate texture coordinates
            //Absolute value of width (x1-x0) and height (y1-y0) are needed to
            //calculate texture coordinates for flipped textures properly
            float s0 = s;
            float t0 = t;
            float s1 = fabsf(x1-x0)+s0;
            float t1 = fabsf(y1-y0)+t0;
            //Transform corner vertex coordinates
            fm_vec2_t p0 = transform_point(x0, y0);
            fm_vec2_t p1 = transform_point(x1, y0);
            fm_vec2_t p2 = transform_point(x1, y1);
            fm_vec2_t p3 = transform_point(x0, y1);

            //Generate vertex buffers for pair of triangles
            float v0[5] = { p0.x, p0.y, s0, t0, 1.0f };
            float v1[5] = { p1.x, p1.y, s1, t0, 1.0f };
            float v2[5] = { p2.x, p2.y, s1, t1, 1.0f };
            float v3[5] = { p3.x, p3.y, s0, t1, 1.0f };
            //Draw quadrilateral
            rdpq_triangle(&TRIFMT_TEX, v0, v1, v2);
            rdpq_triangle(&TRIFMT_TEX, v0, v2, v3);
        }
            break;
    }
}

void rdpq_xform_texture_rectangle_scaled(rdpq_tile_t tile, float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1)
{
    switch(curr_xform->type) {
        case RDPQ_XFORM_NONE:
            rdpq_texture_rectangle_scaled(tile, x0, y0, x1, y1, s0, t0, s1, t1);
            break;
        
        case RDPQ_XFORM_TRANSLATE:
            x0 += curr_xform->mtx.m[2][0];
            x1 += curr_xform->mtx.m[2][0];
            //Apply Y Translation to y0 and y1
            y0 += curr_xform->mtx.m[2][1];
            y1 += curr_xform->mtx.m[2][1];
            rdpq_texture_rectangle_scaled(tile, x0, y0, x1, y1, s0, t0, s1, t1);
            break;
        
        case RDPQ_XFORM_SCALE:
        {
            //Transform corner vertex coordinates
            fm_vec2_t p0 = transform_point(x0, y0);
            fm_vec2_t p1 = transform_point(x1, y1);
            rdpq_texture_rectangle_scaled(tile, p0.x, p0.y, p1.x, p1.y, s0, t0, s1, t1);
        }
            break;
        
        case RDPQ_XFORM_ALL:
        {
            //Transform corner vertex coordinates
            fm_vec2_t p0 = transform_point(x0, y0);
            fm_vec2_t p1 = transform_point(x1, y0);
            fm_vec2_t p2 = transform_point(x1, y1);
            fm_vec2_t p3 = transform_point(x0, y1);

            //Generate vertex buffers for pair of triangles
            float v0[5] = { p0.x, p0.y, s0, t0, 1.0f };
            float v1[5] = { p1.x, p1.y, s1, t0, 1.0f };
            float v2[5] = { p2.x, p2.y, s1, t1, 1.0f };
            float v3[5] = { p3.x, p3.y, s0, t1, 1.0f };
            //Draw quadrilateral
            rdpq_triangle(&TRIFMT_TEX, v0, v1, v2);
            rdpq_triangle(&TRIFMT_TEX, v0, v2, v3);
        }
            break;
    }
}

void rdpq_xform_triangle(const rdpq_trifmt_t *fmt, const float *v1, const float *v2, const float *v3)
{
    switch(curr_xform->type) {
        case RDPQ_XFORM_NONE:
            rdpq_triangle(fmt, v1, v2, v3);
            break;
            
        case RDPQ_XFORM_TRANSLATE:
        case RDPQ_XFORM_SCALE:
        case RDPQ_XFORM_ALL:
            //Call RSP Triangle Implementation Directly
            rdpq_triangle_rsp(fmt, v1, v2, v3, &curr_xform->mtx);
            break;
    }
}
