#ifndef RDP_COMMANDS_H
#define RDP_COMMANDS_H

#include <stdint.h>

#define RDP_TILE_FORMAT_RGBA  0
#define RDP_TILE_FORMAT_YUV   1
#define RDP_TILE_FORMAT_INDEX 2
#define RDP_TILE_FORMAT_IA    3
#define RDP_TILE_FORMAT_I     4

#define RDP_TILE_SIZE_4BIT  0
#define RDP_TILE_SIZE_8BIT  1
#define RDP_TILE_SIZE_16BIT 2
#define RDP_TILE_SIZE_32BIT 3

#define RDP_COLOR16(r,g,b,a) (uint32_t)(((r)<<11)|((g)<<6)|((b)<<1)|(a))
#define RDP_COLOR32(r,g,b,a) (uint32_t)(((r)<<24)|((g)<<16)|((b)<<8)|(a))

#define cast64(x) (uint64_t)(x)

#define _NUM_ARGS2(X,X64,X63,X62,X61,X60,X59,X58,X57,X56,X55,X54,X53,X52,X51,X50,X49,X48,X47,X46,X45,X44,X43,X42,X41,X40,X39,X38,X37,X36,X35,X34,X33,X32,X31,X30,X29,X28,X27,X26,X25,X24,X23,X22,X21,X20,X19,X18,X17,X16,X15,X14,X13,X12,X11,X10,X9,X8,X7,X6,X5,X4,X3,X2,X1,N,...) N
#define NUM_ARGS(...) _NUM_ARGS2(0, __VA_ARGS__ ,64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)

#define _ORBITS1(a)     cast64((a))
#define _ORBITS2(a,b)   ((a) | (b))
#define _ORBITS3(a,...) ((a) | _ORBITS2(__VA_ARGS__))
#define _ORBITS4(a,...) ((a) | _ORBITS3(__VA_ARGS__))
#define _ORBITS_MULTI3(N, ...) _ORBITS ## N (__VA_ARGS__)
#define _ORBITS_MULTI2(N, ...) _ORBITS_MULTI3(N, __VA_ARGS__)
#define _ORBITS_MULTI(...)  _ORBITS_MULTI2(NUM_ARGS(__VA_ARGS__), __VA_ARGS__)

#define COMB_RGB_SUBA_COMBINED  cast64(0)
#define COMB_RGB_SUBA_TEX0      cast64(1)
#define COMB_RGB_SUBA_TEX1      cast64(2)
#define COMB_RGB_SUBA_PRIM      cast64(3)
#define COMB_RGB_SUBA_SHADE     cast64(4)
#define COMB_RGB_SUBA_ENV       cast64(5)
#define COMB_RGB_SUBA_ONE       cast64(6)
#define COMB_RGB_SUBA_NOISE     cast64(7)
#define COMB_RGB_SUBA_ZERO      cast64(8)

#define COMB_RGB_SUBB_COMBINED  cast64(0)
#define COMB_RGB_SUBB_TEX0      cast64(1)
#define COMB_RGB_SUBB_TEX1      cast64(2)
#define COMB_RGB_SUBB_PRIM      cast64(3)
#define COMB_RGB_SUBB_SHADE     cast64(4)
#define COMB_RGB_SUBB_ENV       cast64(5)
#define COMB_RGB_SUBB_KEYCENTER cast64(6)
#define COMB_RGB_SUBB_K4        cast64(7)
#define COMB_RGB_SUBB_ZERO      cast64(8)

#define COMB_RGB_MUL_COMBINED       cast64(0)
#define COMB_RGB_MUL_TEX0           cast64(1)
#define COMB_RGB_MUL_TEX1           cast64(2)
#define COMB_RGB_MUL_PRIM           cast64(3)
#define COMB_RGB_MUL_SHADE          cast64(4)
#define COMB_RGB_MUL_ENV            cast64(5)
#define COMB_RGB_MUL_KEYSCALE       cast64(6)
#define COMB_RGB_MUL_COMBINED_ALPHA cast64(7)
#define COMB_RGB_MUL_TEX0_ALPHA     cast64(8)
#define COMB_RGB_MUL_TEX1_ALPHA     cast64(9)
#define COMB_RGB_MUL_PRIM_ALPHA     cast64(10)
#define COMB_RGB_MUL_SHADE_ALPHA    cast64(11)
#define COMB_RGB_MUL_ENV_ALPHA      cast64(12)
#define COMB_RGB_MUL_LOD_FRAC       cast64(13)
#define COMB_RGB_MUL_PRIM_LOD_FRAC  cast64(14)
#define COMB_RGB_MUL_K5             cast64(15)
#define COMB_RGB_MUL_ZERO           cast64(16)

#define COMB_RGB_ADD_COMBINED  cast64(0)
#define COMB_RGB_ADD_TEX0      cast64(1)
#define COMB_RGB_ADD_TEX1      cast64(2)
#define COMB_RGB_ADD_PRIM      cast64(3)
#define COMB_RGB_ADD_SHADE     cast64(4)
#define COMB_RGB_ADD_ENV       cast64(5)
#define COMB_RGB_ADD_ONE       cast64(6)
#define COMB_RGB_ADD_ZERO      cast64(7)

#define COMB_ALPHA_ADDSUB_COMBINED  cast64(0)
#define COMB_ALPHA_ADDSUB_TEX0      cast64(1)
#define COMB_ALPHA_ADDSUB_TEX1      cast64(2)
#define COMB_ALPHA_ADDSUB_PRIM      cast64(3)
#define COMB_ALPHA_ADDSUB_SHADE     cast64(4)
#define COMB_ALPHA_ADDSUB_ENV       cast64(5)
#define COMB_ALPHA_ADDSUB_ONE       cast64(6)
#define COMB_ALPHA_ADDSUB_ZERO      cast64(7)

#define COMB_ALPHA_MUL_LOD_FRAC         cast64(0)
#define COMB_ALPHA_MUL_TEX0             cast64(1)
#define COMB_ALPHA_MUL_TEX1             cast64(2)
#define COMB_ALPHA_MUL_PRIM             cast64(3)
#define COMB_ALPHA_MUL_SHADE            cast64(4)
#define COMB_ALPHA_MUL_ENV              cast64(5)
#define COMB_ALPHA_MUL_PRIM_LOD_FRAC    cast64(6)
#define COMB_ALPHA_MUL_ZERO             cast64(7)

#define Comb0_Rgb(suba, subb, mul, add) \
    ((COMB_RGB_SUBA_ ## suba)<<52) | ((COMB_RGB_SUBB_ ## subb)<<28) | ((COMB_RGB_MUL_ ## mul)<<47) | ((COMB_RGB_ADD_ ## add)<<15)
#define Comb1_Rgb(suba, subb, mul, add) \
    ((COMB_RGB_SUBA_ ## suba)<<37) | ((COMB_RGB_SUBB_ ## subb)<<24) | ((COMB_RGB_MUL_ ## mul)<<32) | ((COMB_RGB_ADD_ ## add)<<6)
#define Comb0_Alpha(suba, subb, mul, add) \
    ((COMB_ALPHA_ADDSUB_ ## suba)<<44) | ((COMB_ALPHA_ADDSUB_ ## subb)<<12) | ((COMB_ALPHA_MUL_ ## mul)<<41) | ((COMB_ALPHA_ADDSUB_ ## add)<<9)
#define Comb1_Alpha(suba, subb, mul, add) \
    ((COMB_ALPHA_ADDSUB_ ## suba)<<21) | ((COMB_ALPHA_ADDSUB_ ## subb)<<3) | ((COMB_ALPHA_MUL_ ## mul)<<18) | ((COMB_ALPHA_ADDSUB_ ## add)<<0)

#define SOM_ATOMIC_PRIM        ((cast64(1))<<55)

#define SOM_CYCLE_1            ((cast64(0))<<52)
#define SOM_CYCLE_2            ((cast64(1))<<52)
#define SOM_CYCLE_COPY         ((cast64(2))<<52)
#define SOM_CYCLE_FILL         ((cast64(3))<<52)

#define SOM_TEXTURE_PERSP      (cast64(1)<<51)
#define SOM_TEXTURE_DETAIL     (cast64(1)<<50)
#define SOM_TEXTURE_SHARPEN    (cast64(1)<<49)
#define SOM_TEXTURE_LOD        (cast64(1)<<48)

#define SOM_ENABLE_TLUT_RGB16  (cast64(2)<<46)
#define SOM_ENABLE_TLUT_I88    (cast64(3)<<46)

#define SOM_SAMPLE_1X1         (cast64(0)<<45)
#define SOM_SAMPLE_2X2         (cast64(1)<<45)
#define SOM_MIDTEXEL           (cast64(1)<<44)

#define SOM_TC_FILTER          (cast64(6)<<41)
#define SOM_TC_FILTERCONV      (cast64(5)<<41)
#define SOM_TC_CONV            (cast64(0)<<41)

#define SOM_KEY_ENABLED        (cast64(1)<<41)

#define SOM_RGBDITHER_SQUARE   ((cast64(0))<<38)
#define SOM_RGBDITHER_BAYER    ((cast64(1))<<38)
#define SOM_RGBDITHER_NOISE    ((cast64(2))<<38)
#define SOM_RGBDITHER_NONE     ((cast64(3))<<38)

#define SOM_ALPHADITHER_SQUARE ((cast64(0))<<36)
#define SOM_ALPHADITHER_BAYER  ((cast64(1))<<36)
#define SOM_ALPHADITHER_NOISE  ((cast64(2))<<36)
#define SOM_ALPHADITHER_NONE   ((cast64(3))<<36)

#define SOM_BLENDING           ((cast64(1))<<14)
#define SOM_ALPHA_USE_CVG      ((cast64(1))<<13)
#define SOM_CVG_TIMES_ALPHA    ((cast64(1))<<12)
#define SOM_Z_OPAQUE           ((cast64(0))<<10)
#define SOM_Z_INTERPENETRATING ((cast64(1))<<10)
#define SOM_Z_TRANSPARENT      ((cast64(2))<<10)
#define SOM_Z_DECAL            ((cast64(3))<<10)
#define SOM_Z_WRITE            ((cast64(1))<<5)
#define SOM_Z_COMPARE          ((cast64(1))<<4)
#define SOM_Z_SOURCE_PRIM      ((cast64(0))<<2)
#define SOM_Z_SOURCE_PIXEL     ((cast64(1))<<2)
#define SOM_ALPHADITHER_ENABLE ((cast64(1))<<1)
#define SOM_ALPHA_COMPARE      ((cast64(1))<<0)

#define SOM_READ_ENABLE                 ((cast64(1)) << 6)
#define SOM_AA_ENABLE                   ((cast64(1)) << 3)
#define SOM_COVERAGE_DEST_CLAMP         ((cast64(0)) << 8)
#define SOM_COVERAGE_DEST_WRAP          ((cast64(1)) << 8)
#define SOM_COVERAGE_DEST_ZAP           ((cast64(2)) << 8)
#define SOM_COVERAGE_DEST_SAVE          ((cast64(3)) << 8)
#define SOM_COLOR_ON_COVERAGE           ((cast64(1)) << 7)

#endif
