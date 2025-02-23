/**
 * @file mgfx_constants.h
 * @brief Constant definitions for mgfx
 * @ingroup magma
 */

#ifndef __MGFX_CONSTANTS
#define __MGFX_CONSTANTS

/** @brief Input number of the "Position/Normal" vertex input. */
#define MGFX_ATTRIBUTE_POS_NORM         0

/** @brief Input number of the "Color" vertex input. */
#define MGFX_ATTRIBUTE_COLOR            1

/** @brief Input number of the "Texture coordinate" vertex input. */
#define MGFX_ATTRIBUTE_TEXCOORD         2

/** @brief Binding number of the "Fog" uniform. */
#define MGFX_BINDING_FOG                0

/** @brief Binding number of the "Lighting" uniform. */
#define MGFX_BINDING_LIGHTING           1

/** @brief Binding number of the "Texturing" uniform. */
#define MGFX_BINDING_TEXTURING          2

/** @brief Binding number of the "Matrices" uniform. */
#define MGFX_BINDING_MATRICES           3

/** @brief The maximum number of lights. */
#define MGFX_LIGHT_COUNT_MAX    8

/// @cond

#define MGFX_LIGHT_POSITION     0
#define MGFX_LIGHT_COLOR        8
#define MGFX_LIGHT_ATT_INT      16
#define MGFX_LIGHT_ATT_FRAC     24
#define MGFX_LIGHT_SIZE         32

#define MGFX_MATRIX_SIZE        64

#define MGFX_VTX_POS_SHIFT      5

/// @endcond

#endif
