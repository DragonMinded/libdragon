/**
 * @file rdpq_rect.h
 * @brief RDP Command queue
 * @ingroup rdpq
 */

#ifndef LIBDRAGON_RDPQ_RECT_H
#define LIBDRAGON_RDPQ_RECT_H

#include "rdpq.h"

#ifdef __cplusplus
extern "C" {
#endif

// Internal functions used for inline optimizations. Not part of the public API.
// Do not call directly
/// @cond
#define __UNLIKELY(x) __builtin_expect(!!(x), 0)

__attribute__((always_inline))
inline void __rdpq_fill_rectangle_inline(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    if (__UNLIKELY(x0 < 0)) x0 = 0;
    if (__UNLIKELY(y0 < 0)) y0 = 0;
    if (__UNLIKELY(x1 > 0xFFF)) x1 = 0xFFF;
    if (__UNLIKELY(y1 > 0xFFF)) y1 = 0xFFF;
    if (__UNLIKELY(x0+3 >= x1 || y0+3 >= y1)) return;

    extern void __rdpq_fill_rectangle(uint32_t w0, uint32_t w1);
    __rdpq_fill_rectangle(
        _carg(x1, 0xFFF, 12) | _carg(y1, 0xFFF, 0),
        _carg(x0, 0xFFF, 12) | _carg(y0, 0xFFF, 0));
}

__attribute__((always_inline))
inline void __rdpq_texture_rectangle_inline(rdpq_tile_t tile, 
    int32_t x0, int32_t y0, int32_t x1, int32_t y1,
    int32_t s0, int32_t t0)
{
    if (__UNLIKELY(x1 == x0 || y1 == y0)) return;
    int32_t dsdx = 1<<10, dtdy = 1<<10;

    if (__UNLIKELY(x0 > x1)) {
        int32_t tmp = x0; x0 = x1; x1 = tmp;
        x0 += 4; x1 += 4;
        s0 += (x1 - x0 - 4) << 3;
        dsdx = -dsdx;
    }
    if (__UNLIKELY(y0 > y1)) {
        int32_t tmp = y0; y0 = y1; y1 = tmp;
        y0 += 4; y1 += 4;
        t0 += (y1 - y0 - 4) << 3;
        dtdy = -dtdy;
    }
    if (__UNLIKELY(x0 < 0)) {
        s0 -= (x0 * dsdx) >> 7;
        x0 = 0;
        if (__UNLIKELY(x0 >= x1)) return;
    }
    if (__UNLIKELY(y0 < 0)) {
        t0 -= (y0 * dtdy) >> 7;
        y0 = 0;
        if (__UNLIKELY(y0 >= y1)) return;
    }
    if (__UNLIKELY(x1 > 1024*4-1)) {
        x1 = 1024*4-1;
        if (__UNLIKELY(x0 >= x1)) return;
    }
    if (__UNLIKELY(y1 > 1024*4-1)) {
        y1 = 1024*4-1;
        if (__UNLIKELY(y0 >= y1)) return;
    }

    extern void __rdpq_texture_rectangle(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3);
    __rdpq_texture_rectangle(
        _carg(x1, 0xFFF, 12) | _carg(y1, 0xFFF, 0),
        _carg(tile, 0x7, 24) | _carg(x0, 0xFFF, 12) | _carg(y0, 0xFFF, 0),
        _carg(s0, 0xFFFF, 16) | _carg(t0, 0xFFFF, 0),
        _carg(dsdx, 0xFFFF, 16) | _carg(dtdy, 0xFFFF, 0));
}

__attribute__((always_inline))
inline void __rdpq_texture_rectangle_scaled_inline(rdpq_tile_t tile, 
    int32_t x0, int32_t y0, int32_t x1, int32_t y1,
    int32_t s0, int32_t t0, int32_t s1, int32_t t1)
{
    if (__UNLIKELY(x1 == x0 || y1 == y0)) return;
    int32_t dsdx = ((s1 - s0) << 7) / (x1 - x0), dtdy = ((t1 - t0) << 7) / (y1 - y0);

    if (__UNLIKELY(x0 > x1)) {
        int32_t tmp = x0; x0 = x1; x1 = tmp;
        s0 += ((x0 - x1 - 4) * dsdx) >> 7;
    }
    if (__UNLIKELY(y0 > y1)) {
        int32_t tmp = y0; y0 = y1; y1 = tmp;
        t0 += ((y0 - y1 - 4) * dtdy) >> 7;
    }
    if (__UNLIKELY(x0 < 0)) {
        s0 -= (x0 * dsdx) >> 7;
        x0 = 0;
        if (__UNLIKELY(x0 >= x1)) return;
    }
    if (__UNLIKELY(y0 < 0)) {
        t0 -= (y0 * dtdy) >> 7;
        y0 = 0;
        if (__UNLIKELY(y0 >= y1)) return;
    }
    if (__UNLIKELY(x1 > 1024*4-1)) {
        s1 -= ((x1 - 1024*4-1) * dsdx) >> 7;
        x1 = 1024*4-1;
        if (__UNLIKELY(x0 >= x1)) return;
    }
    if (__UNLIKELY(y1 > 1024*4-1)) {
        t1 -= ((y1 - 1024*4-1) * dtdy) >> 7;
        y1 = 1024*4-1;
        if (__UNLIKELY(y0 >= y1)) return;
    }

    extern void __rdpq_texture_rectangle(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3);
    __rdpq_texture_rectangle(
        _carg(x1, 0xFFF, 12) | _carg(y1, 0xFFF, 0),
        _carg(tile, 0x7, 24) | _carg(x0, 0xFFF, 12) | _carg(y0, 0xFFF, 0),
        _carg(s0, 0xFFFF, 16) | _carg(t0, 0xFFFF, 0),
        _carg(dsdx, 0xFFFF, 16) | _carg(dtdy, 0xFFFF, 0));
}

inline void __rdpq_fill_rectangle_fx(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    if (__builtin_constant_p(x0) && __builtin_constant_p(y0) && __builtin_constant_p(x1) && __builtin_constant_p(y1)) {
        __rdpq_fill_rectangle_inline(x0, y0, x1, y1);
    } else {
        extern void __rdpq_fill_rectangle_offline(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
        __rdpq_fill_rectangle_offline(x0, y0, x1, y1);
    }
}

inline void __rdpq_texture_rectangle_fx(rdpq_tile_t tile, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t s, int32_t t)
{
    if (__builtin_constant_p(x0) && __builtin_constant_p(y0) && __builtin_constant_p(x1) && __builtin_constant_p(y1)) {
        __rdpq_texture_rectangle_inline(tile, x0, y0, x1, y1, s, t);
    } else {
        extern void __rdpq_texture_rectangle_offline(rdpq_tile_t tile, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t s0, int32_t t0);
        __rdpq_texture_rectangle_offline(tile, x0, y0, x1, y1, s, t);
    }
}

inline void __rdpq_texture_rectangle_scaled_fx(rdpq_tile_t tile, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t s0, int32_t t0, int32_t s1, int32_t t1)
{
    if (__builtin_constant_p(x0) && __builtin_constant_p(y0) && __builtin_constant_p(x1) && __builtin_constant_p(y1)) {
        __rdpq_texture_rectangle_scaled_inline(tile, x0, y0, x1, y1, s0, t0, s1, t1);
    } else {
        extern void __rdpq_texture_rectangle_scaled_offline(rdpq_tile_t tile, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t s0, int32_t t0, int32_t s1, int32_t t1);
        __rdpq_texture_rectangle_scaled_offline(tile, x0, y0, x1, y1, s0, t0, s1, t1);
    }
}

inline void __rdpq_texture_rectangle_raw_fx(rdpq_tile_t tile, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t s0, uint16_t t0, int16_t dsdx, int16_t dtdy)
{
     extern void __rdpq_texture_rectangle(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3);
    __rdpq_texture_rectangle(
        _carg(x1, 0xFFF, 12) | _carg(y1, 0xFFF, 0),
        _carg(tile, 0x7, 24) | _carg(x0, 0xFFF, 12) | _carg(y0, 0xFFF, 0),
        _carg(s0, 0xFFFF, 16) | _carg(t0, 0xFFFF, 0),
        _carg(dsdx, 0xFFFF, 16) | _carg(dtdy, 0xFFFF, 0));   
}

inline void __rdpq_texture_rectangle_flip_raw_fx(rdpq_tile_t tile, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, int16_t s, int16_t t, int16_t dsdy, int16_t dtdx)
{
    extern void __rdpq_write16_syncuse(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

    // Note that this command is broken in copy mode, so it doesn't
    // require any fixup. The RSP will trigger an assert if this
    // is called in such a mode.
    __rdpq_write16_syncuse(RDPQ_CMD_TEXTURE_RECTANGLE_FLIP,
        _carg(x1, 0xFFF, 12) | _carg(y1, 0xFFF, 0),
        _carg(tile, 0x7, 24) | _carg(x0, 0xFFF, 12) | _carg(y0, 0xFFF, 0),
        _carg(s, 0xFFFF, 16) | _carg(t, 0xFFFF, 0),
        _carg(dsdy, 0xFFFF, 16) | _carg(dtdx, 0xFFFF, 0),
        AUTOSYNC_PIPE | AUTOSYNC_TILE(tile) | AUTOSYNC_TMEM(0));
}
#undef __UNLIKELY
/// @endcond

/**
 * @name Standard rectangle functions
 * 
 * These functions can be used to directly draw filled and/or textured rectangles
 * on the screen. While a rectangle can always be drawn via two triangles,
 * directly invoking the rectangle functions when possible is more efficient on
 * both the CPU and the RDP.
 * 
 * The functions are defined as macros so that they can efficiently accept either
 * integers or floating point values. Usage of fractional values is required for
 * subpixel precision.
 * 
 * \{
 */

/**
 * @brief Draw a filled rectangle (RDP command: FILL_RECTANGLE)
 * 
 * This command is used to render a rectangle filled with a solid color.
 * The color must have been configured via #rdpq_set_fill_color, and the
 * render mode should be set to FILL via #rdpq_set_mode_fill.
 * 
 * The rectangle must be defined using exclusive bottom-right bounds, so for
 * instance `rdpq_fill_rectangle(10,10,30,30)` will draw a square of exactly
 * 20x20 pixels.
 * 
 * Fractional values can be used, and will create a semi-transparent edge. For
 * instance, `rdpq_fill_rectangle(9.75, 9.75, 30.25, 30.25)` will create a 22x22 pixel
 * square, with the most external pixel rows and columns having a alpha of 25%.
 * This obviously makes more sense in RGBA32 mode where there is enough alpha
 * bitdepth to appreciate the result. Make sure to configure the blender via
 * #rdpq_mode_blender (part of the mode API) or via the lower-level #rdpq_set_other_modes_raw,
 * to decide the blending formula.
 * 
 * @code{.c}
 *      // Fill the screen with red color.
 *      rdpq_set_mode_fill(RGBA32(255, 0, 0, 0));
 *      rdpq_fill_rectangle(0, 0, 320, 240);
 * @endcode
 * 
 * 
 * @param[x0]   x0      Top-left X coordinate of the rectangle (integer or float)
 * @param[y0]   y0      Top-left Y coordinate of the rectangle (integer or float)
 * @param[x1]   x1      Bottom-right *exclusive* X coordinate of the rectangle (integer or float)
 * @param[y1]   y1      Bottom-right *exclusive* Y coordinate of the rectangle (integer or float)
 * 
 * @see rdpq_set_fill_color
 * @see rdpq_set_fill_color_stripes
 * 
 * @hideinitializer
 */
#define rdpq_fill_rectangle(x0, y0, x1, y1) ({ \
    __rdpq_fill_rectangle_fx((x0)*4, (y0)*4, (x1)*4, (y1)*4); \
})

/**
 * @brief Draw a textured rectangle (RDP command: TEXTURE_RECTANGLE)
 * 
 * This function enqueues a RDP TEXTURE_RECTANGLE command, that allows to draw a
 * textured rectangle onto the framebuffer (similar to a sprite).
 * 
 * The texture must have been already loaded into TMEM via #rdpq_load_tile or
 * #rdpq_load_block, and a tile descriptor referring to it must be passed to this
 * function.
 * 
 * Input X and Y coordinates are automatically clipped to the screen boundaries (and
 * then scissoring also takes effect), so there is no specific range
 * limit to them. On the contrary, S and T coordinates have a specific range
 * (-1024..1024).
 * 
 * When x0 > x1 or y0 > y1, the rectangle is drawn flipped (mirrored) on either
 * axis (or both, which basically rotates it by 180° instead). 
 * 
 * Before calling this function, make sure to also configure an appropriate
 * render mode. It is possible to use the fast copy mode (#rdpq_set_mode_copy) with
 * this function, assuming that advanced blending or color combiner capabilities
 * are not needed. The copy mode can in fact just blit the pixels from the texture
 * unmodified, applying only a per-pixel rejection to mask out transparent pixels
 * (via alpha compare). See #rdpq_set_mode_copy for more information.
 * 
 * Alternatively, it is possible to use this command also in standard render mode
 * (#rdpq_set_mode_standard), with all the per-pixel blending / combining features.
 * 
 * Normally, rectangles are drawn without any respect for the z-buffer (if any is
 * configured). The only option here is to provide a single Z value valid for the
 * whole rectangle by using #rdpq_mode_zoverride in the mode API
 * (or manually calling #rdpq_set_prim_depth_raw). In fact, it is not possible
 * to specify a per-vertex Z value.
 * 
 * Similarly, it is not possible to specify a per-vertex color/shade value, but
 * instead it is possible to setup a combiner that applies a fixed color to the
 * pixels of the rectangle (eg: #RDPQ_COMBINER_TEX_FLAT).
 * 
 * If you need a full Z-buffering or shading support, an alternative is to
 * call #rdpq_triangle instead, and thus draw the rectangles as two triangles.
 * This will however incur in more overhead on the CPU to setup the primitives.
 * 
 * @param[in]   tile    Tile descriptor referring to the texture in TMEM to use for drawing
 * @param[in]   x0      Top-left X coordinate of the rectangle
 * @param[in]   y0      Top-left Y coordinate of the rectangle
 * @param[in]   x1      Bottom-right *exclusive* X coordinate of the rectangle
 * @param[in]   y1      Bottom-right *exclusive* Y coordinate of the rectangle
 * @param[in]   s       S coordinate of the texture at the top-left corner (range: -1024..1024)
 * @param[in]   t       T coordinate of the texture at the top-left corner (range: -1024..1024)
 * 
 * @hideinitializer
 */
// NOTE: we use a macro here to support both integer and float inputs without ever forcing
// a useless additional conversion.
#define rdpq_texture_rectangle(tile, x0, y0, x1, y1, s, t) \
    __rdpq_texture_rectangle_fx((tile), (x0)*4, (y0)*4, (x1)*4, (y1)*4, (s)*32, (t)*32)

/**
 * @brief Draw a textured rectangle with scaling (RDP command: TEXTURE_RECTANGLE)
 * 
 * This function is similar to #rdpq_texture_rectangle but allows the rectangle
 * to be scaled horizontally and/or vertically, by specifying both the source
 * rectangle in the texture, and the rectangle on the screen.
 * 
 * Refer to #rdpq_texture_rectangle for more details on how this command works.
 * 
 * @param[in]   tile    Tile descriptor referring to the texture in TMEM to use for drawing
 * @param[in]   x0      Top-left X coordinate of the rectangle
 * @param[in]   y0      Top-left Y coordinate of the rectangle
 * @param[in]   x1      Bottom-right *exclusive* X coordinate of the rectangle
 * @param[in]   y1      Bottom-right *exclusive* Y coordinate of the rectangle
 * @param[in]   s0      S coordinate of the texture at the top-left corner (range: -1024..1024)
 * @param[in]   t0      T coordinate of the texture at the top-left corner (range: -1024..1024)
 * @param[in]   s1      S coordinate of the texture at the bottom-right corner (exclusive) (range: -1024..1024)
 * @param[in]   t1      T coordinate of the texture at the bottom-right corner (exclusive) (range: -1024..1024)
 * 
 * @hideinitializer
 */
#define rdpq_texture_rectangle_scaled(tile, x0, y0, x1, y1, s0, t0, s1, t1) \
    __rdpq_texture_rectangle_scaled_fx((tile), (x0)*4, (y0)*4, (x1)*4, (y1)*4, (s0)*32, (t0)*32, (s1)*32, (t1)*32)


/**
 * \}
 * 
 * @name Raw rectangle functions
 * 
 * These functions are similar to the above ones, but they closely match the hardware
 * commands to be sent to RDP. They are exposed for completeness, but most users
 * should use the standard ones, as they provide a easier and more consistent API.
 * 
 * The main differences are that these functions accept only positive integers (so clipping
 * on negative numbers should be performed by the caller, if needed), and the textured
 * functions need the per-pixel horizontal and vertical increments.
 * 
 * \{
 */
 
/**
 * @brief Draw a textured rectangle with scaling -- raw version (RDP command: TEXTURE_RECTANGLE)
 * 
 * This function is similar to #rdpq_texture_rectangle but it does not perform any
 * preprocessing on the input coordinates. Most users should use #rdpq_texture_rectangle
 * or #rdpq_texture_rectangle_scaled instead.
 * 
 * Refer to #rdpq_texture_rectangle for more details on how this command works.
 * 
 * @param tile      Tile descriptor referring to the texture in TMEM to use for drawing
 * @param x0        Top-left X coordinate of the rectangle (range: 0..1024)
 * @param y0        Top-left Y coordinate of the rectangle (range: 0..1024)
 * @param x1        Bottom-right *exclusive* X coordinate of the rectangle (range: 0..1024)
 * @param y1        Bottom-right *exclusive* Y coordinate of the rectangle (range: 0..1024)
 * @param s0        S coordinate of the texture at the top-left corner (range: -1024..1024)
 * @param t0        T coordinate of the texture at the top-left corner (range: -1024..1024)
 * @param dsdx      Horizontal increment of S coordinate per pixel (range: -32..32)
 * @param dtdy      Vertical increment of T coordinate per pixel (range: -32..32)
 * 
 * @see #rdpq_texture_rectangle
 * @see #rdpq_texture_rectangle_scaled
 * 
 * @hideinitializer
 */
#define rdpq_texture_rectangle_raw(tile, x0, y0, x1, y1, s0, t0, dsdx, dtdy) \
    __rdpq_texture_rectangle_raw_fx(tile, (x0)*4, (y0)*4, (x1)*4, (y1)*4, (s0)*32, (t0)*32, (dsdx)*1024, (dtdy)*1024)


/**
 * @brief Draw a textured flipped rectangle (RDP command: TEXTURE_RECTANGLE_FLIP)
 * 
 * The RDP command TEXTURE_RECTANGLE_FLIP is similar to TEXTURE_RECTANGLE, but the 
 * texture S coordinate is incremented over the Y axis, while the texture T coordinate
 * is incremented over the X axis. The graphical effect is similar to a 90° degree
 * rotation plus a mirroring of the texture.
 * 
 * Notice that this command cannot work in COPY mode, so the standard render mode
 * must be activated (via #rdpq_set_mode_standard).
 * 
 * Refer to #rdpq_texture_rectangle_raw for further information.
 * 
 * @param[in]   tile    Tile descriptor referring to the texture in TMEM to use for drawing
 * @param[in]   x0      Top-left X coordinate of the rectangle
 * @param[in]   y0      Top-left Y coordinate of the rectangle
 * @param[in]   x1      Bottom-right *exclusive* X coordinate of the rectangle
 * @param[in]   y1      Bottom-right *exclusive* Y coordinate of the rectangle
 * @param[in]   s       S coordinate of the texture at the top-left corner
 * @param[in]   t       T coordinate of the texture at the top-left corner
 * @param[in]   dsdy    Signed increment of S coordinate for each vertical pixel.
 * @param[in]   dtdx    Signed increment of T coordinate for each horizontal pixel.
 * 
 * @hideinitializer
 */
#define rdpq_texture_rectangle_flip_raw(tile, x0, y0, x1, y1, s, t, dsdy, dtdx) ({ \
    __rdpq_texture_rectangle_flip_raw_fx((tile), (x0)*4, (y0)*4, (x1)*4, (y1)*4, (s)*32, (t)*32, (dsdy)*1024, (dtdx)*1024); \
})


/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif
