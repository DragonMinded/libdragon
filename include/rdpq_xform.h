/**
 * @file rdpq_xform.h
 * @brief RDP Command queue: 2D transformation API.
 * @ingroup rdpq
 * 
 * This file contains functions for 2D transformations and transformed primitives.
 */

#ifndef LIBDRAGON_RDPQ_XFORM_H
#define LIBDRAGON_RDPQ_XFORM_H

#include <stdint.h>
#include "preview.h"
#include "fgeom2d.h"
#include "rdpq_tri.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Replace current transformation matrix with another 3x3 matrix.
 * @preview
 *
 * @param mtx   Pointer to 3x3 matrix to load
 */
LIBDRAGON_PREVIEW_API
void rdpq_xform_mtx_load(fm_mat3_t *mtx);
/**
 * @brief Multiply current transformation matrix by another 3x3 matrix.
 * @preview
 *
 * @param mtx   Pointer to 3x3 matrix to multiply by
 */
LIBDRAGON_PREVIEW_API
void rdpq_xform_mtx_mult(fm_mat3_t *mtx);

/**
 * @brief Replace current transformation matrix with the identity matrix.
 * @preview
 *
 * This clears any transformations that may have been applied.
 */
LIBDRAGON_PREVIEW_API
void rdpq_xform_identity(void);
/**
 * @brief Multiplies current transformation matrix by rotation matrix.
 * @preview
 *
 * @param theta     Counterclockwise angle to rotate by in radians
 */
LIBDRAGON_PREVIEW_API
void rdpq_xform_rotate(float theta);
/**
 * @brief Multiplies current transformation matrix by translation matrix.
 * @preview
 *
 * @param x     X component of translation vector
 * @param y     Y component of translation vector
 */
LIBDRAGON_PREVIEW_API
void rdpq_xform_translate(float x, float y);
/**
 * @brief Multiplies current transformation matrix by scaling matrix.
 * @preview
 *
 * @param x     Scaling factor along X axis
 * @param y     Scaling factor along Y axis
 */
LIBDRAGON_PREVIEW_API
void rdpq_xform_scale(float x, float y);
/**
 * @brief Push current matrix to matrix stack.
 * @preview
 */
LIBDRAGON_PREVIEW_API
void rdpq_xform_push(void);
/**
 * @brief Pop from matrix stack.
 * @preview
 *
 * This replaces the current matrix with the matrix one below it on the stack.
 */
LIBDRAGON_PREVIEW_API
void rdpq_xform_pop(void);

/**
 * @brief Replace current transformation matrix with a combined scale, rotation and translation matrix
 * @preview
 *
 * It is equivalent to loading an identity matrix followed by multiplying
 * by a scale, a rotation, and a translation matrix in that order.
 *
 * @param x         X component of translation vector
 * @param y         Y component of translation vector
 * @param theta     Counterclockwise angle to rotate by in radians
 * @param scale_x   Scaling factor along X axis
 * @param scale_y   Scaling factor along Y axis
 */
LIBDRAGON_PREVIEW_API
void rdpq_xform_load_srt(float x, float y, float theta, float scale_x, float scale_y);
/**
 * @brief Multiply current transformation matrix by a combined scale, rotation and translation matrix
 * @preview
 *
 * It is equivalent to multiplying by a scale, a rotation, and a translation matrix in that order.
 *
 * @param x         X component of translation vector
 * @param y         Y component of translation vector
 * @param theta     Counterclockwise angle to rotate by in radians
 * @param scale_x   Scaling factor along X axis
 * @param scale_y   Scaling factor along Y axis
 */
LIBDRAGON_PREVIEW_API
void rdpq_xform_mult_srt(float x, float y, float theta, float scale_x, float scale_y);

/**
 * @brief Replace current transformation matrix with a combined rotation, scale and translation matrix
 * @preview
 *
 * It is equivalent to loading an identity matrix followed by multiplying
 * by a rotation, a scale, and a translation matrix in that order.
 *
 * @param x         X component of translation vector
 * @param y         Y component of translation vector
 * @param theta     Counterclockwise angle to rotate by in radians
 * @param scale_x   Scaling factor along X axis
 * @param scale_y   Scaling factor along Y axis
 */
LIBDRAGON_PREVIEW_API
void rdpq_xform_load_rst(float x, float y, float theta, float scale_x, float scale_y);
/**
 * @brief Multiply current transformation matrix by a combined rotation, scale and translation matrix
 * @preview
 *
 * It is equivalent to multiplying by a rotation, a scale, and a translation matrix in that order.
 *
 * @param x         X component of translation vector
 * @param y         Y component of translation vector
 * @param theta     Counterclockwise angle to rotate by in radians
 * @param scale_x   Scaling factor along X axis
 * @param scale_y   Scaling factor along Y axis
 */
LIBDRAGON_PREVIEW_API
void rdpq_xform_mult_rst(float x, float y, float theta, float scale_x, float scale_y);

/**
 * @brief Draw a filled rectangle with the current transformation applied.
 * @preview
 *
 * This function works as a replacement for #rdpq_fill_rectangle in contexts
 * where a transformation should be applied.
*/
LIBDRAGON_PREVIEW_API
void rdpq_xform_fill_rectangle(float x0, float y0, float x1, float y1);

/**
 * @brief Draw a filled rectangle with the current transformation applied.
 * @preview
 *
 * This function works as a replacement for #rdpq_texture_rectangle in contexts
 * where a transformation should be applied.
*/
LIBDRAGON_PREVIEW_API
void rdpq_xform_texture_rectangle(rdpq_tile_t tile, float x0, float y0, float x1, float y1, float s, float t);
/**
 * @brief Draw a textured rectangle with the current transformation applied and scaling applied.
 * @preview
 *
 * This function works as a replacement for #rdpq_texture_rectangle_scaled in contexts
 * where a transformation should be applied.
*/
LIBDRAGON_PREVIEW_API
void rdpq_xform_texture_rectangle_scaled(rdpq_tile_t tile, float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1);
/**
 * @brief Draw a triangle with the current transformation applied.
 * @preview
 *
 * This function works as a replacement for #rdpq_triangle in contexts
 * where a transformation should be applied.
*/
LIBDRAGON_PREVIEW_API
void rdpq_xform_triangle(const rdpq_trifmt_t *fmt, const float *v1, const float *v2, const float *v3);

#ifdef __cplusplus
}
#endif

#endif
