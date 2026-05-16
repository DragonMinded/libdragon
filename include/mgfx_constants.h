/**
 * @file mgfx_constants.h
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 * @brief Constant definitions for mgfx
 * @ingroup magma
 */

#ifndef __MGFX_CONSTANTS
#define __MGFX_CONSTANTS

/** 
 * @brief Input number of the "Position" vertex input.
 * 
 * This attribute expects the object space position as three 16-bit signed integers in s10.5 format.
 * It is equivalent to:
 * 
 * @code{.c}
 *      struct position {
 *          int16_t x;
 *          int16_t y;
 *          int16_t z;
 *      }
 * @endcode
 */
#define MGFX_ATTRIBUTE_POSITION         0

/** 
 * @brief Input number of the "Normal" vertex input. 
 * 
 * This attribute expects the object space normal as three normalized signed integers (in the range -1 to 1) packed into 16 bits.
 * The x, y and z components are encoded as 5, 6 and 5 bits each, like so:
 * |16----------------------------0|
 * |x x x x x y y y y y y z z z z z|
 */
#define MGFX_ATTRIBUTE_NORMAL           1

/**
 * @brief Input number of the "Color" vertex input.
 * 
 * This attribute expects the RGBA color as four normalized 8-bit unsigned integers (in the range 0 to 1).
 * It is equivalent to:
 * 
 * @code{.c}
 *      struct color {
 *          uint8_t r;
 *          uint8_t g;
 *          uint8_t b;
 *          uint8_t a;
 *      }
 * @endcode
 */
#define MGFX_ATTRIBUTE_COLOR            2

/**
 * @brief Input number of the "Texture coordinate" vertex input.
 * 
 * This attribute expects the texture coordinates as two 16-bit signed integers.
 * The scale and offset of the texture coordinates can be adjusted in #mgfx_texturing_parms_t.
 * The format is equivalent to:
 * 
 * @code{.c}
 *      struct texcoord {
 *          int16_t s;
 *          int16_t t;
 *      }
 * @endcode
 */
#define MGFX_ATTRIBUTE_TEXCOORD         3

/**
 * @brief Binding number of the "Matrices" uniform.
 * 
 * @see #mgfx_get_matrices
 * @see #mgfx_set_matrices_inline
 */
#define MGFX_BINDING_MATRICES           0

/** 
 * @brief Binding number of the "Texturing" uniform.
 * 
 * @see #mgfx_get_texturing
 * @see #mgfx_set_texturing_inline
 */
#define MGFX_BINDING_TEXTURING          1

/** 
 * @brief Binding number of the "Lighting" uniform.
 * 
 * @see #mgfx_get_lighting
 * @see #mgfx_set_lighting_inline
 */
#define MGFX_BINDING_LIGHTING           2

/**
 * @brief Binding number of the "Fog" uniform.
 * 
 * @see #mgfx_get_fog
 * @see #mgfx_set_fog_inline
 */
#define MGFX_BINDING_FOG                3

/** @brief The maximum number of lights. */
#define MGFX_LIGHT_COUNT_MAX    8

/// @cond

#define MGFX_LIGHT_POSITION     0
#define MGFX_LIGHT_POSITIONW    6
#define MGFX_LIGHT_COLOR        8
#define MGFX_LIGHT_INTENSITY    14
#define MGFX_LIGHT_SIZE         16

#define MGFX_MATRIX_SIZE        64

#define MGFX_VTX_POS_SHIFT      5
#define MGFX_VTX_TEX_SHIFT      8

/// @endcond

#endif
