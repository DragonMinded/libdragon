#ifndef __LIBDRAGON_RDPQ_H
#define __LIBDRAGON_RDPQ_H

#include <stdint.h>
#include <stdbool.h>
#include "graphics.h"
#include "n64sys.h"
#include "rdp_commands.h"
#include "debug.h"

enum {
    RDPQ_CMD_NOOP                       = 0x00,
    RDPQ_CMD_SET_LOOKUP_ADDRESS         = 0x01,
    RDPQ_CMD_TRI                        = 0x08,
    RDPQ_CMD_TRI_ZBUF                   = 0x09,
    RDPQ_CMD_TRI_TEX                    = 0x0A,
    RDPQ_CMD_TRI_TEX_ZBUF               = 0x0B,
    RDPQ_CMD_TRI_SHADE                  = 0x0C,
    RDPQ_CMD_TRI_SHADE_ZBUF             = 0x0D,
    RDPQ_CMD_TRI_SHADE_TEX              = 0x0E,
    RDPQ_CMD_TRI_SHADE_TEX_ZBUF         = 0x0F,

    RDPQ_CMD_TEXTURE_RECTANGLE_EX       = 0x10,
    RDPQ_CMD_TEXTURE_RECTANGLE_EX_FIX   = 0x11,
    RDPQ_CMD_SET_SCISSOR_EX             = 0x12,
    RDPQ_CMD_SET_SCISSOR_EX_FIX         = 0x13,
    RDPQ_CMD_MODIFY_OTHER_MODES         = 0x14,
    RDPQ_CMD_MODIFY_OTHER_MODES_FIX     = 0x15,
    RDPQ_CMD_SET_FILL_COLOR_32          = 0x16,
    RDPQ_CMD_SET_FILL_COLOR_32_FIX      = 0x17,
    RDPQ_CMD_SET_TEXTURE_IMAGE_FIX      = 0x1D,
    RDPQ_CMD_SET_Z_IMAGE_FIX            = 0x1E,
    RDPQ_CMD_SET_COLOR_IMAGE_FIX        = 0x1F,

    RDPQ_CMD_SET_OTHER_MODES_FIX        = 0x20,
    RDPQ_CMD_SYNC_FULL_FIX              = 0x21,
    RDPQ_CMD_TEXTURE_RECTANGLE          = 0x24,
    RDPQ_CMD_TEXTURE_RECTANGLE_FLIP     = 0x25,
    RDPQ_CMD_SYNC_LOAD                  = 0x26,
    RDPQ_CMD_SYNC_PIPE                  = 0x27,
    RDPQ_CMD_SYNC_TILE                  = 0x28,
    RDPQ_CMD_SYNC_FULL                  = 0x29,
    RDPQ_CMD_SET_KEY_GB                 = 0x2A,
    RDPQ_CMD_SET_KEY_R                  = 0x2B,
    RDPQ_CMD_SET_CONVERT                = 0x2C,
    RDPQ_CMD_SET_SCISSOR                = 0x2D,
    RDPQ_CMD_SET_PRIM_DEPTH             = 0x2E,
    RDPQ_CMD_SET_OTHER_MODES            = 0x2F,

    RDPQ_CMD_LOAD_TLUT                  = 0x30,
    RDPQ_CMD_SET_TILE_SIZE              = 0x32,
    RDPQ_CMD_LOAD_BLOCK                 = 0x33,
    RDPQ_CMD_LOAD_TILE                  = 0x34,
    RDPQ_CMD_SET_TILE                   = 0x35,
    RDPQ_CMD_FILL_RECTANGLE             = 0x36,
    RDPQ_CMD_SET_FILL_COLOR             = 0x37,
    RDPQ_CMD_SET_FOG_COLOR              = 0x38,
    RDPQ_CMD_SET_BLEND_COLOR            = 0x39,
    RDPQ_CMD_SET_PRIM_COLOR             = 0x3A,
    RDPQ_CMD_SET_ENV_COLOR              = 0x3B,
    RDPQ_CMD_SET_COMBINE_MODE           = 0x3C,
    RDPQ_CMD_SET_TEXTURE_IMAGE          = 0x3D,
    RDPQ_CMD_SET_Z_IMAGE                = 0x3E,
    RDPQ_CMD_SET_COLOR_IMAGE            = 0x3F,
};

/** @brief Used internally for bit-packing RDP commands. */
#define _carg(value, mask, shift) (((uint32_t)((value) & mask)) << shift)

#ifdef __cplusplus
extern "C" {
#endif

void rdpq_init();

void rdpq_close();

/**
 * @brief Add a fence to synchronize RSP with RDP commands.
 * 
 * This function schedules a fence in the RSP queue that makes RSP waits until
 * all previously enqueued RDP commands have finished executing. This is useful
 * in the rare cases in which you need to post-process the output of RDP with RSP
 * commands.
 * 
 * Notice that the RSP will spin-lock waiting for RDP to become idle, so, if
 * possible, call rdpq_fence as late as possible, to allow for parallel RDP/RSP
 * execution for the longest possible time.
 * 
 * Notice that this does not block the CPU in any way; the CPU will just
 * schedule the fence command in the RSP queue and continue execution. If you
 * need to block the CPU until the RDP is done, check #rspq_wait or #rdpq_sync_full
 * instead.
 * 
 * @see #rdpq_sync_full
 * @see #rspq_wait
 */
void rdpq_fence(void);


inline void rdpq_fill_triangle(bool flip, uint8_t level, uint8_t tile, int16_t yl, int16_t ym, int16_t yh, int32_t xl, int32_t dxldy, int32_t xh, int32_t dxhdy, int32_t xm, int32_t dxmdy)
{
    extern void __rdpq_fill_triangle(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    __rdpq_fill_triangle(
        _carg(flip ? 1 : 0, 0x1, 23) | _carg(level, 0x7, 19) | _carg(tile, 0x7, 16) | _carg(yl, 0x3FFF, 0),
        _carg(ym, 0x3FFF, 16) | _carg(yh, 0x3FFF, 0),
        xl,
        dxldy,
        xh,
        dxhdy,
        xm,
        dxmdy);
}

/**
 * @brief Low level function to draw a textured rectangle
 */
inline void rdpq_texture_rectangle_fx(uint8_t tile, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, int16_t s, int16_t t, int16_t dsdx, int16_t dtdy)
{
    extern void __rdpq_texture_rectangle(uint32_t, uint32_t, uint32_t, uint32_t);

    __rdpq_texture_rectangle(
        _carg(x1, 0xFFF, 12) | _carg(y1, 0xFFF, 0),
        _carg(tile, 0x7, 24) | _carg(x0, 0xFFF, 12) | _carg(y0, 0xFFF, 0),
        _carg(s, 0xFFFF, 16) | _carg(t, 0xFFFF, 0),
        _carg(dsdx, 0xFFFF, 16) | _carg(dtdy, 0xFFFF, 0));
}

#define rdpq_texture_rectangle(tile, x0, y0, x1, y1, s, t, dsdx, dtdy) ({ \
    rdpq_texture_rectangle_fx((tile), (x0)*4, (y0)*4, (x1)*4, (y1)*4, (s)*32, (t)*32, (dsdx)*1024, (dtdy)*1024); \
})

/**
 * @brief Low level function to draw a textured rectangle (s and t coordinates flipped)
 */
inline void rdpq_texture_rectangle_flip_fx(uint8_t tile, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, int16_t s, int16_t t, int16_t dsdx, int16_t dtdy)
{
    extern void __rdpq_write16(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

    __rdpq_write16(RDPQ_CMD_TEXTURE_RECTANGLE_FLIP,
        _carg(x1, 0xFFF, 12) | _carg(y1, 0xFFF, 0),
        _carg(tile, 0x7, 24) | _carg(x0, 0xFFF, 12) | _carg(y0, 0xFFF, 0),
        _carg(s, 0xFFFF, 16) | _carg(t, 0xFFFF, 0),
        _carg(dsdx, 0xFFFF, 16) | _carg(dtdy, 0xFFFF, 0));
}

#define rdpq_texture_rectangle_flip(tile, x0, y0, x1, y1, s, t, dsdx, dtdy) ({ \
    rdpq_texture_rectangle_flip_fx((tile), (x0)*4, (y0)*4, (x1)*4, (y1)*4, (s)*32, (t)*32, (dsdx)*1024, (dtdy)*1024); \
})

/**
 * @brief Low level function to sync the RDP pipeline
 */
inline void rdpq_sync_pipe(void)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_SYNC_PIPE, 0, 0);
}

/**
 * @brief Low level function to sync RDP tile operations
 */
inline void rdpq_sync_tile(void)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_SYNC_TILE, 0, 0);
}

/**
 * @brief Schedule a RDP SYNC_FULL command and register a callback when it is done.
 * 
 * This function schedules a RDP SYNC_FULL command into the RSP queue. This
 * command basically forces the RDP to finish drawing everything that has been
 * sent to it before it, and then generate an interrupt when it is done.
 * 
 * This is normally useful at the end of the frame. For instance, it is used
 * internally by #rdp_detach_display to make sure RDP is finished drawing on
 * the target display before detaching it.
 * 
 * The function can be passed an optional callback that will be called
 * when the RDP interrupt triggers. This can be useful to perform some operations
 * asynchronously.
 * 
 * @param      callback  A callback to invoke under interrupt when the RDP
 *                       is finished drawing, or NULL if no callback is necessary.
 * @param      arg       Opaque argument that will be passed to the callback.
 * 
 * @see #rspq_wait
 * @see #rdpq_fence
 * 
 */
inline void rdpq_sync_full(void (*callback)(void*), void* arg)
{
    extern void __rdpq_sync_full(uint32_t, uint32_t);
    __rdpq_sync_full(PhysicalAddr(callback), (uint32_t)arg);
}

/**
 * @brief Low level function to synchronize RDP texture load operations
 */
inline void rdpq_sync_load(void)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_SYNC_LOAD, 0, 0);
}

/**
 * @brief Low level function to set the green and blue components of the chroma key
 */
inline void rdpq_set_key_gb(uint16_t wg, uint8_t wb, uint8_t cg, uint16_t sg, uint8_t cb, uint8_t sb)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_SET_KEY_GB,
        _carg(wg, 0xFFF, 12) | _carg(wb, 0xFFF, 0),
        _carg(cg, 0xFF, 24) | _carg(sg, 0xFF, 16) | _carg(cb, 0xFF, 8) | _carg(sb, 0xFF, 0));
}

/**
 * @brief Low level function to set the red component of the chroma key
 */
inline void rdpq_set_key_r(uint16_t wr, uint8_t cr, uint8_t sr)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_SET_KEY_R, 0, _carg(wr, 0xFFF, 16) | _carg(cr, 0xFF, 8) | _carg(sr, 0xFF, 0));
}

/**
 * @brief Low level functions to set the matrix coefficients for texture format conversion
 */
inline void rdpq_set_convert(uint16_t k0, uint16_t k1, uint16_t k2, uint16_t k3, uint16_t k4, uint16_t k5)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_SET_CONVERT,
        _carg(k0, 0x1FF, 13) | _carg(k1, 0x1FF, 4) | (((uint32_t)(k2 & 0x1FF)) >> 5),
        _carg(k2, 0x1F, 27) | _carg(k3, 0x1FF, 18) | _carg(k4, 0x1FF, 9) | _carg(k5, 0x1FF, 0));
}

/**
 * @brief Low level function to set the scissoring region
 */
#define rdpq_set_scissor(x0, y0, x1, y1) ({ \
    extern void __rdpq_set_scissor(uint32_t, uint32_t); \
    uint32_t x0fx = (x0)*4; \
    uint32_t y0fx = (y0)*4; \
    uint32_t x1fx = (x1)*4; \
    uint32_t y1fx = (y1)*4; \
    assertf(x0fx <= x1fx, "x0 must not be greater than x1!"); \
    assertf(y0fx <= y1fx, "y0 must not be greater than y1!"); \
    assertf(x1fx > 0, "x1 must not be zero!"); \
    assertf(y1fx > 0, "y1 must not be zero!"); \
    __rdpq_set_scissor( \
        _carg(x0fx, 0xFFF, 12) | _carg(y0fx, 0xFFF, 0), \
        _carg(x1fx, 0xFFF, 12) | _carg(y1fx, 0xFFF, 0)); \
})

/**
 * @brief Low level function to set the primitive depth
 */
inline void rdpq_set_prim_depth(uint16_t primitive_z, uint16_t primitive_delta_z)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_SET_PRIM_DEPTH, 0, _carg(primitive_z, 0xFFFF, 16) | _carg(primitive_delta_z, 0xFFFF, 0));
}

/**
 * @brief Low level function to set the "other modes"
 */
inline void rdpq_set_other_modes(uint64_t modes)
{
    extern void __rdpq_set_other_modes(uint32_t, uint32_t);
    __rdpq_set_other_modes(
        (modes >> 32) & 0x00FFFFFF,
        modes & 0xFFFFFFFF);
}

/**
 * @brief Low level function to load a texture palette into TMEM
 */
inline void rdpq_load_tlut(uint8_t tile, uint8_t lowidx, uint8_t highidx)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_LOAD_TLUT, 
        _carg(lowidx, 0xFF, 14), 
        _carg(tile, 0x7, 24) | _carg(highidx, 0xFF, 14));
}

/**
 * @brief Low level function to set the size of a tile descriptor
 */
inline void rdpq_set_tile_size_fx(uint8_t tile, uint16_t s0, uint16_t t0, uint16_t s1, uint16_t t1)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_SET_TILE_SIZE,
        _carg(s0, 0xFFF, 12) | _carg(t0, 0xFFF, 0),
        _carg(tile, 0x7, 24) | _carg(s1-4, 0xFFF, 12) | _carg(t1-4, 0xFFF, 0));
}

#define rdpq_set_tile_size(tile, s0, t0, s1, t1) ({ \
    rdpq_set_tile_size_fx((tile), (s0)*4, (t0)*4, (s1)*4, (t1)*4); \
})

/**
 * @brief Low level function to load a texture image into TMEM in a single memory transfer
 */
inline void rdpq_load_block_fx(uint8_t tile, uint16_t s0, uint16_t t0, uint16_t s1, uint16_t dxt)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_LOAD_BLOCK,
        _carg(s0, 0xFFC, 12) | _carg(t0, 0xFFC, 0),
        _carg(tile, 0x7, 24) | _carg(s1-4, 0xFFC, 12) | _carg(dxt, 0xFFF, 0));
}

// TODO: perform ceiling function on dxt
#define rdpq_load_block(tile, s0, t0, s1, dxt) ({ \
    rdpq_load_block_fx((tile), (s0)*4, (t0)*4, (s1)*4, (dxt)*2048); \
})

/**
 * @brief Low level function to load a texture image into TMEM
 */
inline void rdpq_load_tile_fx(uint8_t tile, uint16_t s0, uint16_t t0, uint16_t s1, uint16_t t1)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_LOAD_TILE,
        _carg(s0, 0xFFF, 12) | _carg(t0, 0xFFF, 0),
        _carg(tile, 0x7, 24) | _carg(s1-4, 0xFFF, 12) | _carg(t1-4, 0xFFF, 0));
}

#define rdpq_load_tile(tile, s0, t0, s1, t1) ({ \
    rdpq_load_tile_fx((tile), (s0)*4, (t0)*4, (s1)*4, (t1)*4); \
})

/**
 * @brief Low level function to set the properties of a tile descriptor
 */
inline void rdpq_set_tile(uint8_t format, uint8_t size, uint16_t line, uint16_t tmem_addr, 
    uint8_t tile, uint8_t palette, uint8_t ct, uint8_t mt, uint8_t mask_t, uint8_t shift_t,
    uint8_t cs, uint8_t ms, uint8_t mask_s, uint8_t shift_s)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_SET_TILE,
        _carg(format, 0x7, 21) | _carg(size, 0x3, 19) | _carg(line, 0x1FF, 9) | _carg(tmem_addr, 0x1FF, 0),
        _carg(tile, 0x7, 24) | _carg(palette, 0xF, 20) | _carg(ct, 0x1, 19) | _carg(mt, 0x1, 18) | _carg(mask_t, 0xF, 14) | 
        _carg(shift_t, 0xF, 10) | _carg(cs, 0x1, 9) | _carg(ms, 0x1, 8) | _carg(mask_s, 0xF, 4) | _carg(shift_s, 0xF, 0));
}

/**
 * @brief Low level function to render a rectangle filled with a solid color
 */
inline void rdpq_fill_rectangle_fx(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_FILL_RECTANGLE,
        _carg(x1, 0xFFF, 12) | _carg(y1, 0xFFF, 0),
        _carg(x0, 0xFFF, 12) | _carg(y0, 0xFFF, 0));
}

#define rdpq_fill_rectangle(x0, y0, x1, y1) ({ \
    rdpq_fill_rectangle_fx((x0)*4, (y0)*4, (x1)*4, (y1)*4); \
})

/**
 * @brief Low level function to set the fill color
 */
inline void rdpq_set_fill_color(color_t color) {
    extern void __rdpq_set_fill_color(uint32_t);
    __rdpq_set_fill_color((color.r << 24) | (color.g << 16) | (color.b << 8) | (color.a << 0));
}

inline void rdpq_set_fill_color_pattern(color_t color1, color_t color2) {
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    uint32_t c1 = (((int)color1.r >> 3) << 11) | (((int)color1.g >> 3) << 6) | (((int)color1.b >> 3) << 1) | (color1.a >> 7);
    uint32_t c2 = (((int)color2.r >> 3) << 11) | (((int)color2.g >> 3) << 6) | (((int)color2.b >> 3) << 1) | (color2.a >> 7);
    __rdpq_write8(RDPQ_CMD_SET_FILL_COLOR, 0, (c1 << 16) | c2);
}

/**
 * @brief Low level function to set the fog color
 */
inline void rdpq_set_fog_color(color_t color)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_SET_FOG_COLOR, 0, color_to_packed32(color));
}

/**
 * @brief Low level function to set the blend color
 */
inline void rdpq_set_blend_color(color_t color)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_SET_BLEND_COLOR, 0, color_to_packed32(color));
}

/**
 * @brief Low level function to set the primitive color
 */
inline void rdpq_set_prim_color(color_t color)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_SET_PRIM_COLOR, 0, color_to_packed32(color));
}

/**
 * @brief Low level function to set the environment color
 */
inline void rdpq_set_env_color(color_t color)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_SET_ENV_COLOR, 0, color_to_packed32(color));
}

/**
 * @brief Low level function to set the color combiner parameters
 */
inline void rdpq_set_combine_mode(uint64_t flags)
{
    extern void __rdpq_write8(uint32_t, uint32_t, uint32_t);
    __rdpq_write8(RDPQ_CMD_SET_COMBINE_MODE,
        (flags >> 32) & 0x00FFFFFF,
        flags & 0xFFFFFFFF);
}

/**
 * @brief Low level function to set RDRAM pointer to a texture image
 */
inline void rdpq_set_texture_image_lookup(uint8_t index, uint32_t offset, uint8_t format, uint8_t size, uint16_t width)
{
    assertf(index <= 15, "Lookup address index out of range [0,15]: %d", index);
    extern void __rdpq_set_fixup_image(uint32_t, uint32_t, uint32_t, uint32_t);
    __rdpq_set_fixup_image(RDPQ_CMD_SET_TEXTURE_IMAGE, RDPQ_CMD_SET_TEXTURE_IMAGE_FIX,
        _carg(format, 0x7, 21) | _carg(size, 0x3, 19) | _carg(width-1, 0x3FF, 0),
        _carg(index, 0xF, 28) | (offset & 0x3FFFFFF));
}

inline void rdpq_set_texture_image(void* dram_ptr, uint8_t format, uint8_t size, uint16_t width)
{
    rdpq_set_texture_image_lookup(0, PhysicalAddr(dram_ptr), format, size, width);
}

/**
 * @brief Low level function to set RDRAM pointer to the depth buffer
 */
inline void rdpq_set_z_image_lookup(uint8_t index, uint32_t offset)
{
    assertf(index <= 15, "Lookup address index out of range [0,15]: %d", index);
    extern void __rdpq_set_fixup_image(uint32_t, uint32_t, uint32_t, uint32_t);
    __rdpq_set_fixup_image(RDPQ_CMD_SET_Z_IMAGE, RDPQ_CMD_SET_Z_IMAGE_FIX,
        0, 
        _carg(index, 0xF, 28) | (offset & 0x3FFFFFF));
}

inline void rdpq_set_z_image(void* dram_ptr)
{
    rdpq_set_z_image_lookup(0, PhysicalAddr(dram_ptr));
}

/**
 * @brief Low level function to set RDRAM pointer to the color buffer
 */
inline void rdpq_set_color_image_lookup(uint8_t index, uint32_t offset, uint32_t format, uint32_t size, uint32_t width, uint32_t height, uint32_t stride)
{
    uint32_t pixel_size = size == RDP_TILE_SIZE_16BIT ? 2 : 4;
    assertf(stride % pixel_size == 0, "Stride must be a multiple of the pixel size!");
    assertf(index <= 15, "Lookup address index out of range [0,15]: %d", index);

    extern void __rdpq_set_color_image(uint32_t, uint32_t);
    __rdpq_set_color_image(
        _carg(format, 0x7, 21) | _carg(size, 0x3, 19) | _carg((stride/pixel_size)-1, 0x3FF, 0),
        _carg(index, 0xF, 28) | (offset & 0x3FFFFFF));
    rdpq_set_scissor(0, 0, width, height);
}

inline void rdpq_set_color_image(void* dram_ptr, uint32_t format, uint32_t size, uint32_t width, uint32_t height, uint32_t stride)
{
    assertf(((uint32_t)dram_ptr & 63) == 0, "buffer pointer is not aligned to 64 bytes, so it cannot use as RDP color image.\nAllocate it with memalign(64, len) or malloc_uncached_align(64, len)");
    rdpq_set_color_image_lookup(0, PhysicalAddr(dram_ptr), format, size, width, height, stride);
}

inline void rdpq_set_cycle_mode(uint32_t cycle_mode)
{
    uint32_t mask = ~(0x3<<20);
    assertf((mask & cycle_mode) == 0, "Invalid cycle mode: %lx", cycle_mode);

    extern void __rdpq_modify_other_modes(uint32_t, uint32_t, uint32_t);
    __rdpq_modify_other_modes(0, mask, cycle_mode);
}

#ifdef __cplusplus
}
#endif

#endif
