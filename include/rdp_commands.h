#ifndef RDP_COMMANDS_H
#define RDP_COMMANDS_H

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

// When compiling C/C++ code, 64-bit immediate operands require explicit
// casting to a 64-bit type
#ifdef __ASSEMBLER__
#define cast64(x) (x)
#else
#include <stdint.h>
#define cast64(x) (uint64_t)(x)
#endif

#define RdpSetClippingFX(x0,y0,x1,y1) \
    ((cast64(0x2D))<<56 | (cast64(x0)<<44) | (cast64(y0)<<32) | ((x1)<<12) | ((y1)<<0))
#define RdpSetClippingI(x0,y0,x1,y1) RdpSetClippingFX((x0)<<2, (y0)<<2, (x1)<<2, (y1)<<2)
#define RdpSetClippingF(x0,y0,x1,y1) RdpSetClippingFX((int)((x0)*4), (int)((y0)*4), (int)((x1)*4), (int)((y1)*4))

#define RdpSetKeyGb(wg, wb, cg, sg, cb, sb) \
    ((cast64(0x2A)<<56) | ((cast64((wg))&0xFFF)<<44) | ((cast64((wb))&0xFFF)<<32) | ((cast64((cg))&0xFF)<<24) | ((cast64((sg))&0xFF)<<16) | ((cast64((cb))&0xFF)<<8) | ((cast64((sb))&0xFF)<<0))

#define RdpSetKeyR(wr, cr, sr) \
    ((cast64(0x2B)<<56) | ((cast64((wr))&0xFFF)<<16) | ((cast64((cr))&0xFF)<<8) | ((cast64((sr))&0xFF)<<0))

#define RdpSetConvert(k0,k1,k2,k3,k4,k5) \
    ((cast64(0x2C)<<56) | ((cast64((k0))&0x1FF)<<45) | ((cast64((k1))&0x1FF)<<36) | ((cast64((k2))&0x1FF)<<27) | ((cast64((k3))&0x1FF)<<18) | ((cast64((k4))&0x1FF)<<9) | ((cast64((k5))&0x1FF)<<0))

#define RdpSetTile(fmt, size, line, addr, tidx, palette, ct, mt, maskt, shiftt, cs, ms, masks, shifts) \
    ((cast64(0x35)<<56) | (cast64((fmt)) << 53) | (cast64((size)) << 51) | (cast64((line)) << 41) | (cast64((addr)) << 32) | ((tidx) << 24) | (cast64((palette)&0xF)<<20) | \
     (cast64((ct)&0x1)<<19) | (cast64((mt)&0x1)<<18) | (cast64((maskt)&0xF)<<14) | (cast64((shiftt)&0xF)<<10) | (cast64((cs)&0x1)<<9) | (cast64((ms)&0x1)<<8) | (cast64((masks)&0xF)<<4) | (cast64((shifts)&0xF)<<0))

#ifndef __ASSEMBLER__
    #define RdpSetTexImage(fmt, size, addr, width) \
        ({ \
            assertf(size != RDP_TILE_SIZE_4BIT, "RdpSetTexImage cannot be called with RDP_TILE_SIZE_4BIT"); \
            ((cast64(0x3D)<<56) | ((addr) & 0x3FFFFF) | (cast64(((width))-1)<<32) | (cast64((fmt))<<53) | (cast64((size))<<51)); \
        })
#else
    #define RdpSetTexImage(fmt, size, addr, width) \
        ((cast64(0x3D)<<56) | ((addr) & 0x3FFFFF) | (cast64(((width))-1)<<32) | (cast64((fmt))<<53) | (cast64((size))<<51))
#endif

#define RdpLoadBlock(tidx,s0,t0,s1,dxt) \
    ((cast64(0x33)<<56) | (cast64((tidx))<<24) | (cast64((s0))<<44) | (cast64((t0))<<32) | ((s1)<<12) | ((dxt)<<0))

#define RdpLoadTileFX(tidx,s0,t0,s1,t1) \
    ((cast64(0x34)<<56) | (cast64((tidx))<<24) | (cast64((s0))<<44) | (cast64((t0))<<32) | ((s1)<<12) | ((t1)<<0))
#define RdpLoadTileI(tidx,s0,t0,s1,t1) RdpLoadTileFX(tidx, (s0)<<2, (t0)<<2, (s1)<<2, (t1)<<2)

#define RdpLoadTlut(tidx, lowidx, highidx) \
    ((cast64(0x30)<<56) | (cast64(tidx) << 24) | (cast64(lowidx)<<46) | (cast64(highidx)<<14))

#define RdpSetTileSizeFX(tidx,s0,t0,s1,t1) \
    ((cast64(0x32)<<56) | ((tidx)<<24) | (cast64(s0)<<44) | (cast64(t0)<<32) | ((s1)<<12) | ((t1)<<0))
#define RdpSetTileSizeI(tidx,s0,t0,s1,t1) \
    RdpSetTileSizeFX(tidx, (s0)<<2, (t0)<<2, (s1)<<2, (t1)<<2)

#define RdpTextureRectangle1FX(tidx,x0,y0,x1,y1) \
    ((cast64(0x24)<<56) | (cast64((x1)&0xFFF)<<44) | (cast64((y1)&0xFFF)<<32) | ((tidx)<<24) | (((x0)&0xFFF)<<12) | (((y0)&0xFFF)<<0))
#define RdpTextureRectangle1I(tidx,x0,y0,x1,y1) \
    RdpTextureRectangle1FX(tidx, (x0)<<2, (y0)<<2, (x1)<<2, (y1)<<2)
#define RdpTextureRectangle1F(tidx,x0,y0,x1,y1) \
    RdpTextureRectangle1FX(tidx, (int32_t)((x0)*4.f), (int32_t)((y0)*4.f), (int32_t)((x1)*4.f), (int32_t)((y1)*4.f))

#define RdpTextureRectangleFlip1FX(tidx,x0,y0,x1,y1) \
    ((cast64(0x25)<<56) | (cast64((x1)&0xFFF)<<44) | (cast64((y1)&0xFFF)<<32) | ((tidx)<<24) | (((x0)&0xFFF)<<12) | (((y0)&0xFFF)<<0))
#define RdpTextureRectangleFlip1I(tidx,x0,y0,x1,y1) \
    RdpTextureRectangleFlip1FX(tidx, (x0)<<2, (y0)<<2, (x1)<<2, (y1)<<2)
#define RdpTextureRectangleFlip1F(tidx,x0,y0,x1,y1) \
    RdpTextureRectangleFlip1FX(tidx, (int32_t)((x0)*4.f), (int32_t)((y0)*4.f), (int32_t)((x1)*4.f), (int32_t)((y1)*4.f))

#define RdpTextureRectangle2FX(s,t,ds,dt) \
    ((cast64((s)&0xFFFF)<<48) | (cast64((t)&0xFFFF)<<32) | (cast64((ds)&0xFFFF)<<16) | (cast64((dt)&0xFFFF)<<0))
#define RdpTextureRectangle2I(s,t,ds,dt) \
    RdpTextureRectangle2FX((s)<<5, (t)<<5, (ds)<<10, (dt)<<10)
#define RdpTextureRectangle2F(s,t,ds,dt) \
    RdpTextureRectangle2FX((int32_t)((s)*32.f), (int32_t)((t)*32.f), (int32_t)((ds)*1024.f), (int32_t)((dt)*1024.f))

#define RdpSetColorImage(fmt, size, width, addr) \
    ((cast64(0x3f)<<56) | (cast64((fmt)&0x7)<<53) | (cast64((size)&0x3)<<51) | (cast64((width)-1)<<32) | (((addr)&0x3FFFFF)<<0))

#define RdpSetDepthImage(addr) \
    ((cast64(0x3e)<<56) | (((addr)&0x3FFFFF)<<0))

#define RdpFillRectangleFX(x0,y0,x1,y1) \
    ((cast64(0x36)<<56) | ((x0)<<12) | ((y0)<<0) | (cast64(x1)<<44) | (cast64(y1)<<32))
#define RdpFillRectangleI(x0,y0,x1,y1) RdpFillRectangleFX((x0)<<2, (y0)<<2, (x1)<<2, (y1)<<2)
#define RdpFillRectangleF(x0,y0,x1,y1) RdpFillRectangleFX((int)((x0)*4), (int)((y0)*4), (int)((x1)*4), (int)((y1)*4))

#define RdpSetFillColor16(color) \
    (((cast64(0x37))<<56) | (cast64(color)<<16) | (color))

#define RdpSetFillColor(color) \
    (((cast64(0x37))<<56) | (color))

#define RdpSetPrimColor(color) \
    (((cast64(0x3a))<<56) | (color))

#define RdpSetPrimDepth(z, dz) \
    ((cast64(0x2e)<<56) | (cast64((z)&0xFFFF)<<16) | (cast64((dz)&0xFFFF)<<0))

#define RdpSetEnvColor(color) \
    (((cast64(0x3b))<<56) | (color))

#define RdpSetBlendColor(color) \
    (((cast64(0x39))<<56) | (color))

#define RdpSetFogColor(color) \
    (((cast64(0x38))<<56) | (color))

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

// RDP command to configure the color combiner. Pass to this macro
// up to 4 Comb* macros as arguments. For instance:
//    RdpSetCommand(Comb1_Rgb(TEX0, TEX1, SHADE, ONE))
// Remember that in 1-cycle mode, you need to use Comb1.
#define RdpSetCombine(...) \
    ((cast64(0x3C)<<56) | _ORBITS_MULTI(__VA_ARGS__))


#define SOM_CYCLE_1    ((cast64(0))<<52)
#define SOM_CYCLE_2    ((cast64(1))<<52)
#define SOM_CYCLE_COPY ((cast64(2))<<52)
#define SOM_CYCLE_FILL ((cast64(3))<<52)

#define SOM_TEXTURE_DETAIL     (cast64(1)<<50)
#define SOM_TEXTURE_SHARPEN    (cast64(1)<<49)

#define SOM_ENABLE_TLUT_RGB16  (cast64(2)<<46)
#define SOM_ENABLE_TLUT_I88    (cast64(3)<<46)

#define SOM_SAMPLE_1X1         (cast64(0)<<45)
#define SOM_SAMPLE_2X2         (cast64(1)<<45)
#define SOM_MIDTEXEL           (cast64(1)<<44)

#define SOM_TC_FILTER          (cast64(0)<<41)  // NOTE: this values are bit-inverted, so that they end up with a good default
#define SOM_TC_FILTERCONV      (cast64(3)<<41)
#define SOM_TC_CONV            (cast64(6)<<41)

#define SOM_RGBDITHER_SQUARE   ((cast64(0))<<38)
#define SOM_RGBDITHER_BAYER    ((cast64(1))<<38)
#define SOM_RGBDITHER_NOISE    ((cast64(2))<<38)
#define SOM_RGBDITHER_NONE     ((cast64(3))<<38)

#define SOM_ALPHADITHER_SQUARE ((cast64(0))<<36)
#define SOM_ALPHADITHER_BAYER  ((cast64(1))<<36)
#define SOM_ALPHADITHER_NOISE  ((cast64(2))<<36)
#define SOM_ALPHADITHER_NONE   ((cast64(3))<<36)

#define SOM_BLENDING           ((cast64(1))<<14)
#define SOM_Z_WRITE            ((cast64(1))<<5)
#define SOM_Z_COMPARE          ((cast64(1))<<4)
#define SOM_ALPHA_COMPARE      ((cast64(1))<<0)

#define SOM_READ_ENABLE                 ((cast64(1)) << 6)
#define SOM_AA_ENABLE                   ((cast64(1)) << 3)
#define SOM_COVERAGE_DEST_CLAMP         ((cast64(0)) << 8)
#define SOM_COVERAGE_DEST_WRAP          ((cast64(1)) << 8)
#define SOM_COVERAGE_DEST_ZAP           ((cast64(2)) << 8)
#define SOM_COVERAGE_DEST_SAVE          ((cast64(3)) << 8)
#define SOM_COLOR_ON_COVERAGE           ((cast64(1)) << 7)


#define RdpSetOtherModes(som_flags) \
    ((cast64(0x2f)<<56) | ((som_flags) ^ (cast64(6)<<41)))

#define RdpSyncFull() \
    (cast64(0x29)<<56)
#define RdpSyncLoad() \
    (cast64(0x26)<<56)
#define RdpSyncPipe() \
    (cast64(0x27)<<56)
#define RdpSyncTile() \
    (cast64(0x28)<<56)

/**********************************************************
 * Mid-level macros
 **********************************************************/

#define RDP_AUTO_TMEM_SLOT(n)   (-(n))
#define RDP_AUTO_PITCH          (-1)

#define RDP_NUM_SLOTS_TILE4BPP(w, h)   (0x800 / ((w)*(h)/2))
#define RDP_NUM_SLOTS_PALETTE16        16

/**
 * MRdpLoadTex4bpp - Display list for loading a 4bpp texture into TMEM
 *
 * @param tidx          Tile ID (0-7)
 * @param rdram_addr    Address of the texture in RDRAM
 * @param width         Width of the texture in pixels
 * @param height        Height of the texture in pixels
 * @param pitch         Pitch of the texture in RDRAM in bytes, 
 *                      or RDP_AUTO_PITCH in case the texture is linear in memory.
 * @param tmem_addr     Address of TMEM where to load the texture,
 *                      or RDP_AUTO_TMEM_SLOT(n) to load the texture in the Nth
 *                      available slot for textures of this size.
 * @param tmem_pitch    Pitch of the texture in TMEM in bytes,
 *                      or RDP_AUTO_PITCH to store the texture linearly.
 *
 * @note RDP_AUTO_TMEM_SLOT(n) allow to allocate TMEM using slots of fixed size.
 *       The slot size is calculated given the texture width / height. You can
 *       use RDP_NUM_SLOTS_TILE4BPP to calculate how many slots are available
 *       for a given texture size. If you need to load textures of different
 *       sizes, RDP_AUTO_TMEM_SLOT cannot be used, and TMEM addresses must
 *       be calculated manually.
 */
#ifndef __ASSEMBLER__
    #define MRdpLoadTex4bpp(tidx, rdram_addr, width, height, pitch, tmem_addr, tmem_pitch) \
        RdpSetTile(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_8BIT, (tmem_pitch) < 0 ? (width)/8 : tmem_pitch/8, (tmem_addr) < 0 ? -(tmem_addr) * (width)*(height)/2/8 : tmem_addr, tidx), \
        RdpSetTexImage(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_8BIT, rdram_addr, (pitch) < 0 ? (width)/2 : (pitch)), \
        RdpLoadTileI(tidx, 0, 0, (width)/2, (height))
#else
    #define MRdpLoadTex4bpp_Slot_Autopitch(tidx, rdram_addr, width, height, tmem_addr) \
        RdpSetTile(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_8BIT, (width)/8, -(tmem_addr) * (width)*(height)/2/8, tidx), \
        RdpSetTexImage(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_8BIT, rdram_addr, (width)/2), \
        RdpLoadTileI(tidx, 0, 0, (width)/2, (height))
#endif

/**
 * MRdpLoadPalette16 - Display list for loading a 16-color palette into TMEM
 *
 * @param tid           Tile ID (0-7)
 * @param rdram_addr    Address of the palette in RDRAM
 * @param tmem_addr     Address of the palette in TMEM,
 *                      or RDP_AUTO_TMEM_SLOT(n) to load the palette into the Nth
 *                      available slot for palettes of 16 colors.
 *
 * @note The maximum number of 16-bit palettes that can be stored in TMEM is
 * RDRDP_NUM_SLOTS_PALETTE16 (16).
 *
 */
#ifndef __ASSEMBLER__
    #define MRdpLoadPalette16(tidx, rdram_addr, tmem_addr) \
        RdpSetTile(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_4BIT, 16, ((tmem_addr) <= 0 ? (0x800 + -(tmem_addr)*(16*2*4)) : tmem_addr)/8, tidx), \
        RdpSetTexImage(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_16BIT, rdram_addr, 16), \
        RdpLoadTlut(tidx, 0, 15)
#else
    #define MRdpLoadPalette16_Addr(tidx, rdram_addr, tmem_addr) \
        RdpSetTile(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_4BIT, 16, tmem_addr/8, tidx), \
        RdpSetTexImage(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_16BIT, rdram_addr, 16), \
        RdpLoadTlut(tidx, 0, 15)
    #define MRdpLoadPalette16_Slot(tidx, rdram_addr, slot) \
        RdpSetTile(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_4BIT, 16, (0x800 + -(slot)*(16*2*4))/8, tidx), \
        RdpSetTexImage(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_16BIT, rdram_addr, 16), \
        RdpLoadTlut(tidx, 0, 15)
#endif


/**
 * MRdpSetTile4bpp - Display list for configure a tile ID to draw a 4bpp texture
 *
 * @param tidx             Tile ID (0-7)
 * @param tmem_tex_addr    Address in TMEM of the texture, or RDP_AUTO_TMEM_SLOT
 *                         to select the nth slot for textures of this size.
 * @param tmem_tex_pitch   Pitch in TMEM of the texture in bytes, or RDP_AUTO_PITCH
 *                         if the texture is stored linearly.
 * @param tmem_pal_addr    Address in TMEM of the palette, or RDP_AUTO_TMEM_SLOT
 *                         to select the nth available palette.
 * @param width            Width of the texture in pixels
 * @param height           Height of the texture in pixels
 *
 * @note You can load TMEM using MRdpLoadTile4bpp and MRdpLoadPalette16.
 */

#ifndef __ASSEMBLER__
    #define MRdpSetTile4bpp(tidx, tmem_tex_addr, tmem_tex_pitch, tmem_pal_addr, width, height) \
        RdpSetTile(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_4BIT, \
            (tmem_tex_pitch) < 0 ? (width)/8 : tmem_tex_pitch, \
            (tmem_tex_addr) < 0 ? -(tmem_tex_addr) * (width)*(height)/2/8 : tmem_tex_addr, tidx) \
            | (((tmem_pal_addr)<0 ? -(tmem_pal_addr) : ((tmem_pal_addr)&0x780)>>7) << 20), \
        RdpSetTileSizeI(tidx, 0, 0, (width)-1, (height)-1)
#else
    #define MRdpSetTile4bpp_Slot_Autopitch(tidx, tmem_tex_addr, tmem_pal_addr, width, height) \
        RdpSetTile(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_4BIT, \
            (width)/8, \
            -(tmem_tex_addr) * (width)*(height)/2/8, tidx) \
            | ((-(tmem_pal_addr)) << 20), \
        RdpSetTileSizeI(tidx, 0, 0, (width)-1, (height)-1)
#endif

/**
 * MRdpDrawRect4bpp - Display list for drawing a 4bpp textured rectangle
 *
 * @param tidx             Tile ID (0-7) previously setup using MRdpSetTile4bpp
 * @param x                X coordinate of the rectangle
 * @param y                Y coordinate of the rectangle
 * @param w                width of the rectangle
 * @param h                height of the rectangle
 *
 */

#define MRdpTextureRectangle4bpp(tidx, x, y, w, h) \
    RdpTextureRectangle1I(tidx, x, y, (x)+(w)-1, (y)+(h)-1), \
    RdpTextureRectangle2I(0, 0, 4, 1)


#endif
