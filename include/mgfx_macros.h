/**
 * @file mgfx_macros.h
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 * @brief Helper macros for mgfx
 * @ingroup magma
 */

#ifndef __LIBDRAGON_MGFX_MACROS_H
#define __LIBDRAGON_MGFX_MACROS_H

#include "mgfx_constants.h"

/** @brief Convert a number fixed point format with f fractional bits. */
#define MGFX_FIXED_POINT(v, f)   ((v)*(1<<(f)))

/** @brief Convert a number to the format used for vertex positions in mgfx. */
#define MGFX_POS_COORD(v)   MGFX_FIXED_POINT((v), MGFX_VTX_POS_SHIFT)

/** @brief Convert a number to the format used for texture coordinates in mgfx. */
#define MGFX_TEX_COORD(v)   MGFX_FIXED_POINT((v), MGFX_VTX_TEX_SHIFT)

/** @brief Construct a vertex position in a format supported by the mgfx shader. */
#define MGFX_POS(x, y, z)   { MGFX_POS_COORD(x), MGFX_POS_COORD(y), MGFX_POS_COORD(z) }

/** @brief Construct a texture coordinate in a format supported by the mgfx shader. */
#define MGFX_TEX(s, t)      { MGFX_TEX_COORD(s), MGFX_TEX_COORD(t) }

/** @brief Construct a vertex normal in a format supported by the mgfx shader. */
#define MGFX_NRM(x, y, z)   ((((x) & 0x1F)<<11) | \
                             (((y) & 0x3F)<<5)  | \
                             (((z) & 0x1F)<<0))

#endif
