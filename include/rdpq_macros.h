/**
 * @file rdpq_macros.h
 * @brief RDP command macros
 * @ingroup rdp
 * 
 * This file contains macros that can be used to assembly some complex RDP commands:
 * the blender and the color combiner configurations.
 * 
 * The file is meant to be included also from RSP assembly code, for readability
 * while manipulating these commands.
 */
#ifndef LIBDRAGON_RDPQ_MACROS_H
#define LIBDRAGON_RDPQ_MACROS_H

#ifndef __ASSEMBLER__

/** @brief A combiner formula, created by #RDPQ_COMBINER1 or #RDPQ_COMBINER2 */
typedef uint64_t rdpq_combiner_t;
/** @brief A blender formula, created by #RDPQ_BLENDER or #RDPQ_BLENDER2 */
typedef uint32_t rdpq_blender_t;

#endif

///@cond
#ifndef __ASSEMBLER__
#include <stdint.h>
#define cast64(x) (uint64_t)(x)
#define castcc(x) (rdpq_combiner_t)(x)
#define castbl(x) (rdpq_blender_t)(x)
#else
#define cast64(x) x
#define castcc(x) x
#define castbl(x) x
#endif
///@endcond

/// @cond
// Internal helpers to build a color combiner setting
#define _RDPQ_COMB1_RGB_SUBA_TEX0      cast64(1)
#define _RDPQ_COMB1_RGB_SUBA_PRIM      cast64(3)
#define _RDPQ_COMB1_RGB_SUBA_SHADE     cast64(4)
#define _RDPQ_COMB1_RGB_SUBA_ENV       cast64(5)
#define _RDPQ_COMB1_RGB_SUBA_ONE       cast64(6)
#define _RDPQ_COMB1_RGB_SUBA_1         cast64(6)
#define _RDPQ_COMB1_RGB_SUBA_NOISE     cast64(7)
#define _RDPQ_COMB1_RGB_SUBA_ZERO      cast64(8)
#define _RDPQ_COMB1_RGB_SUBA_0         cast64(8)

#define _RDPQ_COMB2A_RGB_SUBA_TEX0      cast64(1)
#define _RDPQ_COMB2A_RGB_SUBA_TEX1      cast64(2)
#define _RDPQ_COMB2A_RGB_SUBA_PRIM      cast64(3)
#define _RDPQ_COMB2A_RGB_SUBA_SHADE     cast64(4)
#define _RDPQ_COMB2A_RGB_SUBA_ENV       cast64(5)
#define _RDPQ_COMB2A_RGB_SUBA_ONE       cast64(6)
#define _RDPQ_COMB2A_RGB_SUBA_1         cast64(6)
#define _RDPQ_COMB2A_RGB_SUBA_NOISE     cast64(7)
#define _RDPQ_COMB2A_RGB_SUBA_ZERO      cast64(8)
#define _RDPQ_COMB2A_RGB_SUBA_0         cast64(8)

#define _RDPQ_COMB2B_RGB_SUBA_COMBINED  cast64(0)
#define _RDPQ_COMB2B_RGB_SUBA_TEX1      cast64(1)
#define _RDPQ_COMB2B_RGB_SUBA_TEX0_BUG  cast64(2) // TEX0 is buggy in 2nd cycle: it refers to *next* pixel in the scanline
#define _RDPQ_COMB2B_RGB_SUBA_PRIM      cast64(3)
#define _RDPQ_COMB2B_RGB_SUBA_SHADE     cast64(4)
#define _RDPQ_COMB2B_RGB_SUBA_ENV       cast64(5)
#define _RDPQ_COMB2B_RGB_SUBA_ONE       cast64(6)
#define _RDPQ_COMB2B_RGB_SUBA_1         cast64(6)
#define _RDPQ_COMB2B_RGB_SUBA_NOISE     cast64(7)
#define _RDPQ_COMB2B_RGB_SUBA_ZERO      cast64(8)
#define _RDPQ_COMB2B_RGB_SUBA_0         cast64(8)

#define _RDPQ_COMB1_RGB_SUBB_TEX0      cast64(1)
#define _RDPQ_COMB1_RGB_SUBB_PRIM      cast64(3)
#define _RDPQ_COMB1_RGB_SUBB_SHADE     cast64(4)
#define _RDPQ_COMB1_RGB_SUBB_ENV       cast64(5)
#define _RDPQ_COMB1_RGB_SUBB_KEYCENTER cast64(6)
#define _RDPQ_COMB1_RGB_SUBB_K4        cast64(7)
#define _RDPQ_COMB1_RGB_SUBB_ZERO      cast64(8)
#define _RDPQ_COMB1_RGB_SUBB_0         cast64(8)

#define _RDPQ_COMB2A_RGB_SUBB_TEX0      cast64(1)
#define _RDPQ_COMB2A_RGB_SUBB_TEX1      cast64(2)
#define _RDPQ_COMB2A_RGB_SUBB_PRIM      cast64(3)
#define _RDPQ_COMB2A_RGB_SUBB_SHADE     cast64(4)
#define _RDPQ_COMB2A_RGB_SUBB_ENV       cast64(5)
#define _RDPQ_COMB2A_RGB_SUBB_KEYCENTER cast64(6)
#define _RDPQ_COMB2A_RGB_SUBB_K4        cast64(7)
#define _RDPQ_COMB2A_RGB_SUBB_ZERO      cast64(8)
#define _RDPQ_COMB2A_RGB_SUBB_0         cast64(8)

#define _RDPQ_COMB2B_RGB_SUBB_COMBINED  cast64(0)
#define _RDPQ_COMB2B_RGB_SUBB_TEX1      cast64(1)
#define _RDPQ_COMB2B_RGB_SUBA_TEX0_BUG  cast64(2) // TEX0 is buggy in 2nd cycle: it refers to *next* pixel in the scanline
#define _RDPQ_COMB2B_RGB_SUBB_PRIM      cast64(3)
#define _RDPQ_COMB2B_RGB_SUBB_SHADE     cast64(4)
#define _RDPQ_COMB2B_RGB_SUBB_ENV       cast64(5)
#define _RDPQ_COMB2B_RGB_SUBB_KEYCENTER cast64(6)
#define _RDPQ_COMB2B_RGB_SUBB_K4        cast64(7)
#define _RDPQ_COMB2B_RGB_SUBB_ZERO      cast64(8)
#define _RDPQ_COMB2B_RGB_SUBB_0         cast64(8)

#define _RDPQ_COMB1_RGB_MUL_TEX0           cast64(1)
#define _RDPQ_COMB1_RGB_MUL_PRIM           cast64(3)
#define _RDPQ_COMB1_RGB_MUL_SHADE          cast64(4)
#define _RDPQ_COMB1_RGB_MUL_ENV            cast64(5)
#define _RDPQ_COMB1_RGB_MUL_KEYSCALE       cast64(6)
#define _RDPQ_COMB1_RGB_MUL_TEX0_ALPHA     cast64(8)
#define _RDPQ_COMB1_RGB_MUL_PRIM_ALPHA     cast64(10)
#define _RDPQ_COMB1_RGB_MUL_SHADE_ALPHA    cast64(11)
#define _RDPQ_COMB1_RGB_MUL_ENV_ALPHA      cast64(12)
#define _RDPQ_COMB1_RGB_MUL_LOD_FRAC       cast64(13)
#define _RDPQ_COMB1_RGB_MUL_PRIM_LOD_FRAC  cast64(14)
#define _RDPQ_COMB1_RGB_MUL_K5             cast64(15)
#define _RDPQ_COMB1_RGB_MUL_ZERO           cast64(16)
#define _RDPQ_COMB1_RGB_MUL_0              cast64(16)

#define _RDPQ_COMB2A_RGB_MUL_TEX0           cast64(1)
#define _RDPQ_COMB2A_RGB_MUL_TEX1           cast64(2)
#define _RDPQ_COMB2A_RGB_MUL_PRIM           cast64(3)
#define _RDPQ_COMB2A_RGB_MUL_SHADE          cast64(4)
#define _RDPQ_COMB2A_RGB_MUL_ENV            cast64(5)
#define _RDPQ_COMB2A_RGB_MUL_KEYSCALE       cast64(6)
#define _RDPQ_COMB2A_RGB_MUL_TEX0_ALPHA     cast64(8)
#define _RDPQ_COMB2A_RGB_MUL_TEX1_ALPHA     cast64(9)
#define _RDPQ_COMB2A_RGB_MUL_PRIM_ALPHA     cast64(10)
#define _RDPQ_COMB2A_RGB_MUL_SHADE_ALPHA    cast64(11)
#define _RDPQ_COMB2A_RGB_MUL_ENV_ALPHA      cast64(12)
#define _RDPQ_COMB2A_RGB_MUL_LOD_FRAC       cast64(13)
#define _RDPQ_COMB2A_RGB_MUL_PRIM_LOD_FRAC  cast64(14)
#define _RDPQ_COMB2A_RGB_MUL_K5             cast64(15)
#define _RDPQ_COMB2A_RGB_MUL_ZERO           cast64(16)
#define _RDPQ_COMB2A_RGB_MUL_0              cast64(16)

#define _RDPQ_COMB2B_RGB_MUL_COMBINED       cast64(0)
#define _RDPQ_COMB2B_RGB_MUL_TEX1           cast64(1)
#define _RDPQ_COMB2B_RGB_MUL_TEX0_BUG       cast64(2)  // TEX0 is buggy in 2nd cycle: it refers to *next* pixel in the scanline
#define _RDPQ_COMB2B_RGB_MUL_PRIM           cast64(3)
#define _RDPQ_COMB2B_RGB_MUL_SHADE          cast64(4)
#define _RDPQ_COMB2B_RGB_MUL_ENV            cast64(5)
#define _RDPQ_COMB2B_RGB_MUL_KEYSCALE       cast64(6)
#define _RDPQ_COMB2B_RGB_MUL_COMBINED_ALPHA cast64(7)
#define _RDPQ_COMB2B_RGB_MUL_TEX1_ALPHA     cast64(8)
#define _RDPQ_COMB2B_RGB_MUL_TEX0_ALPHA     cast64(9) // TEX0_ALPHA is buggy in 2nd cycle: it refers to *next* pixel in the scanline
#define _RDPQ_COMB2B_RGB_MUL_PRIM_ALPHA     cast64(10)
#define _RDPQ_COMB2B_RGB_MUL_SHADE_ALPHA    cast64(11)
#define _RDPQ_COMB2B_RGB_MUL_ENV_ALPHA      cast64(12)
#define _RDPQ_COMB2B_RGB_MUL_LOD_FRAC       cast64(13)
#define _RDPQ_COMB2B_RGB_MUL_PRIM_LOD_FRAC  cast64(14)
#define _RDPQ_COMB2B_RGB_MUL_K5             cast64(15)
#define _RDPQ_COMB2B_RGB_MUL_ZERO           cast64(16)
#define _RDPQ_COMB2B_RGB_MUL_0              cast64(16)

#define _RDPQ_COMB1_RGB_ADD_TEX0      cast64(1)
#define _RDPQ_COMB1_RGB_ADD_PRIM      cast64(3)
#define _RDPQ_COMB1_RGB_ADD_SHADE     cast64(4)
#define _RDPQ_COMB1_RGB_ADD_ENV       cast64(5)
#define _RDPQ_COMB1_RGB_ADD_ONE       cast64(6)
#define _RDPQ_COMB1_RGB_ADD_1         cast64(6)
#define _RDPQ_COMB1_RGB_ADD_ZERO      cast64(7)
#define _RDPQ_COMB1_RGB_ADD_0         cast64(7)

#define _RDPQ_COMB2A_RGB_ADD_TEX0      cast64(1)
#define _RDPQ_COMB2A_RGB_ADD_TEX1      cast64(2)
#define _RDPQ_COMB2A_RGB_ADD_PRIM      cast64(3)
#define _RDPQ_COMB2A_RGB_ADD_SHADE     cast64(4)
#define _RDPQ_COMB2A_RGB_ADD_ENV       cast64(5)
#define _RDPQ_COMB2A_RGB_ADD_ONE       cast64(6)
#define _RDPQ_COMB2A_RGB_ADD_1         cast64(6)
#define _RDPQ_COMB2A_RGB_ADD_ZERO      cast64(7)
#define _RDPQ_COMB2A_RGB_ADD_0         cast64(7)

#define _RDPQ_COMB2B_RGB_ADD_COMBINED  cast64(0)
#define _RDPQ_COMB2B_RGB_ADD_TEX1      cast64(1)
#define _RDPQ_COMB2B_RGB_ADD_TEX0_BUG  cast64(2)  // TEX0 is buggy in 2nd cycle: it refers to *next* pixel in the scanline
#define _RDPQ_COMB2B_RGB_ADD_PRIM      cast64(3)
#define _RDPQ_COMB2B_RGB_ADD_SHADE     cast64(4)
#define _RDPQ_COMB2B_RGB_ADD_ENV       cast64(5)
#define _RDPQ_COMB2B_RGB_ADD_ONE       cast64(6)
#define _RDPQ_COMB2B_RGB_ADD_1         cast64(6)
#define _RDPQ_COMB2B_RGB_ADD_ZERO      cast64(7)
#define _RDPQ_COMB2B_RGB_ADD_0         cast64(7)

#define _RDPQ_COMB1_ALPHA_ADDSUB_TEX0      cast64(1)
#define _RDPQ_COMB1_ALPHA_ADDSUB_PRIM      cast64(3)
#define _RDPQ_COMB1_ALPHA_ADDSUB_SHADE     cast64(4)
#define _RDPQ_COMB1_ALPHA_ADDSUB_ENV       cast64(5)
#define _RDPQ_COMB1_ALPHA_ADDSUB_ONE       cast64(6)
#define _RDPQ_COMB1_ALPHA_ADDSUB_1         cast64(6)
#define _RDPQ_COMB1_ALPHA_ADDSUB_ZERO      cast64(7)
#define _RDPQ_COMB1_ALPHA_ADDSUB_0         cast64(7)

#define _RDPQ_COMB2A_ALPHA_ADDSUB_TEX0      cast64(1)
#define _RDPQ_COMB2A_ALPHA_ADDSUB_TEX1      cast64(2)
#define _RDPQ_COMB2A_ALPHA_ADDSUB_PRIM      cast64(3)
#define _RDPQ_COMB2A_ALPHA_ADDSUB_SHADE     cast64(4)
#define _RDPQ_COMB2A_ALPHA_ADDSUB_ENV       cast64(5)
#define _RDPQ_COMB2A_ALPHA_ADDSUB_ONE       cast64(6)
#define _RDPQ_COMB2A_ALPHA_ADDSUB_1         cast64(6)
#define _RDPQ_COMB2A_ALPHA_ADDSUB_ZERO      cast64(7)
#define _RDPQ_COMB2A_ALPHA_ADDSUB_0         cast64(7)

#define _RDPQ_COMB2B_ALPHA_ADDSUB_COMBINED  cast64(0)
#define _RDPQ_COMB2B_ALPHA_ADDSUB_TEX1      cast64(1)   // TEX0 not available in 2nd cycle (pipelined)
#define _RDPQ_COMB2B_ALPHA_ADDSUB_PRIM      cast64(3)
#define _RDPQ_COMB2B_ALPHA_ADDSUB_SHADE     cast64(4)
#define _RDPQ_COMB2B_ALPHA_ADDSUB_ENV       cast64(5)
#define _RDPQ_COMB2B_ALPHA_ADDSUB_ONE       cast64(6)
#define _RDPQ_COMB2B_ALPHA_ADDSUB_1         cast64(6)
#define _RDPQ_COMB2B_ALPHA_ADDSUB_ZERO      cast64(7)
#define _RDPQ_COMB2B_ALPHA_ADDSUB_0         cast64(7)

#define _RDPQ_COMB1_ALPHA_MUL_LOD_FRAC         cast64(0)
#define _RDPQ_COMB1_ALPHA_MUL_TEX0             cast64(1)
#define _RDPQ_COMB1_ALPHA_MUL_PRIM             cast64(3)
#define _RDPQ_COMB1_ALPHA_MUL_SHADE            cast64(4)
#define _RDPQ_COMB1_ALPHA_MUL_ENV              cast64(5)
#define _RDPQ_COMB1_ALPHA_MUL_PRIM_LOD_FRAC    cast64(6)
#define _RDPQ_COMB1_ALPHA_MUL_ZERO             cast64(7)
#define _RDPQ_COMB1_ALPHA_MUL_0                cast64(7)

#define _RDPQ_COMB2A_ALPHA_MUL_LOD_FRAC         cast64(0)
#define _RDPQ_COMB2A_ALPHA_MUL_TEX0             cast64(1)
#define _RDPQ_COMB2A_ALPHA_MUL_TEX1             cast64(2)
#define _RDPQ_COMB2A_ALPHA_MUL_PRIM             cast64(3)
#define _RDPQ_COMB2A_ALPHA_MUL_SHADE            cast64(4)
#define _RDPQ_COMB2A_ALPHA_MUL_ENV              cast64(5)
#define _RDPQ_COMB2A_ALPHA_MUL_PRIM_LOD_FRAC    cast64(6)
#define _RDPQ_COMB2A_ALPHA_MUL_ZERO             cast64(7)
#define _RDPQ_COMB2A_ALPHA_MUL_0                cast64(7)

#define _RDPQ_COMB2B_ALPHA_MUL_LOD_FRAC         cast64(0)
#define _RDPQ_COMB2B_ALPHA_MUL_TEX1             cast64(1)  // TEX0 not available in 2nd cycle (pipelined)
#define _RDPQ_COMB2B_ALPHA_MUL_PRIM             cast64(3)
#define _RDPQ_COMB2B_ALPHA_MUL_SHADE            cast64(4)
#define _RDPQ_COMB2B_ALPHA_MUL_ENV              cast64(5)
#define _RDPQ_COMB2B_ALPHA_MUL_PRIM_LOD_FRAC    cast64(6)
#define _RDPQ_COMB2B_ALPHA_MUL_ZERO             cast64(7)
#define _RDPQ_COMB2B_ALPHA_MUL_0                cast64(7)

#define __rdpq_1cyc_comb_rgb(suba, subb, mul, add) \
    (((_RDPQ_COMB1_RGB_SUBA_ ## suba)<<52) | ((_RDPQ_COMB1_RGB_SUBB_ ## subb)<<28) | ((_RDPQ_COMB1_RGB_MUL_ ## mul)<<47) | ((_RDPQ_COMB1_RGB_ADD_ ## add)<<15) | \
     ((_RDPQ_COMB1_RGB_SUBA_ ## suba)<<37) | ((_RDPQ_COMB1_RGB_SUBB_ ## subb)<<24) | ((_RDPQ_COMB1_RGB_MUL_ ## mul)<<32) | ((_RDPQ_COMB1_RGB_ADD_ ## add)<<6))
#define __rdpq_1cyc_comb_alpha(suba, subb, mul, add) \
    (((_RDPQ_COMB1_ALPHA_ADDSUB_ ## suba)<<44) | ((_RDPQ_COMB1_ALPHA_ADDSUB_ ## subb)<<12) | ((_RDPQ_COMB1_ALPHA_MUL_ ## mul)<<41) | ((_RDPQ_COMB1_ALPHA_ADDSUB_ ## add)<<9) | \
     ((_RDPQ_COMB1_ALPHA_ADDSUB_ ## suba)<<21) | ((_RDPQ_COMB1_ALPHA_ADDSUB_ ## subb)<<3)  | ((_RDPQ_COMB1_ALPHA_MUL_ ## mul)<<18) | ((_RDPQ_COMB1_ALPHA_ADDSUB_ ## add)<<0))

#define __rdpq_2cyc_comb2a_rgb(suba, subb, mul, add) \
    (((_RDPQ_COMB2A_RGB_SUBA_ ## suba)<<52) | ((_RDPQ_COMB2A_RGB_SUBB_ ## subb)<<28) | ((_RDPQ_COMB2A_RGB_MUL_ ## mul)<<47) | ((_RDPQ_COMB2A_RGB_ADD_ ## add)<<15))
#define __rdpq_2cyc_comb2a_alpha(suba, subb, mul, add) \
    (((_RDPQ_COMB2A_ALPHA_ADDSUB_ ## suba)<<44) | ((_RDPQ_COMB2A_ALPHA_ADDSUB_ ## subb)<<12) | ((_RDPQ_COMB2A_ALPHA_MUL_ ## mul)<<41) | ((_RDPQ_COMB2A_ALPHA_ADDSUB_ ## add)<<9))
#define __rdpq_2cyc_comb2b_rgb(suba, subb, mul, add) \
    (((_RDPQ_COMB2B_RGB_SUBA_ ## suba)<<37) | ((_RDPQ_COMB2B_RGB_SUBB_ ## subb)<<24) | ((_RDPQ_COMB2B_RGB_MUL_ ## mul)<<32) | ((_RDPQ_COMB2B_RGB_ADD_ ## add)<<6))
#define __rdpq_2cyc_comb2b_alpha(suba, subb, mul, add) \
    (((_RDPQ_COMB2B_ALPHA_ADDSUB_ ## suba)<<21) | ((_RDPQ_COMB2B_ALPHA_ADDSUB_ ## subb)<<3) | ((_RDPQ_COMB2B_ALPHA_MUL_ ## mul)<<18) | ((_RDPQ_COMB2B_ALPHA_ADDSUB_ ## add)<<0))
/// @endcond

/** @brief Combiner: mask to isolate settings related to cycle 0 */
#define RDPQ_COMB0_MASK    ((cast64(0xF)<<52)|(cast64(0x1F)<<47)|(cast64(0x7)<<44)|(cast64(0x7)<<41)|(cast64(0xF)<<28)|(cast64(0x7)<<15)|(cast64(0x7)<<12)|(cast64(0x7)<<9))
/** @brief Combiner: mask to isolate settings related to cycle 1 */
#define RDPQ_COMB1_MASK    (~RDPQ_COMB0_MASK & cast64(0x00FFFFFFFFFFFFFF))

/** 
 * @brief Flag to mark the combiner as requiring two passes 
 *
 * This is an internal flag used by rdpq to mark combiner configurations that
 * require 2 passes to be executed, and differentiate them from 1 pass configurations.
 * 
 * It is used by rdpq to automatically switch to 2cycle mode when such a
 * combiner is configured.
 * 
 * Application code should not use this macro directly.
 */
#define RDPQ_COMBINER_2PASS   (cast64(1)<<63)

/**
 * @brief Build a 1-pass combiner formula
 * 
 * This macro allows to build a 1-pass color combiner formula. 
 * In general, the color combiner is able to execute the following
 * per-pixel formula:
 * 
 *      (A - B) * C + D
 * 
 * where A, B, C, D can be configured picking several possible
 * inputs called "slots". Two different formulas (with the same structure
 * but different inputs) must be configured: one for the RGB 
 * channels and for the alpha channel.
 * 
 * The macro must be invoked as:
 * 
 *      RDPQ_COMBINER1((A1, B1, C1, D1), (A2, B2, C2, D2))
 * 
 * where `A1`, `B1`, `C1`, `D1` define the formula used for RGB channels,
 * while `A2`, `B2`, `C2`, `D2` define the formula for the alpha channel. 
 * Please notice the double parenthesis.
 * 
 * For example, this macro:
 * 
 *      RDPQ_COMBINER1((TEX0, 0, SHADE, 0), (0, 0, 0, TEX0))
 * 
 * configures the formulas:
 * 
 *      RGB   = (TEX0 - 0) * SHADE + 0    = TEX0 * SHADE
 *      ALPHA = (0    - 0) * 0     + TEX0 = TEX0
 * 
 * In the RGB channels, the texel color is multiplied by the shade color
 * (which is the per-pixel interpolated vertex color), basically applying
 * gouraud shading. The alpha channel of the texel is instead passed through
 * with no modifications.
 *
 * The output of the combiner goes into the blender unit, that allows for further
 * operations on the RGB channels, especially allowing to blend it with the
 * framebuffer contents. See #RDPQ_BLENDER for information on how to configure the blender.
 * 
 * The values created by #RDPQ_COMBINER1 are of type #rdpq_combiner_t. They can be used
 * in two different ways:
 * 
 *  * When using the higher-level mode API (rdpq_mode.h), pass it to
 *    #rdpq_mode_combiner. This will take care of everything else required
 *    to make the combiner work (eg: render mode tweaks). See the
 *    documentation of #rdpq_mode_combiner for more information.
 *  * When using the lower-level API (#rdpq_set_combiner_raw),
 *    the combiner is configured into RDP, but it is up to the programmer
 *    to make sure the current render mode is compatible with it,
 *    or tweak it by calling #rdpq_set_other_modes_raw. For instance,
 *    if the render mode is in 2-cycle mode, only a 2-pass combiner
 *    should be set.
 * 
 * This is the list of all possible slots. Not all slots are
 * available for the four variables (see the table below).
 *  
 *  * `TEX0`: texel of the first texture being drawn.
 *  * `TEX1`: texel of the second texture being drawn.
 *  * `TEX0_BUG`: due to a hardware bug, when using TEX0 in the second pass,
 *    RDP will actually sample the next texel in the scanline. We call this
 *    slot `TEX0_BUG` to make it clear that there is a potential issue.
 *  * `SHADE`: per-pixel interpolated color. This can be set on each
 *    vertex of a triangle, and is interpolated across each pixel. It
 *    cannot be used while drawing rectangles.
 *  * `PRIM`: value of the PRIM register (set via #rdpq_set_prim_color)
 *  * `ENV`: value of the ENV register (set via #rdpq_set_env_color)
 *  * `NOISE`: a random value
 *  * `1`: the constant value 1.0
 *  * `0`: the constant value 0.0
 *  * `K4`: the constant value configured as `k4` as part of YUV parameters
 *    (via #rdpq_set_yuv_parms).
 *  * `K5`: the constant value configured as `k5` as part of YUV parameters
 *    (via #rdpq_set_yuv_parms).
 *  * `TEX0_ALPHA`: alpha of the text of the texture being drawn.
 *  * `SHADE_ALPHA`: alpha of the per-pixel interpolated color.
 *  * `PRIM_ALPHA`: alpha of the PRIM register (set via #rdpq_set_prim_color)
 *  * `ENV_ALPHA`: alpha of the ENV register (set via #rdpq_set_env_color)
 *  * `LOD_FRAC`: the LOD fraction, that is the fractional value that can be used
 *                as interpolation value between different mipmaps. It basically
 *                says how much the texture is being scaled down.
 *  * `PRIM_LOD_FRAC`
 *  * `KEYCENTER`
 *  * `KEYSCALE`
 * 
 * These tables show, for each possible variable of the RGB and ALPHA formula,
 * which slots are allowed:
 * 
 * <table>
 * <tr><th rowspan="4" width="60em">RGB</th>
 *     <th>A</th><td>`TEX0`, `SHADE`, `PRIM`, `ENV`, `NOISE`, `1`, `0`</td></tr>
 * <tr><th>B</th> <td>`TEX0`, `SHADE`, `PRIM`, `ENV`, `KEYCENTER`, `K4`, `0`</td></tr>
 * <tr><th>C</th> <td>`TEX0`, `SHADE`, `PRIM`, `ENV`, `TEX0_ALPHA`, `SHADE_ALPHA`, `PRIM_ALPHA`, `ENV_ALPHA`, `LOD_FRAC`, `PRIM_LOD_FRAC`, `K5`, 'KEYSCALE', `0`</td></tr>
 * <tr><th>D</th></tr><td>`TEX0`, `SHADE`, `PRIM`, `ENV`, `1`, `0`</td></tr>
 * </table>
 * 
 * <table>
 * <tr><th rowspan="4" width="60em">ALPHA</th>
 *     <th>A</th><td>`TEX0`, `SHADE`, `PRIM`, `ENV`, `1`, `0`</td></tr>
 * <tr><th>B</th><td>`TEX0`, `SHADE`, `PRIM`, `ENV`, `1`, `0`</td></tr>
 * <tr><th>C</th><td>`TEX0`, `SHADE`, `PRIM`, `ENV`, `LOD_FRAC`, `PRIM_LOD_FRAC`, `0`</td></tr>
 * <tr><th>D</th><td>`TEX0`, `SHADE`, `PRIM`, `ENV`, `1`, `0`</td></tr>
 * </table>
 * 
 * For instance, to draw a gouraud-shaded textured triangle, one might want to calculate
 * the following combiner formula:
 * 
 *        RGB   = TEX0 * SHADE
 *        ALPHA = TEX0 * SHADE
 * 
 * which means that for all channels, we multiply the value sampled from the texture
 * with the per-pixel interpolated color coming from the triangle vertex. To do so,
 * we need to adapt the formula to the 4-variable combiner structure:
 * 
 *        RGB   = (TEX0 - 0) * SHADE + 0
 *        ALPHA = (TEX0 - 0) * SHADE + 0
 * 
 * To program this into the combiner, we can issue the following command:
 * 
 *        rdpq_mode_combiner(RDPQ1_COMBINER((TEX0, 0, SHADE, 0), (TEX0, 0, SHADE, 0)));
 *
 * Notice that this is just a way to obtain the formula above. Another possibility is:
 * 
 *        rdpq_mode_combiner(RDPQ1_COMBINER((1, 0, SHADE, TEX0), (1, 0, SHADE, TEX0)));
 * 
 * which will obtain exactly the same result.
 * 
 * A complete example drawing a textured rectangle with a fixed semi-transparency of 0.7:
 * 
 * @code{.c}
 *      // Set standard mode
 *      rdpq_set_mode_standard();    
 *      
 *      // Set a combiner to sample TEX0 as-is in RGB channels, and put a fixed value
 *      // as alpha channel, coming from the ENV register.
 *      rdpq_mode_combiner(RDPQ_COMBINER1((ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, ENV)));
 * 
 *      // Set the fixed value in the ENV register. RGB components are ignored as the slot
 *      // ENV is not used in the RGB combiner formula, so we just put zero there.
 *      rdpq_set_env_color(RGBA32(0, 0, 0, 0.7*255));
 * 
 *      // Activate blending with the background
 *      rdpq_mode_blender(RDPQ_BLENDER(IN_RGB, ENV_ALPHA, MEMORY_RGB, INV_MUX_ALPHA));
 * 
 *      // Load the texture in TMEM
 *      rdpq_tex_load(TILE0, texture, 0);
 * 
 *      // Draw the rectangle
 *      rdpq_texture_rectangle(TILE0,
 *          0, 0, 100, 80,
 *          0, 0, 1.f, 1.0f);
 * @endcode
 * 
 * @param[in]  rgb      The RGB formula as `(A, B, C, D)`
 * @param[in]  alpha    The ALPHA formula as `(A, B, C, D)`
 * 
 * @see #rdpq_mode_combiner
 * @see #rdpq_set_combiner_raw
 * @see #RDPQ_COMBINER2
 * @see #RDPQ_BLENDER
 * 
 * @hideinitializer
 */
#define RDPQ_COMBINER1(rgb, alpha) \
    castcc(__rdpq_1cyc_comb_rgb rgb   | __rdpq_1cyc_comb_alpha alpha)

/**
 * @brief Build a 2-pass combiner formula
 * 
 * This is similar to #RDPQ_COMBINER1, but it creates a two-passes combiner.
 * The combiner unit in RDP in fact allows up to two sequential combiner
 * formulas that can be applied to each pixel. 
 * 
 * In the second pass, you can refer to the output of the first pass using
 * the `COMBINED` slot (not available in the first pass).
 * 
 * Refer to #RDPQ_COMBINER1 for more information.
 * 
 * @see #rdpq_mode_combiner
 * @see #rdpq_set_combiner_raw
 * @see #RDPQ_COMBINER1
 * @see #RDPQ_BLENDER
 * 
 * @hideinitializer
 */
#define RDPQ_COMBINER2(rgb0, alpha0, rgb1, alpha1) \
    castcc(__rdpq_2cyc_comb2a_rgb rgb0 | __rdpq_2cyc_comb2a_alpha alpha0 | \
           __rdpq_2cyc_comb2b_rgb rgb1 | __rdpq_2cyc_comb2b_alpha alpha1 | \
           RDPQ_COMBINER_2PASS)


/**
 * @name Standard color combiners
 * 
 * These macros offer some standard color combiner configuration that can be
 * used to implement common render modes.
 * 
 * @{
 */
/** @brief Draw a flat color.
 *  Configure the color via #rdpq_set_prim_color.
 */
#define RDPQ_COMBINER_FLAT       RDPQ_COMBINER1((0,0,0,PRIM),       (0,0,0,PRIM))
/** @brief Draw an interpolated color.
 * This can be used for solid, non-textured triangles with
 * per-vertex lighting (gouraud shading). The colors must be
 * specified on each vertex. Only triangles allow to specify
 * a per-vertex color, so you cannot draw rectangles with this.
 */
#define RDPQ_COMBINER_SHADE      RDPQ_COMBINER1((0,0,0,SHADE),      (0,0,0,SHADE))
/**
 * @brief Draw with a texture.
 * This is standard texture mapping, without any lights.
 * It can be used for rectangles (#rdpq_texture_rectangle)
 * or triangles (#rdpq_triangle).
 */
#define RDPQ_COMBINER_TEX        RDPQ_COMBINER1((0,0,0,TEX0),       (0,0,0,TEX0))
/**
 * @brief Draw with a texture modulated with a flat color.
 * Configure the color via #rdpq_set_prim_color.
 * 
 * Among other uses, this mode is the correct one to colorize a
 * #FMT_IA8 and #FMT_IA4 texture with a fixed color.
 */
#define RDPQ_COMBINER_TEX_FLAT   RDPQ_COMBINER1((TEX0,0,PRIM,0),    (TEX0,0,PRIM,0))
/**
 * @brief Draw with a texture modulated with an interpolated color.
 * This does texturing with gouraud shading, and can be used for textured triangles
 * with per-vertex lighting.
 * 
 * This mode makes sense only for triangles with per-vertex colors. It should
 * not be used with rectangles.
 */
#define RDPQ_COMBINER_TEX_SHADE  RDPQ_COMBINER1((TEX0,0,SHADE,0),   (TEX0,0,SHADE,0))
/** @} */

/** @name SET_OTHER_MODES bit macros
 * 
 * These macros can be used to assemble a raw `SET_OTHER_MODES` command to send
 * via #rdpq_set_other_modes_raw (or #rdpq_change_other_modes_raw). Assembling
 * this command manually can be complex because of the different intertwined
 * render modes that can be created. Beginners should look into the RDPQ
 * mode API before (rdpq_mode.h),
 * 
 * rdpq stores some special flag within unused bits of this register. These
 * flags are defined using the prefix `SOMX_`.
 */
///@{
#define SOMX_NUMLODS_MASK      ((cast64(7))<<59)            ///< Rdpq extension: number of LODs
#define SOMX_NUMLODS_SHIFT     59                           ///< Rdpq extension: number of LODs shift
#define SOMX_FOG               ((cast64(1))<<58)            ///< RDPQ special state: fogging is enabled

#define SOM_ATOMIC_PRIM        ((cast64(1))<<55)            ///< Atomic: serialize command execution 

#define SOM_CYCLE_1            ((cast64(0))<<52)            ///< Set cycle-type: 1cyc
#define SOM_CYCLE_2            ((cast64(1))<<52)            ///< Set cycle-type: 2cyc
#define SOM_CYCLE_COPY         ((cast64(2))<<52)            ///< Set cycle-type: copy
#define SOM_CYCLE_FILL         ((cast64(3))<<52)            ///< Set cycle-type: fill
#define SOM_CYCLE_MASK         ((cast64(3))<<52)            ///< Cycle-type mask
#define SOM_CYCLE_SHIFT        52                           ///< Cycle-type shift

#define SOM_TEXTURE_PERSP      (cast64(1)<<51)              ///< Texture: enable perspective correction
#define SOM_TEXTURE_DETAIL     (cast64(1)<<50)              ///< Texture: enable "detail"
#define SOM_TEXTURE_SHARPEN    (cast64(1)<<49)              ///< Texture: enable "sharpen"
#define SOM_TEXTURE_LOD        (cast64(1)<<48)              ///< Texture: enable LODs.
#define SOM_TEXTURE_LOD_SHIFT  48                           ///< Texture: LODs shift

#define SOM_TLUT_NONE          (cast64(0)<<46)              ///< TLUT: no palettes
#define SOM_TLUT_RGBA16        (cast64(2)<<46)              ///< TLUT: draw with palettes in format RGB16
#define SOM_TLUT_IA16          (cast64(3)<<46)              ///< TLUT: draw with palettes in format IA16
#define SOM_TLUT_MASK          (cast64(3)<<46)              ///< TLUT mask
#define SOM_TLUT_SHIFT         46                           ///< TLUT mask shift

#define SOM_SAMPLE_POINT       (cast64(0)<<44)              ///< Texture sampling: point sampling (1x1)
#define SOM_SAMPLE_BILINEAR    (cast64(2)<<44)              ///< Texture sampling: bilinear interpolation (2x2)
#define SOM_SAMPLE_MEDIAN      (cast64(3)<<44)              ///< Texture sampling: mid-texel average (2x2)
#define SOM_SAMPLE_MASK        (cast64(3)<<44)              ///< Texture sampling mask
#define SOM_SAMPLE_SHIFT       44                           ///< Texture sampling mask shift

#define SOM_TF0_RGB           (cast64(1)<<43)               ///< Texture Filter, cycle 0 (TEX0): standard fetching (for RGB)
#define SOM_TF0_YUV           (cast64(0)<<43)               ///< Texture Filter, cycle 0 (TEX0): fetch nearest and do first step of color conversion (for YUV)
#define SOM_TF1_RGB           (cast64(2)<<41)               ///< Texture Filter, cycle 1 (TEX1): standard fetching (for RGB)
#define SOM_TF1_YUV           (cast64(0)<<41)               ///< Texture Filter, cycle 1 (TEX1): fetch nearest and do first step of color conversion (for YUV)
#define SOM_TF1_YUVTEX0       (cast64(1)<<41)               ///< Texture Filter, cycle 1 (TEX1): don't fetch, and instead do color conversion on TEX0 (allows YUV with bilinear filtering)
#define SOM_TF_MASK           (cast64(7)<<41)               ///< Texture Filter mask
#define SOM_TF_SHIFT          41                            ///< Texture filter mask shift

#define SOM_RGBDITHER_SQUARE   ((cast64(0))<<38)            ///< RGB Dithering: square filter
#define SOM_RGBDITHER_BAYER    ((cast64(1))<<38)            ///< RGB Dithering: bayer filter
#define SOM_RGBDITHER_NOISE    ((cast64(2))<<38)            ///< RGB Dithering: noise
#define SOM_RGBDITHER_NONE     ((cast64(3))<<38)            ///< RGB Dithering: none
#define SOM_RGBDITHER_MASK     ((cast64(3))<<38)            ///< RGB Dithering mask
#define SOM_RGBDITHER_SHIFT    38                           ///< RGB Dithering mask shift

#define SOM_ALPHADITHER_SAME   ((cast64(0))<<36)            ///< Alpha Dithering: same as RGB
#define SOM_ALPHADITHER_INVERT ((cast64(1))<<36)            ///< Alpha Dithering: invert pattern compared to RG
#define SOM_ALPHADITHER_NOISE  ((cast64(2))<<36)            ///< Alpha Dithering: noise
#define SOM_ALPHADITHER_NONE   ((cast64(3))<<36)            ///< Alpha Dithering: none
#define SOM_ALPHADITHER_MASK   ((cast64(3))<<36)            ///< Alpha Dithering mask
#define SOM_ALPHADITHER_SHIFT  36                           ///< Alpha Dithering mask shift

#define SOMX_LOD_INTERPOLATE     ((cast64(1))<<32)          ///< RDPQ special state: mimap interpolation (aka trilinear) requested
#define SOMX_LOD_INTERPOLATE_SHQ ((cast64(1))<<33)          ///< RDPQ special state: mimap interpolation for SHC texture format
#define SOMX_LOD_INTERP_MASK     ((cast64(3))<<32)          ///< RDPQ special state: mask for LOD interpolation formulas
#define SOMX_LOD_INTERP_SHIFT    32                         ///< RDPQ special state: shift for LOD interpolation formulas
#define SOMX_AA_REDUCED          ((cast64(1))<<34)          ///< RDPQ special state: reduced antialiasing is enabled
#define SOMX_UPDATE_FREEZE       ((cast64(1))<<35)          ///< RDPQ special state: render mode update is frozen (see #rdpq_mode_begin)

#define SOM_BLEND0_MASK        (cast64(0xCCCC0000) | SOM_BLENDING | SOM_READ_ENABLE | SOMX_BLEND_2PASS)     ///< Blender: mask of settings related to pass 0
#define SOM_BLEND1_MASK        (cast64(0x33330000) | SOM_BLENDING | SOM_READ_ENABLE | SOMX_BLEND_2PASS)     ///< Blender: mask of settings related to pass 1
#define SOM_BLEND_MASK         (SOM_BLEND0_MASK | SOM_BLEND1_MASK)                                          ///< Blender: mask of all settings

#define SOMX_BLEND_2PASS       ((cast64(1))<<15)            ///< RDPQ special state: record that the blender is made of 2 passes

#define SOM_BLENDING           ((cast64(1))<<14)            ///< Activate blending for all pixels

#define SOM_BLALPHA_CC           ((cast64(0))<<12)          ///< Blender IN_ALPHA is the output of the combiner output (default)
#define SOM_BLALPHA_CVG          ((cast64(2))<<12)          ///< Blender IN_ALPHA is the coverage of the current pixel
#define SOM_BLALPHA_CVG_TIMES_CC ((cast64(3))<<12)          ///< Blender IN_ALPHA is the product of the combiner output and the coverage
#define SOM_BLALPHA_MASK         ((cast64(3))<<12)          ///< Blender alpha configuration mask
#define SOM_BLALPHA_SHIFT        12                         ///< Blender alpha configuration shift

#define SOM_ZMODE_OPAQUE           ((cast64(0))<<10)        ///< Z-mode: opaque surface
#define SOM_ZMODE_INTERPENETRATING ((cast64(1))<<10)        ///< Z-mode: interprenating surfaces
#define SOM_ZMODE_TRANSPARENT      ((cast64(2))<<10)        ///< Z-mode: transparent surface
#define SOM_ZMODE_DECAL            ((cast64(3))<<10)        ///< Z-mode: decal surface
#define SOM_ZMODE_MASK             ((cast64(3))<<10)        ///< Z-mode mask
#define SOM_ZMODE_SHIFT            10                       ///< Z-mode mask shift

#define SOM_Z_WRITE            ((cast64(1))<<5)             ///< Activate Z-buffer write
#define SOM_Z_WRITE_SHIFT      5                            ///< Z-buffer write bit shift

#define SOM_Z_COMPARE          ((cast64(1))<<4)             ///< Activate Z-buffer compare
#define SOM_Z_COMPARE_SHIFT    4                            ///< Z-buffer compare bit shift

#define SOM_ZSOURCE_PIXEL      ((cast64(0))<<2)             ///< Z-source: per-pixel Z
#define SOM_ZSOURCE_PRIM       ((cast64(1))<<2)             ///< Z-source: fixed value
#define SOM_ZSOURCE_MASK       ((cast64(1))<<2)             ///< Z-source mask
#define SOM_ZSOURCE_SHIFT      2                            ///< Z-source mask shift

#define SOM_ALPHACOMPARE_NONE       ((cast64(0))<<0)        ///< Alpha Compare: disable
#define SOM_ALPHACOMPARE_THRESHOLD  ((cast64(1))<<0)        ///< Alpha Compare: use blend alpha as threshold
#define SOM_ALPHACOMPARE_NOISE      ((cast64(3))<<0)        ///< Alpha Compare: use noise as threshold
#define SOM_ALPHACOMPARE_MASK       ((cast64(3))<<0)        ///< Alpha Compare mask
#define SOM_ALPHACOMPARE_SHIFT      0                       ///< Alpha Compare mask shift

#define SOM_READ_ENABLE                 ((cast64(1)) << 6)  ///< Enable reads from framebuffer
#define SOM_AA_ENABLE                   ((cast64(1)) << 3)  ///< Enable anti-alias

#define SOM_COVERAGE_DEST_CLAMP         ((cast64(0)) << 8)  ///< Coverage: add and clamp to 7 (full)
#define SOM_COVERAGE_DEST_WRAP          ((cast64(1)) << 8)  ///< Coverage: add and wrap from 0
#define SOM_COVERAGE_DEST_ZAP           ((cast64(2)) << 8)  ///< Coverage: force 7 (full)
#define SOM_COVERAGE_DEST_SAVE          ((cast64(3)) << 8)  ///< Coverage: save (don't write)
#define SOM_COVERAGE_DEST_MASK          ((cast64(3)) << 8)  ///< Coverage mask
#define SOM_COVERAGE_DEST_SHIFT         8                   ///< Coverage mask shift

#define SOM_COLOR_ON_CVG_OVERFLOW       ((cast64(1)) << 7)  ///< Update color buffer only on coverage overflow
///@}

///@cond
// Helpers macros for RDPQ_BLENDER
#define _RDPQ_SOM_BLEND1_A_IN_RGB         cast64(0)
#define _RDPQ_SOM_BLEND1_A_MEMORY_RGB     cast64(1)
#define _RDPQ_SOM_BLEND1_A_BLEND_RGB      cast64(2)
#define _RDPQ_SOM_BLEND1_A_FOG_RGB        cast64(3)

#define _RDPQ_SOM_BLEND1_B1_IN_ALPHA       cast64(0)
#define _RDPQ_SOM_BLEND1_B1_FOG_ALPHA      cast64(1)
#define _RDPQ_SOM_BLEND1_B1_SHADE_ALPHA    cast64(2)
#define _RDPQ_SOM_BLEND1_B1_ZERO           cast64(3)
#define _RDPQ_SOM_BLEND1_B1_0              cast64(3)

#define _RDPQ_SOM_BLEND1_B2_INV_MUX_ALPHA  cast64(0)
#define _RDPQ_SOM_BLEND1_B2_MEMORY_CVG     cast64(1)
#define _RDPQ_SOM_BLEND1_B2_ONE            cast64(2)
#define _RDPQ_SOM_BLEND1_B2_1              cast64(2)
#define _RDPQ_SOM_BLEND1_B2_ZERO           cast64(3)
#define _RDPQ_SOM_BLEND1_B2_0              cast64(3)

#define _RDPQ_SOM_BLEND2A_A_IN_RGB        cast64(0)
#define _RDPQ_SOM_BLEND2A_A_BLEND_RGB     cast64(2)
#define _RDPQ_SOM_BLEND2A_A_FOG_RGB       cast64(3)

#define _RDPQ_SOM_BLEND2A_B1_IN_ALPHA       cast64(0)
#define _RDPQ_SOM_BLEND2A_B1_FOG_ALPHA      cast64(1)
#define _RDPQ_SOM_BLEND2A_B1_SHADE_ALPHA    cast64(2)
#define _RDPQ_SOM_BLEND2A_B1_ZERO           cast64(3)
#define _RDPQ_SOM_BLEND2A_B1_0              cast64(3)

#define _RDPQ_SOM_BLEND2A_B2_INV_MUX_ALPHA  cast64(0)    // only valid option is "1-b1" in the first pass

#define _RDPQ_SOM_BLEND2B_A_CYCLE1_RGB    cast64(0)
#define _RDPQ_SOM_BLEND2B_A_MEMORY_RGB    cast64(1)
#define _RDPQ_SOM_BLEND2B_A_BLEND_RGB     cast64(2)
#define _RDPQ_SOM_BLEND2B_A_FOG_RGB       cast64(3)

#define _RDPQ_SOM_BLEND2B_B1_IN_ALPHA       cast64(0)
#define _RDPQ_SOM_BLEND2B_B1_FOG_ALPHA      cast64(1)
#define _RDPQ_SOM_BLEND2B_B1_SHADE_ALPHA    cast64(2)
#define _RDPQ_SOM_BLEND2B_B1_ZERO           cast64(3)
#define _RDPQ_SOM_BLEND2B_B1_0              cast64(3)

#define _RDPQ_SOM_BLEND2B_B2_INV_MUX_ALPHA  cast64(0)
#define _RDPQ_SOM_BLEND2B_B2_MEMORY_CVG     cast64(1)
#define _RDPQ_SOM_BLEND2B_B2_ONE            cast64(2)
#define _RDPQ_SOM_BLEND2B_B2_1              cast64(2)
#define _RDPQ_SOM_BLEND2B_B2_ZERO           cast64(3)
#define _RDPQ_SOM_BLEND2B_B2_0              cast64(3)

#define _RDPQ_SOM_BLEND_EXTRA_A_IN_RGB          cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_A_CYCLE1_RGB      cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_A_MEMORY_RGB      (SOM_READ_ENABLE)
#define _RDPQ_SOM_BLEND_EXTRA_A_BLEND_RGB       cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_A_FOG_RGB         cast64(0)

#define _RDPQ_SOM_BLEND_EXTRA_B1_IN_ALPHA       cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_B1_FOG_ALPHA      cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_B1_SHADE_ALPHA    cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_B1_ZERO           cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_B1_0              cast64(0)

#define _RDPQ_SOM_BLEND_EXTRA_B2_INV_MUX_ALPHA  cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_B2_MEMORY_CVG     (SOM_READ_ENABLE)
#define _RDPQ_SOM_BLEND_EXTRA_B2_ONE            cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_B2_1              cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_B2_ZERO           cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_B2_0              cast64(0)

#define __rdpq_blend(cyc, a1, b1, a2, b2, sa1, sb1, sa2, sb2) (\
        ((_RDPQ_SOM_BLEND ## cyc ## _A_  ## a1) << sa1) | \
        ((_RDPQ_SOM_BLEND ## cyc ## _B1_ ## b1) << sb1) | \
        ((_RDPQ_SOM_BLEND ## cyc ## _A_  ## a2) << sa2) | \
        ((_RDPQ_SOM_BLEND ## cyc ## _B2_ ## b2) << sb2) |  \
         (_RDPQ_SOM_BLEND_EXTRA_A_  ## a1) | \
         (_RDPQ_SOM_BLEND_EXTRA_B1_ ## b1) | \
         (_RDPQ_SOM_BLEND_EXTRA_A_  ## a2) | \
         (_RDPQ_SOM_BLEND_EXTRA_B2_ ## b2) \
)

#define __rdpq_blend_1cyc_0(a1, b1, a2, b2) __rdpq_blend(1,  a1, b1, a2, b2, 30, 26, 22, 18)
#define __rdpq_blend_1cyc_1(a1, b1, a2, b2) __rdpq_blend(1,  a1, b1, a2, b2, 28, 24, 20, 16)
#define __rdpq_blend_2cyc_0(a1, b1, a2, b2) __rdpq_blend(2A, a1, b1, a2, b2, 30, 26, 22, 18)
#define __rdpq_blend_2cyc_1(a1, b1, a2, b2) __rdpq_blend(2B, a1, b1, a2, b2, 28, 24, 20, 16)
///@endcond

/**
 * @brief Build a 1-pass blender formula
 * 
 * This macro allows to build a 1-pass blender formula. 
 * In general, the blender is able to execute the following
 * per-pixel formula:
 * 
 *      (P * A) + (Q * B)
 * 
 * where P and Q are usually pixel inputs, while A and B are
 * blending factors. `P`, `Q`, `A`, `B` can be configured picking
 * several possible inputs called "slots".
 * 
 * The macro must be invoked as:
 * 
 *      RDPQ_BLENDER((P, A, Q, B))
 * 
 * where `P`, `A`, `Q`, `B` can be any of the values described below.
 * Please notice the double parenthesis.
 * 
 * For example, this macro:
 * 
 *      RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, 1))
 * 
 * configures the formula:
 * 
 *      (IN_RGB * IN_ALPHA) + (MEMORY_RGB * 1.0)
 * 
 * The value created is of type #rdpq_blender_t. They can be used
 * in two different ways:
 * 
 *  * When using the higher-level mode API (rdpq_mode.h), the blender
 *    formula can be passed to either #rdpq_mode_fog or #rdpq_mode_blender.
 *    The blender unit is in fact capable of running up two passes
 *    in sequence, so each function configures one different pass.
 *  * When using the lower-level API (#rdpq_set_other_modes_raw),
 *    the value created by #RDPQ_BLENDER can be directly combined
 *    with other `SOM_*` macros to create the final value to
 *    pass to the function. If a two-pass blender must be configured,
 *    use #RDPQ_BLENDER2 instead.
 * 
 * Pre-made formulas for common scenarios are available: see
 * #RDPQ_BLENDER_MULTIPLY, #RDPQ_BLENDER_ADDITIVE, #RDPQ_FOG_STANDARD.
 * 
 * These are all possible inputs for `P` and `Q`:
 * 
 *  * `IN_RGB`: The RGB channels of the pixel being drawn. This is
 *    actually the output of the color combiner (that can be
 *    configured via #rdpq_mode_combiner, #RDPQ_COMBINER1,
 *    and #RDPQ_COMBINER2).
 *  * `MEMORY_RGB`: Current contents of the framebuffer, where the
 *    current pixel will be drawn. Reading the framebuffer contents
 *    and using them in the formula allows to create the typical
 *    blending effect.
 *  * `BLEND_RGB`: A fixed RGB value programmed into the BLEND register.
 *    This can be configured via #rdpq_set_blend_color.
 *  * `FOG_RGB`: A fixed RGB value programmed into the FOG register.
 *    This can be configured via #rdpq_set_fog_color.
 * 
 * These are all possible inputs for `A`:
 * 
 *  * `IN_ALPHA`: The alpha channel of the pixel being drawn. This is
 *    actually the output of the color combiner (that can be
 *    configured via #rdpq_mode_combiner, #RDPQ_COMBINER1,
 *    and #RDPQ_COMBINER2).
 *  * `FOG_ALPHA`: The alpha channel of the FOG register.
 *    This can be configured via #rdpq_set_fog_color.
 *  * `SHADE_ALPHA`: The alpha channel of the shade color.
 *    The shade component is the color optionally set on
 *    each vertex when drawing a triangle (see #rdpq_triangle).
 *    The RDP interpolates it on each pixel.
 *  * `0`: the constant value 0.
 * 
 * These are all possible inputs for `B`:
 * 
 *  * `INV_MUX_ALPHA`: This value is the inverse of whatever input
 *    was selected for `A`. For instance, if `A` was configured
 *    as `FOG_ALPHA`, setting `B` to `INV_MUX_ALPHA` means using
 *    `1.0 - FOG_ALPHA` in the calculation. This basically allows
 *    to do a linear interpolation between `P` and `Q` where
 *    `A` is the interpolation factor.
 *  * `MEMORY_CVG`: This is the subpixel coverage value stored in
 *    the framebuffer at the position where the current pixel will
 *    be drawn. The coverage is normally stored as a value in the
 *    range 0-7, but the blender normalizes in the range 0.0-1.0.
 *  * `1`: the constant value 1.
 *  * `0`: the constant value 0.
 * 
 * The blender uses the framebuffer precision for the RGB channels:
 * when drawing to a 32-bit framebuffer, `P` and `Q` will have
 * 8-bit precision per channel, whilst when drawing to a 16-bit
 * framebuffer, `P` and `Q` will be 5-bit. You can add 
 * dithering if needed, via #rdpq_mode_dithering.
 * 
 * On the other hand, `A` and `B` always have a reduced 5-bit
 * precision, even on 32-bit framebuffers. This means that the
 * alpha values will be quantized during the blending, possibly
 * creating mach banding. Consider using dithering via
 * #rdpq_mode_dithering to improve the quality of the picture.
 * 
 * Notice that the blender formula only works on RGB channels. Alpha
 * channels can be used as input (as multiplicative factor), but the
 * blender does not produce an alpha channel as output. In fact,
 * the RGB output will be written to the framebuffer after the blender,
 * while the bits normally used for alpha in each framebuffer pixel
 * will contain information about subpixel coverage (that will
 * be then used by VI for doing antialiasing as a post-process filter
 * -- see #rdpq_mode_antialias for a brief explanation).
 * 
 * @see #rdpq_mode_blender
 * @see #rdpq_mode_fog
 * @see #rdpq_mode_dithering
 * @see #rdpq_set_fog_color
 * @see #rdpq_set_blend_color
 * @see #rdpq_set_other_modes_raw
 * 
 * @hideinitializer
 */
#define RDPQ_BLENDER(bl)        castbl(__rdpq_blend_1cyc_0 bl  | __rdpq_blend_1cyc_1 bl)

/**
 * @brief Build a 2-pass blender formula
 *
 * This macro is similar to #RDPQ_BLENDER, but it can be used to build a
 * two-passes blender formula. This formula can be then configured using the
 * mode API via #rdpq_mode_blender, or using the lower-level API via
 * #rdpq_change_other_modes_raw.
 * 
 * Refer to #RDPQ_BLENDER for information on how to build a blender formula.
 * 
 * In two-passes mode, there are a few differences and gotchas in the way the formula
 * must be constructed:
 * 
 * * In the first pass, `B` must be `INV_MUX_ALPHA` (any other value is invalid
 *   and will result in a compile-time error).
 * * In the first pass, `MEMORY_RGB` is not available.
 * * In the second pass, `IN_RGB` is not available, but you can
 *   instead use `CYCLE1_RGB` to refer to the output of the first cycle.
 *   `IN_ALPHA` is still available (as the blender does not produce a alpha
 *   output, so the input alpha is available also in the second pass).
 * * In the second pass, because of a hardware bug, `SHADE_ALPHA` will actually
 *   refer to the alpha color of the *next* pixel in the scanline (the pixel
 *   to the right). On the last pixel of the triangle in each scanline, the
 *   value read as `SHADE_ALPHA` is mostly undefined. Given this hardware bug,
 *   avoid using `SHADE_ALPHA` in the second pass if possible.
 * 
 * @see #RDPQ_BLENDER
 * @see #rdpq_mode_blender
 * @see #rdpq_set_other_modes_raw
 * 
 * @hideinitializer
 */
#define RDPQ_BLENDER2(bl0, bl1) castbl(__rdpq_blend_2cyc_0 bl0 | __rdpq_blend_2cyc_1 bl1 | SOMX_BLEND_2PASS)

/** 
 * @brief The maximum Z value, which is the default reset value for the Z-Buffer.
 * 
 * This is equivalent to #ZBUF_VAL(1.0f)
 */
#define ZBUF_MAX        0xFFFC 

#ifndef __ASSEMBLER__

///@cond
extern uint16_t __rdpq_zfp14(float f);
///@endcond

/** 
 * @brief Create a packed Z-buffer value for a given Z value.
 * 
 * This macro can be used to convert a floating point Z value in range [0..1]
 * to a packed Z value that can be written as-is in the Z-buffer, for instance
 * via #rdpq_clear_z.
 * 
 * Notice that this macro sets Delta-Z to 0 in the packed Z value, since it is
 * not possible to fully configure Delta-Z via #rdpq_clear_z anyway.
 */
#define ZBUF_VAL(f)     (__rdpq_zfp14(f) << 2)

#endif

#endif
