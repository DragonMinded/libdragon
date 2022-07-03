#ifndef RDP_COMMANDS_H
#define RDP_COMMANDS_H

#ifndef __ASSEMBLER__
#include <stdint.h>
#define cast64(x) (uint64_t)(x)
#else
#define cast64(x) x
#endif

#define RDP_TILE_FORMAT_RGBA  0
#define RDP_TILE_FORMAT_YUV   1
#define RDP_TILE_FORMAT_INDEX 2
#define RDP_TILE_FORMAT_IA    3
#define RDP_TILE_FORMAT_I     4

#define RDP_TILE_SIZE_4BIT  0
#define RDP_TILE_SIZE_8BIT  1
#define RDP_TILE_SIZE_16BIT 2
#define RDP_TILE_SIZE_32BIT 3

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

#define RDPQ_COMB0_MASK    ((cast64(0xF)<<52)|(cast64(0xF)<<47)|(cast64(0x7)<<44)|(cast64(0x7)<<41)|(cast64(0xF)<<28)|(cast64(0x7)<<15)|(cast64(0x7)<<12)|(cast64(0x7)<<9))
#define RDPQ_COMB1_MASK    (~RDPQ_COMB0_MASK & cast64(0x00FFFFFFFFFFFFFF))

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

#define RDPQ_COMBINER_2PASS   (cast64(1)<<63)

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

#define SOM_ENABLE_TLUT_RGB16  (cast64(2)<<46)
#define SOM_ENABLE_TLUT_I88    (cast64(3)<<46)

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

#define SOM_BLEND0_MASK        (cast64(0x33330000) | SOM_BLENDING | SOM_READ_ENABLE | RDPQ_BLENDER_2PASS)
#define SOM_BLEND1_MASK        (cast64(0xCCCC0000) | SOM_BLENDING | SOM_READ_ENABLE | RDPQ_BLENDER_2PASS)
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

#define SOM_BLEND_A_PIXEL_RGB       cast64(0)
#define SOM_BLEND_A_CYCLE1_RGB      cast64(0)
#define SOM_BLEND_A_MEMORY_RGB      cast64(1)
#define SOM_BLEND_A_BLEND_RGB       cast64(2)
#define SOM_BLEND_A_FOG_RGB         cast64(3)

#define SOM_BLEND_B1_MUX_ALPHA      cast64(0)
#define SOM_BLEND_B1_FOG_ALPHA      cast64(1)
#define SOM_BLEND_B1_SHADE_ALPHA    cast64(2)
#define SOM_BLEND_B1_ZERO           cast64(3)

#define SOM_BLEND_B2_INV_MUX_ALPHA  cast64(0)
#define SOM_BLEND_B2_MEMORY_ALPHA   cast64(1)
#define SOM_BLEND_B2_ONE            cast64(2)
#define SOM_BLEND_B2_ZERO           cast64(3)

#define __rdpq_blend_0(a1, b1, a2, b2) \
    (((SOM_BLEND_A_ ## a1) << 30) | ((SOM_BLEND_B1_ ## b1) << 26) | ((SOM_BLEND_A_ ## a2) << 22) | ((SOM_BLEND_B2_ ## b2) << 18))
#define __rdpq_blend_1(a1, b1, a2, b2) \
    (((SOM_BLEND_A_ ## a1) << 28) | ((SOM_BLEND_B1_ ## b1) << 24) | ((SOM_BLEND_A_ ## a2) << 20) | ((SOM_BLEND_B2_ ## b2) << 16))

#define Blend(a1, b1, a2, b2) \
    (__rdpq_blend_0(a1, b1, a2, b2) | __rdpq_blend_1(a1, b1, a2, b2))

#define __rdpq_blend(a1, b1, a2, b2, sa1, sb1, sa2, sb2) ({ \
    uint32_t _bl = \
        ((SOM_BLEND_A_  ## a1) << sa1) | \
        ((SOM_BLEND_B1_ ## b1) << sb1) | \
        ((SOM_BLEND_A_  ## a2) << sa2) | \
        ((SOM_BLEND_B2_ ## b2) << sb2);  \
    if ((SOM_BLEND_A_  ## a1) == SOM_BLEND_A_MEMORY_RGB || \
        (SOM_BLEND_A_  ## a2) == SOM_BLEND_A_MEMORY_RGB || \
        (SOM_BLEND_B2_ ## b2) == SOM_BLEND_B2_MEMORY_ALPHA) \
        _bl |= SOM_READ_ENABLE; \
    _bl | SOM_BLENDING; \
})

#define __rdpq_blend0(a1, b1, a2, b2) __rdpq_blend(a1, b1, a2, b2, 30, 26, 22, 18)
#define __rdpq_blend1(a1, b1, a2, b2) __rdpq_blend(a1, b1, a2, b2, 28, 24, 20, 16)

#define RDPQ_BLENDER_2PASS     (1<<15)

#define RDPQ_BLENDER1(bl)        (SOM_BLENDING | __rdpq_blend0 bl  | __rdpq_blend1 bl)
#define RDPQ_BLENDER2(bl0, bl1)  (SOM_BLENDING | __rdpq_blend0 bl0 | __rdpq_blend1 bl1 | RDPQ_BLENDER_2PASS)

#endif
