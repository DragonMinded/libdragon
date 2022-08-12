/**
 * @file rdp_commands.h
 * @brief RDP command macros
 * @ingroup rdp
 * 
 * This file contains macros that can be used to assembly some complex RDP commands:
 * the blender and the color combiner configurations.
 * 
 * The file is meant to be included also from RSP assembly code, for readability
 * while manipulating these commands.
 */
#ifndef LIBDRAGON_RDP_COMMANDS_H
#define LIBDRAGON_RDP_COMMANDS_H

///@cond
#ifndef __ASSEMBLER__
#include <stdint.h>
#define cast64(x) (uint64_t)(x)
#else
#define cast64(x) x
#endif
///@endcond

#define RDP_TILE_FORMAT_RGBA  0    ///< RDP internal format: RGBA (see #tex_format_t)
#define RDP_TILE_FORMAT_YUV   1    ///< RDP internal format: YUV (see #tex_format_t)
#define RDP_TILE_FORMAT_INDEX 2    ///< RDP internal format: INDEX (see #tex_format_t)
#define RDP_TILE_FORMAT_IA    3    ///< RDP internal format: IA (see #tex_format_t)
#define RDP_TILE_FORMAT_I     4    ///< RDP internal format: I (see #tex_format_t)

#define RDP_TILE_SIZE_4BIT  0      ///< RDP internal format size: 4-bit (see #tex_format_t)
#define RDP_TILE_SIZE_8BIT  1      ///< RDP internal format size: 8-bit (see #tex_format_t)
#define RDP_TILE_SIZE_16BIT 2      ///< RDP internal format size: 16-bit (see #tex_format_t)
#define RDP_TILE_SIZE_32BIT 3      ///< RDP internal format size: 32-bit (see #tex_format_t)

/// @cond
// Intenral helpers to build a color combiner setting
#define _RDPQ_COMB1_RGB_SUBA_TEX0      cast64(1)
#define _RDPQ_COMB1_RGB_SUBA_PRIM      cast64(3)
#define _RDPQ_COMB1_RGB_SUBA_SHADE     cast64(4)
#define _RDPQ_COMB1_RGB_SUBA_ENV       cast64(5)
#define _RDPQ_COMB1_RGB_SUBA_ONE       cast64(6)
#define _RDPQ_COMB1_RGB_SUBA_NOISE     cast64(7)
#define _RDPQ_COMB1_RGB_SUBA_ZERO      cast64(8)

#define _RDPQ_COMB2A_RGB_SUBA_TEX0      cast64(1)
#define _RDPQ_COMB2A_RGB_SUBA_TEX1      cast64(2)
#define _RDPQ_COMB2A_RGB_SUBA_PRIM      cast64(3)
#define _RDPQ_COMB2A_RGB_SUBA_SHADE     cast64(4)
#define _RDPQ_COMB2A_RGB_SUBA_ENV       cast64(5)
#define _RDPQ_COMB2A_RGB_SUBA_ONE       cast64(6)
#define _RDPQ_COMB2A_RGB_SUBA_NOISE     cast64(7)
#define _RDPQ_COMB2A_RGB_SUBA_ZERO      cast64(8)

#define _RDPQ_COMB2B_RGB_SUBA_COMBINED  cast64(0)
#define _RDPQ_COMB2B_RGB_SUBA_TEX1      cast64(1)  // TEX0 not available in 2nd cycle (pipelined)
#define _RDPQ_COMB2B_RGB_SUBA_PRIM      cast64(3)
#define _RDPQ_COMB2B_RGB_SUBA_SHADE     cast64(4)
#define _RDPQ_COMB2B_RGB_SUBA_ENV       cast64(5)
#define _RDPQ_COMB2B_RGB_SUBA_ONE       cast64(6)
#define _RDPQ_COMB2B_RGB_SUBA_NOISE     cast64(7)
#define _RDPQ_COMB2B_RGB_SUBA_ZERO      cast64(8)

#define _RDPQ_COMB1_RGB_SUBB_TEX0      cast64(1)
#define _RDPQ_COMB1_RGB_SUBB_PRIM      cast64(3)
#define _RDPQ_COMB1_RGB_SUBB_SHADE     cast64(4)
#define _RDPQ_COMB1_RGB_SUBB_ENV       cast64(5)
#define _RDPQ_COMB1_RGB_SUBB_KEYCENTER cast64(6)
#define _RDPQ_COMB1_RGB_SUBB_K4        cast64(7)
#define _RDPQ_COMB1_RGB_SUBB_ZERO      cast64(8)

#define _RDPQ_COMB2A_RGB_SUBB_TEX0      cast64(1)
#define _RDPQ_COMB2A_RGB_SUBB_TEX1      cast64(2)
#define _RDPQ_COMB2A_RGB_SUBB_PRIM      cast64(3)
#define _RDPQ_COMB2A_RGB_SUBB_SHADE     cast64(4)
#define _RDPQ_COMB2A_RGB_SUBB_ENV       cast64(5)
#define _RDPQ_COMB2A_RGB_SUBB_KEYCENTER cast64(6)
#define _RDPQ_COMB2A_RGB_SUBB_K4        cast64(7)
#define _RDPQ_COMB2A_RGB_SUBB_ZERO      cast64(8)

#define _RDPQ_COMB2B_RGB_SUBB_COMBINED  cast64(0)
#define _RDPQ_COMB2B_RGB_SUBB_TEX1      cast64(1)  // TEX0 not available in 2nd cycle (pipelined)
#define _RDPQ_COMB2B_RGB_SUBB_PRIM      cast64(3)
#define _RDPQ_COMB2B_RGB_SUBB_SHADE     cast64(4)
#define _RDPQ_COMB2B_RGB_SUBB_ENV       cast64(5)
#define _RDPQ_COMB2B_RGB_SUBB_KEYCENTER cast64(6)
#define _RDPQ_COMB2B_RGB_SUBB_K4        cast64(7)
#define _RDPQ_COMB2B_RGB_SUBB_ZERO      cast64(8)

#define _RDPQ_COMB1_RGB_MUL_TEX0           cast64(1)
#define _RDPQ_COMB1_RGB_MUL_PRIM           cast64(3)
#define _RDPQ_COMB1_RGB_MUL_SHADE          cast64(4)
#define _RDPQ_COMB1_RGB_MUL_ENV            cast64(5)
#define _RDPQ_COMB1_RGB_MUL_KEYSCALE       cast64(6)
#define _RDPQ_COMB1_RGB_MUL_COMBINED_ALPHA cast64(7)
#define _RDPQ_COMB1_RGB_MUL_TEX0_ALPHA     cast64(8)
#define _RDPQ_COMB1_RGB_MUL_TEX1_ALPHA     cast64(9)
#define _RDPQ_COMB1_RGB_MUL_PRIM_ALPHA     cast64(10)
#define _RDPQ_COMB1_RGB_MUL_SHADE_ALPHA    cast64(11)
#define _RDPQ_COMB1_RGB_MUL_ENV_ALPHA      cast64(12)
#define _RDPQ_COMB1_RGB_MUL_LOD_FRAC       cast64(13)
#define _RDPQ_COMB1_RGB_MUL_PRIM_LOD_FRAC  cast64(14)
#define _RDPQ_COMB1_RGB_MUL_K5             cast64(15)
#define _RDPQ_COMB1_RGB_MUL_ZERO           cast64(16)

#define _RDPQ_COMB2A_RGB_MUL_TEX0           cast64(1)
#define _RDPQ_COMB2A_RGB_MUL_TEX1           cast64(2)
#define _RDPQ_COMB2A_RGB_MUL_PRIM           cast64(3)
#define _RDPQ_COMB2A_RGB_MUL_SHADE          cast64(4)
#define _RDPQ_COMB2A_RGB_MUL_ENV            cast64(5)
#define _RDPQ_COMB2A_RGB_MUL_KEYSCALE       cast64(6)
#define _RDPQ_COMB2A_RGB_MUL_COMBINED_ALPHA cast64(7)
#define _RDPQ_COMB2A_RGB_MUL_TEX0_ALPHA     cast64(8)
#define _RDPQ_COMB2A_RGB_MUL_TEX1_ALPHA     cast64(9)
#define _RDPQ_COMB2A_RGB_MUL_PRIM_ALPHA     cast64(10)
#define _RDPQ_COMB2A_RGB_MUL_SHADE_ALPHA    cast64(11)
#define _RDPQ_COMB2A_RGB_MUL_ENV_ALPHA      cast64(12)
#define _RDPQ_COMB2A_RGB_MUL_LOD_FRAC       cast64(13)
#define _RDPQ_COMB2A_RGB_MUL_PRIM_LOD_FRAC  cast64(14)
#define _RDPQ_COMB2A_RGB_MUL_K5             cast64(15)
#define _RDPQ_COMB2A_RGB_MUL_ZERO           cast64(16)

#define _RDPQ_COMB2B_RGB_MUL_COMBINED       cast64(0)
#define _RDPQ_COMB2B_RGB_MUL_TEX1           cast64(1)  // TEX0 not available in 2nd cycle (pipelined)
#define _RDPQ_COMB2B_RGB_MUL_PRIM           cast64(3)
#define _RDPQ_COMB2B_RGB_MUL_SHADE          cast64(4)
#define _RDPQ_COMB2B_RGB_MUL_ENV            cast64(5)
#define _RDPQ_COMB2B_RGB_MUL_KEYSCALE       cast64(6)
#define _RDPQ_COMB2B_RGB_MUL_COMBINED_ALPHA cast64(7)
#define _RDPQ_COMB2B_RGB_MUL_TEX0_ALPHA     cast64(8)
#define _RDPQ_COMB2B_RGB_MUL_TEX1_ALPHA     cast64(9)
#define _RDPQ_COMB2B_RGB_MUL_PRIM_ALPHA     cast64(10)
#define _RDPQ_COMB2B_RGB_MUL_SHADE_ALPHA    cast64(11)
#define _RDPQ_COMB2B_RGB_MUL_ENV_ALPHA      cast64(12)
#define _RDPQ_COMB2B_RGB_MUL_LOD_FRAC       cast64(13)
#define _RDPQ_COMB2B_RGB_MUL_PRIM_LOD_FRAC  cast64(14)
#define _RDPQ_COMB2B_RGB_MUL_K5             cast64(15)
#define _RDPQ_COMB2B_RGB_MUL_ZERO           cast64(16)

#define _RDPQ_COMB1_RGB_ADD_TEX0      cast64(1)
#define _RDPQ_COMB1_RGB_ADD_PRIM      cast64(3)
#define _RDPQ_COMB1_RGB_ADD_SHADE     cast64(4)
#define _RDPQ_COMB1_RGB_ADD_ENV       cast64(5)
#define _RDPQ_COMB1_RGB_ADD_ONE       cast64(6)
#define _RDPQ_COMB1_RGB_ADD_ZERO      cast64(7)

#define _RDPQ_COMB2A_RGB_ADD_TEX0      cast64(1)
#define _RDPQ_COMB2A_RGB_ADD_TEX1      cast64(2)
#define _RDPQ_COMB2A_RGB_ADD_PRIM      cast64(3)
#define _RDPQ_COMB2A_RGB_ADD_SHADE     cast64(4)
#define _RDPQ_COMB2A_RGB_ADD_ENV       cast64(5)
#define _RDPQ_COMB2A_RGB_ADD_ONE       cast64(6)
#define _RDPQ_COMB2A_RGB_ADD_ZERO      cast64(7)

#define _RDPQ_COMB2B_RGB_ADD_COMBINED  cast64(0)
#define _RDPQ_COMB2B_RGB_ADD_TEX1      cast64(1)  // TEX0 not available in 2nd cycle (pipelined)
#define _RDPQ_COMB2B_RGB_ADD_PRIM      cast64(3)
#define _RDPQ_COMB2B_RGB_ADD_SHADE     cast64(4)
#define _RDPQ_COMB2B_RGB_ADD_ENV       cast64(5)
#define _RDPQ_COMB2B_RGB_ADD_ONE       cast64(6)
#define _RDPQ_COMB2B_RGB_ADD_ZERO      cast64(7)

#define _RDPQ_COMB1_ALPHA_ADDSUB_TEX0      cast64(1)
#define _RDPQ_COMB1_ALPHA_ADDSUB_PRIM      cast64(3)
#define _RDPQ_COMB1_ALPHA_ADDSUB_SHADE     cast64(4)
#define _RDPQ_COMB1_ALPHA_ADDSUB_ENV       cast64(5)
#define _RDPQ_COMB1_ALPHA_ADDSUB_ONE       cast64(6)
#define _RDPQ_COMB1_ALPHA_ADDSUB_ZERO      cast64(7)

#define _RDPQ_COMB2A_ALPHA_ADDSUB_TEX0      cast64(1)
#define _RDPQ_COMB2A_ALPHA_ADDSUB_TEX1      cast64(2)
#define _RDPQ_COMB2A_ALPHA_ADDSUB_PRIM      cast64(3)
#define _RDPQ_COMB2A_ALPHA_ADDSUB_SHADE     cast64(4)
#define _RDPQ_COMB2A_ALPHA_ADDSUB_ENV       cast64(5)
#define _RDPQ_COMB2A_ALPHA_ADDSUB_ONE       cast64(6)
#define _RDPQ_COMB2A_ALPHA_ADDSUB_ZERO      cast64(7)

#define _RDPQ_COMB2B_ALPHA_ADDSUB_COMBINED  cast64(0)
#define _RDPQ_COMB2B_ALPHA_ADDSUB_TEX1      cast64(1)   // TEX0 not available in 2nd cycle (pipelined)
#define _RDPQ_COMB2B_ALPHA_ADDSUB_PRIM      cast64(3)
#define _RDPQ_COMB2B_ALPHA_ADDSUB_SHADE     cast64(4)
#define _RDPQ_COMB2B_ALPHA_ADDSUB_ENV       cast64(5)
#define _RDPQ_COMB2B_ALPHA_ADDSUB_ONE       cast64(6)
#define _RDPQ_COMB2B_ALPHA_ADDSUB_ZERO      cast64(7)

#define _RDPQ_COMB1_ALPHA_MUL_LOD_FRAC         cast64(0)
#define _RDPQ_COMB1_ALPHA_MUL_TEX0             cast64(1)
#define _RDPQ_COMB1_ALPHA_MUL_PRIM             cast64(3)
#define _RDPQ_COMB1_ALPHA_MUL_SHADE            cast64(4)
#define _RDPQ_COMB1_ALPHA_MUL_ENV              cast64(5)
#define _RDPQ_COMB1_ALPHA_MUL_PRIM_LOD_FRAC    cast64(6)
#define _RDPQ_COMB1_ALPHA_MUL_ZERO             cast64(7)

#define _RDPQ_COMB2A_ALPHA_MUL_LOD_FRAC         cast64(0)
#define _RDPQ_COMB2A_ALPHA_MUL_TEX0             cast64(1)
#define _RDPQ_COMB2A_ALPHA_MUL_TEX1             cast64(2)
#define _RDPQ_COMB2A_ALPHA_MUL_PRIM             cast64(3)
#define _RDPQ_COMB2A_ALPHA_MUL_SHADE            cast64(4)
#define _RDPQ_COMB2A_ALPHA_MUL_ENV              cast64(5)
#define _RDPQ_COMB2A_ALPHA_MUL_PRIM_LOD_FRAC    cast64(6)
#define _RDPQ_COMB2A_ALPHA_MUL_ZERO             cast64(7)

#define _RDPQ_COMB2B_ALPHA_MUL_LOD_FRAC         cast64(0)
#define _RDPQ_COMB2B_ALPHA_MUL_TEX1             cast64(1)  // TEX0 not available in 2nd cycle (pipelined)
#define _RDPQ_COMB2B_ALPHA_MUL_PRIM             cast64(3)
#define _RDPQ_COMB2B_ALPHA_MUL_SHADE            cast64(4)
#define _RDPQ_COMB2B_ALPHA_MUL_ENV              cast64(5)
#define _RDPQ_COMB2B_ALPHA_MUL_PRIM_LOD_FRAC    cast64(6)
#define _RDPQ_COMB2B_ALPHA_MUL_ZERO             cast64(7)

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
 * where A, B, C, D can be configured picking several possibile
 * inputs called "slots". Two different formulas (with the same structure
 * but different inputs) must be configured: one for the RGB 
 * channels and for the alpha channel.
 * 
 * This is the list of all possibile slots. Not all slots are
 * available for the four variables (see the table below).
 *  
 *  * `TEX0`: texel of the texture being drawn.
 *  * `SHADE`: per-pixel interpolated color. This can be set on each
 *    vertex of a triangle, and is interpolated across each pixel. It
 *    cannot be used while drawing rectangles.
 *  * `PRIM`: value of the PRIM register (set via #rdp_set_prim_color)
 *  * `ENV`: value of the ENV register (set via #rdp_set_env_color)
 *  * `NOISE`: a random value
 *  * `ONE`: the constant value 1.0
 *  * `ZERO`: the constant value 0.0
 *  * `K4`: the constant value configured as `k4` as part of YUV parameters
 *    (via #rdpq_set_yuv_parms).
 *  * `K5`: the constant value configured as `k5` as part of YUV parameters
 *    (via #rdpq_set_yuv_parms).
 *  * `TEX0_ALPHA`: alpha of the text of the texture being drawn.
 *  * `SHADE_ALPHA`: alpha of the per-pixel interpolated color.
 *  * `PRIM_ALPHA`: alpha of the PRIM register (set via #rdp_set_prim_color)
 *  * `ENV_ALPHA`: alpha o fthe ENV register (set via #rdp_set_env_color)
 *  * `LOD_FRAC`
 *  * `PRIM_LOD_FRAC`
 *  * `KEYSCALE`
 * 
 * These tables show, for each possible variable of the RGB and ALPHA formula,
 * which slots are allowed:
 * 
 * <table>
 * <tr><th rowspan="4" width="60em">RGB</th>
 *     <th>A</th><td>`TEX0`, `SHADE`, `PRIM`, `ENV`, `NOISE`, `ONE`, `ZERO`</td></tr>
 * <tr><th>B</th> <td>`TEX0`, `SHADE`, `PRIM`, `ENV`, `KEYCENTER`, `K4`, `ZERO`</td></tr>
 * <tr><th>C</th> <td>`TEX0`, `SHADE`, `PRIM`, `ENV`, `TEX0_ALPHA`, `SHADE_ALPHA`, `PRIM_ALPHA`, `ENV_ALPHA`, `LOD_FRAC`, `PRIM_LOD_FRAC`, `K5`, `ZERO`</td></tr>
 * <tr><th>D</th></tr><td>`TEX0`, `SHADE`, `PRIM`, `ENV`, `ONE`, `ZERO`</td></tr>
 * </table>
 * 
 * <table>
 * <tr><th rowspan="4" width="60em">ALPHA</th>
 *     <th>A</th><td>`TEX0`, `SHADE`, `PRIM`, `ENV`, `ONE`, `ZERO`</td></tr>
 * <tr><th>B</th><td>`TEX0`, `SHADE`, `PRIM`, `ENV`, `ONE`, `ZERO`</td></tr>
 * <tr><th>C</th><td>`TEX0`, `SHADE`, `PRIM`, `ENV`, `LOD_FRAC`, `PRIM_LOD_FRAC`, `ZERO`</td></tr>
 * <tr><th>D</th><td>`TEX0`, `SHADE`, `PRIM`, `ENV`, `ONE`, `ZERO`</td></tr>
 * </table>
 * 
 * For instance, to draw a goraud-shaded textured triangle, one might want to calculate
 * the following combiner formula:
 * 
 *        RGB   = TEX0 * SHADE
 *        ALPHA = TEX0 * SHADE
 * 
 * which means that for all channels, we multiply the value sampled from the texture
 * with the per-pixel interpolated color coming from the triangle vertex. To do so,
 * we need to adapt the formula to the 4-variable combiner structure:
 * 
 *        RGB   = (TEX0 - ZERO) * SHADE + ZERO
 *        ALPHA = (TEX0 - ZERO) * SHADE + ZERO
 * 
 * To program this into the combiner, we can issue the following command:
 * 
 *        rdpq_mode_combiner(RDPQ1_COMBINER((TEX0, ZERO, SHADE, ZERO), (TEX0, ZERO, SHADE, ZERO)));
 *
 * Notice that this is just a way to obtain the formula above. Another possibility is:
 * 
 *        rdpq_mode_combiner(RDPQ1_COMBINER((ONE, ZERO, SHADE, TEX0), (ONE, ZERO, SHADE, TEX0)));
 * 
 * which will obtain exactly the same result.
 * 
 * Please note the use of the double parantheses within the `RDP1_COMBINER` call. These are required
 * for the macro to work correctly.
 * 
 * The output of the combiner goes into the blender unit. See #RDPQ_BLENDER1 for information on
 * how to configure the blender.
 * 
 * A complete example drawing a textured rectangle with a fixed semitransparency of 0.7:
 * 
 * @code{.c}
 *      // Set standard mode
 *      rdpq_set_mode_standard();    
 *      
 *      // Set a combiner to sample TEX0 as-is in RGB channels, and put a fixed value
 *      // as alpha channel, coming from the ENV register.
 *      rdpq_mode_combiner(RDPQ_COMBINER((ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, ENV)));
 * 
 *      // Set the fixed value in the ENV register. RGB components are ignored as the slot
 *      // ENV is not used in the RGB combiner formula, so we just put zero there.
 *      rdpq_set_env_color(RGBA32(0, 0, 0, 0.7*255));
 * 
 *      // Activate blending with the background
 *      rdpq_mode_blender
 * 
 * 
 * @param[in]  rgb      The RGB formula as `(A, B, C, D)`
 * @param[in]  alpha    The ALPHA formula as `(A, B, C, D)`
 * 
 * @hideinitializer
 */
#define RDPQ_COMBINER1(rgb, alpha) \
    (__rdpq_1cyc_comb_rgb rgb   | __rdpq_1cyc_comb_alpha alpha)
#define RDPQ_COMBINER2(rgb0, alpha0, rgb1, alpha1) \
    (__rdpq_2cyc_comb2a_rgb rgb0 | __rdpq_2cyc_comb2a_alpha alpha0 | \
     __rdpq_2cyc_comb2b_rgb rgb1 | __rdpq_2cyc_comb2b_alpha alpha1 | \
     RDPQ_COMBINER_2PASS)


#define SOM_ATOMIC_PRIM        ((cast64(1))<<55)

#define SOM_CYCLE_1            ((cast64(0))<<52)
#define SOM_CYCLE_2            ((cast64(1))<<52)
#define SOM_CYCLE_COPY         ((cast64(2))<<52)
#define SOM_CYCLE_FILL         ((cast64(3))<<52)
#define SOM_CYCLE_MASK         ((cast64(3))<<52)

#define SOM_TEXTURE_PERSP      (cast64(1)<<51)
#define SOM_TEXTURE_DETAIL     (cast64(1)<<50)
#define SOM_TEXTURE_SHARPEN    (cast64(1)<<49)
#define SOM_TEXTURE_LOD        (cast64(1)<<48)

#define SOM_TLUT_NONE          (cast64(0)<<46)
#define SOM_TLUT_RGBA16        (cast64(2)<<46)
#define SOM_TLUT_IA16          (cast64(3)<<46)
#define SOM_TLUT_MASK          (cast64(3)<<46)

#define SOM_SAMPLE_MASK        (cast64(3)<<44)
#define SOM_SAMPLE_1X1         (cast64(0)<<44)
#define SOM_SAMPLE_2X2         (cast64(2)<<44)
#define SOM_SAMPLE_MIDTEXEL    (cast64(3)<<44)

#define SOM_TC_FILTER          (cast64(6)<<41)
#define SOM_TC_FILTERCONV      (cast64(5)<<41)
#define SOM_TC_CONV            (cast64(0)<<41)

#define SOM_TF_POINT           (cast64(0)<<44)
#define SOM_TF_BILERP          (cast64(2)<<44)
#define SOM_TF_AVERAGE         (cast64(3)<<44)

#define SOM_RGBDITHER_SQUARE   ((cast64(0))<<38)
#define SOM_RGBDITHER_BAYER    ((cast64(1))<<38)
#define SOM_RGBDITHER_NOISE    ((cast64(2))<<38)
#define SOM_RGBDITHER_NONE     ((cast64(3))<<38)
#define SOM_RGBDITHER_MASK     ((cast64(4))<<38)
#define SOM_RGBDITHER_SHIFT    38

#define SOM_ALPHADITHER_SQUARE ((cast64(0))<<36)
#define SOM_ALPHADITHER_BAYER  ((cast64(1))<<36)
#define SOM_ALPHADITHER_NOISE  ((cast64(2))<<36)
#define SOM_ALPHADITHER_NONE   ((cast64(3))<<36)
#define SOM_ALPHADITHER_MASK   ((cast64(4))<<36)
#define SOM_ALPHADITHER_SHIFT  36

#define SOM_BLEND0_MASK        (cast64(0xCCCC0000) | SOM_BLENDING | SOM_READ_ENABLE | RDPQ_BLENDER_2PASS)
#define SOM_BLEND1_MASK        (cast64(0x33330000) | SOM_BLENDING | SOM_READ_ENABLE | RDPQ_BLENDER_2PASS)
#define SOM_BLEND_MASK         (SOM_BLEND0_MASK | SOM_BLEND1_MASK)
#define SOM_BLENDING           ((cast64(1))<<14)
#define SOM_ALPHA_USE_CVG      ((cast64(1))<<13)
#define SOM_CVG_TIMES_ALPHA    ((cast64(1))<<12)
#define SOM_Z_OPAQUE           ((cast64(0))<<10)
#define SOM_Z_INTERPENETRATING ((cast64(1))<<10)
#define SOM_Z_TRANSPARENT      ((cast64(2))<<10)
#define SOM_Z_DECAL            ((cast64(3))<<10)
#define SOM_Z_WRITE            ((cast64(1))<<5)
#define SOM_Z_COMPARE          ((cast64(1))<<4)
#define SOM_Z_SOURCE_PIXEL     ((cast64(0))<<2)
#define SOM_Z_SOURCE_PRIM      ((cast64(1))<<2)
#define SOM_ALPHADITHER_ENABLE ((cast64(1))<<1)
#define SOM_ALPHA_COMPARE      ((cast64(1))<<0)
#define SOM_ALPHACOMPARE_MASK  ((cast64(3))<<0)

#define SOM_READ_ENABLE                 ((cast64(1)) << 6)
#define SOM_AA_ENABLE                   ((cast64(1)) << 3)
#define SOM_COVERAGE_DEST_CLAMP         ((cast64(0)) << 8)
#define SOM_COVERAGE_DEST_WRAP          ((cast64(1)) << 8)
#define SOM_COVERAGE_DEST_ZAP           ((cast64(2)) << 8)
#define SOM_COVERAGE_DEST_SAVE          ((cast64(3)) << 8)
#define SOM_COLOR_ON_COVERAGE           ((cast64(1)) << 7)

#define _RDPQ_SOM_BLEND1_A_IN_RGB         cast64(0)
#define _RDPQ_SOM_BLEND1_A_MEMORY_RGB     cast64(1)
#define _RDPQ_SOM_BLEND1_A_BLEND_RGB      cast64(2)
#define _RDPQ_SOM_BLEND1_A_FOG_RGB        cast64(3)

#define _RDPQ_SOM_BLEND1_B1_IN_ALPHA       cast64(0)
#define _RDPQ_SOM_BLEND1_B1_FOG_ALPHA      cast64(1)
#define _RDPQ_SOM_BLEND1_B1_SHADE_ALPHA    cast64(2)
#define _RDPQ_SOM_BLEND1_B1_ZERO           cast64(3)

#define _RDPQ_SOM_BLEND1_B2_INV_MUX_ALPHA  cast64(0)
#define _RDPQ_SOM_BLEND1_B2_MEMORY_ALPHA   cast64(1)
#define _RDPQ_SOM_BLEND1_B2_ONE            cast64(2)
#define _RDPQ_SOM_BLEND1_B2_ZERO           cast64(3)

#define _RDPQ_SOM_BLEND2A_A_IN_RGB        cast64(0)
#define _RDPQ_SOM_BLEND2A_A_BLEND_RGB     cast64(2)
#define _RDPQ_SOM_BLEND2A_A_FOG_RGB       cast64(3)

#define _RDPQ_SOM_BLEND2A_B1_IN_ALPHA       cast64(0)
#define _RDPQ_SOM_BLEND2A_B1_FOG_ALPHA      cast64(1)
#define _RDPQ_SOM_BLEND2A_B1_SHADE_ALPHA    cast64(2)
#define _RDPQ_SOM_BLEND2A_B1_ZERO           cast64(3)

#define _RDPQ_SOM_BLEND2A_B2_INV_MUX_ALPHA  cast64(0)    // only valid option is "1-b1" in the first pass

#define _RDPQ_SOM_BLEND2B_A_CYCLE1_RGB    cast64(0)
#define _RDPQ_SOM_BLEND2B_A_MEMORY_RGB    cast64(1)
#define _RDPQ_SOM_BLEND2B_A_BLEND_RGB     cast64(2)
#define _RDPQ_SOM_BLEND2B_A_FOG_RGB       cast64(3)

#define _RDPQ_SOM_BLEND2B_B1_CYCLE1_ALPHA   cast64(0)
#define _RDPQ_SOM_BLEND2B_B1_FOG_ALPHA      cast64(1)
#define _RDPQ_SOM_BLEND2B_B1_SHADE_ALPHA    cast64(2)
#define _RDPQ_SOM_BLEND2B_B1_ZERO           cast64(3)

#define _RDPQ_SOM_BLEND2B_B2_INV_MUX_ALPHA  cast64(0)
#define _RDPQ_SOM_BLEND2B_B2_MEMORY_ALPHA   cast64(1)
#define _RDPQ_SOM_BLEND2B_B2_ONE            cast64(2)
#define _RDPQ_SOM_BLEND2B_B2_ZERO           cast64(3)

#define _RDPQ_SOM_BLEND_EXTRA_A_IN_RGB          cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_A_MEMORY_RGB      (SOM_READ_ENABLE | RDPQ_BLENDER_2PASS)
#define _RDPQ_SOM_BLEND_EXTRA_A_BLEND_RGB       cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_A_FOG_RGB         cast64(0)

#define _RDPQ_SOM_BLEND_EXTRA_B1_IN_ALPHA       cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_B1_FOG_ALPHA      cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_B1_SHADE_ALPHA    cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_B1_ZERO           cast64(0)

#define _RDPQ_SOM_BLEND_EXTRA_B2_INV_MUX_ALPHA  cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_B2_MEMORY_ALPHA   (SOM_READ_ENABLE | RDPQ_BLENDER_2PASS)
#define _RDPQ_SOM_BLEND_EXTRA_B2_ONE            cast64(0)
#define _RDPQ_SOM_BLEND_EXTRA_B2_ZERO           cast64(0)

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

#define RDPQ_BLENDER_2PASS     cast64(1<<15)

#define RDPQ_BLENDER(bl)        (__rdpq_blend_1cyc_0 bl  | __rdpq_blend_1cyc_1 bl)
// #define RDPQ_BLENDER2(bl0, bl1)  (__rdpq_blend_2cyc_0 bl0 | __rdpq_blend_2cyc_1 bl1 | RDPQ_BLENDER_2PASS)

#endif
