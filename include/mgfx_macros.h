/**
 * @file mgfx_macros.h
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 * @brief Helper macros for mgfx
 * @ingroup magma
 */

#ifndef __LIBDRAGON_MGFX_MACROS_H
#define __LIBDRAGON_MGFX_MACROS_H

#include "mgfx_constants.h"

/** @brief Convert a number to s10.5 format, which is used for vertex positions in mgfx. */
#define MGFX_S10_5(v)       ((v)*(1<<MGFX_VTX_POS_SHIFT))

/** @brief Convert a number to s7.8 format, which may be used for texture coordinates in mgfx (Though any signed fixed point format is accepted). */
#define MGFX_S7_8(v)        ((v)*(1<<MGFX_VTX_TEX_SHIFT))

/** @brief Construct a vertex position in a format supported by the mgfx shader. */
#define MGFX_POS(x, y, z)   { MGFX_S10_5(x), MGFX_S10_5(y), MGFX_S10_5(z) }

/** @brief Construct a texture coordinate in a format supported by the mgfx shader. */
#define MGFX_TEX(s, t)      { MGFX_S7_8(s), MGFX_S7_8(t) }

/** @brief Construct a vertex normal in a format supported by the mgfx shader. */
#define MGFX_NRM(x, y, z)   ((((x) & 0x1F)<<11) | \
                             (((y) & 0x3F)<<5)  | \
                             (((z) & 0x1F)<<0))

#endif
