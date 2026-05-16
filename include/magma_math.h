/**
 * @file magma_math.h
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 * @brief Math helper functions for magma
 * @ingroup magma
 */

#ifndef __MAGMA_MATH
#define __MAGMA_MATH

#include "fgeom.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a magma-compatible perspective projection matrix.
 * 
 * The resulting eye space coordinate system is right handed, with positive X pointing right,
 * positive Y pointing up, and positive Z pointing towards the camera (out of the screen).
 */
void mg_mat4_perspective(fm_mat4_t *out, float fovy, float aspect, float z_near, float z_far);

/**
 * @brief Create a magma-compatible orthographic projection matrix.
 * 
 * The resulting eye space coordinate system is right handed, with positive X pointing right,
 * positive Y pointing up, and positive Z pointing towards the camera (out of the screen).
 */
void mg_mat4_ortho(fm_mat4_t *out, float l, float r, float b, float t, float n, float f);

#ifdef __cplusplus
}
#endif

#endif
