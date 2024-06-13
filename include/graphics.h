/**
 * @file graphics.h
 * @brief 2D Graphics
 * @ingroup graphics
 */
#ifndef __LIBDRAGON_GRAPHICS_H
#define __LIBDRAGON_GRAPHICS_H

#include <stdint.h>

/**
 * @defgroup graphics 2D Graphics
 * @ingroup display
 * @brief Software routines for manipulating graphics in a display context.
 *
 * The graphics subsystem is responsible for software manipulation of a display
 * context as returned from the @ref display.  All of the functions use a pure
 * software drawing method and are thus much slower than hardware sprite support.
 * However, they are slightly more flexible and offer no hardware limitations
 * in terms of sprite size.
 *
 * Code wishing to draw to the screen should first acquire a display context
 * using #display_get.  Once the display context is acquired, code may draw to
 * the context using any of the graphics functions present.  Wherever practical,
 * two versions of graphics functions are available: a transparent variety and
 * a non-transparent variety.  Code that wishes to display sprites without
 * transparency can get a slight performance boost by using the non-transparent
 * variety of calls since no software alpha blending needs to occur.  Once
 * code has finished drawing to the display context, it can be displayed to the
 * screen using #display_show.
 *
 * The graphics subsystem makes use of the same contexts as the @ref rdp.  Thus,
 * with careful coding, both hardware and software routines can be used to draw
 * to the display context with no ill effects.  The colors returned by 
 * #graphics_make_color and #graphics_convert_color are also compatible with both
 * hardware and software graphics routines.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct surface_s surface_t;
typedef struct sprite_s sprite_t;
///@endcond

/** @brief Generic color structure */
typedef struct __attribute__((packed))
{
    /** @brief Red component */
    uint8_t r;
    /** @brief Green component */
    uint8_t g;
    /** @brief Blue component */
    uint8_t b;
    /** @brief Alpha component */
    uint8_t a;
} color_t;

#ifndef __cplusplus
_Static_assert(sizeof(color_t) == 4, "invalid sizeof for color_t");
#endif

/** @brief Create a #color_t from the R,G,B,A components in the RGBA16 range (that is: RGB in 0-31, A in 0-1) */
#define RGBA16(rx,gx,bx,ax) ({ \
    int rx1 = rx, gx1 = gx, bx1 = bx; \
    (color_t){.r=(rx1<<3)|(rx1>>3), .g=(gx1<<3)|(gx1>>3), .b=(bx1<<3)|(bx1>>3), .a=ax ? 0xFF : 0}; \
})

/** @brief Create a #color_t from the R,G,B,A components in the RGBA32 range (0-255). */
#define RGBA32(rx,gx,bx,ax) ({ \
    (color_t){.r=rx, .g=gx, .b=bx, .a=ax}; \
})

/** @brief Convert a #color_t to the 16-bit packed format used by a #FMT_RGBA16 surface (RGBA 5551) */
inline uint16_t color_to_packed16(color_t c) {
    return (((int)c.r >> 3) << 11) | (((int)c.g >> 3) << 6) | (((int)c.b >> 3) << 1) | (c.a >> 7);
}

/** @brief Convert a #color_t to the 32-bit packed format used by a #FMT_RGBA32 surface (RGBA 8888) */
inline uint32_t color_to_packed32(color_t c) {
    return *(uint32_t*)&c;
}
/** @brief Create a #color_t from the 16-bit packed format used by a #FMT_RGBA16 surface (RGBA 5551) */
inline color_t color_from_packed16(uint16_t c) {
    return (color_t){ .r=(uint8_t)(((c>>11)&0x1F)<<3), .g=(uint8_t)(((c>>6)&0x1F)<<3), .b=(uint8_t)(((c>>1)&0x1F)<<3), .a=(uint8_t)((c&0x1) ? 0xFF : 0) };
}

/** @brief Create a #color_t from the 32-bit packed format used by a #FMT_RGBA32 surface (RGBA 8888) */
inline color_t color_from_packed32(uint32_t c) {
    return (color_t){ .r=(uint8_t)(c>>24), .g=(uint8_t)(c>>16), .b=(uint8_t)(c>>8), .a=(uint8_t)c };
}

/**
 * @brief Return a packed 32-bit representation of an RGBA color
 *
 * This is exactly the same as calling `graphics_convert_color(RGBA32(r,g,b,a))`.
 * Refer to #graphics_convert_color for more information.
 *
 * @deprecated By switching to the rdpq API, this function should not be required
 * anymore. Use #RGBA32 or #RGBA16 instead. Please avoid using it in new code if possible.
 *
 * @param[in] r
 *            8-bit red value
 * @param[in] g
 *            8-bit green value
 * @param[in] b
 *            8-bit blue value
 * @param[in] a
 *            8-bit alpha value.  Note that 255 is opaque and 0 is transparent
 *
 * @return a 32-bit representation of the color suitable for blitting in software or hardware
 * 
 * @see #graphics_convert_color
 * 
 */
uint32_t graphics_make_color( int r, int g, int b, int a );

/**
 * @brief Convert a color structure to a 32-bit representation of an RGBA color
 * 
 * This function is similar to #color_to_packed16 and #color_to_packed32, but
 * automatically picks the version matching with the current display configuration.
 * Notice that this might be wrong if you are drawing to an arbitrary surface rather
 * than a framebuffer.
 *
 * @note In 16 bpp mode, this function will return a packed 16-bit color
 * in BOTH the lower 16 bits and the upper 16 bits. In general, this is not necessary.
 * However, for drawing with the old deprecated RDP API (in particular,
 * rdp_set_primitive_color), this is still required.
 * 
 * @deprecated By switching to the rdpq API, this function should not be required
 * anymore. Please avoid using it in new code if possible.
 * 
 * @param[in] color
 *            A color structure representing an RGBA color
 * 
 * @return a 32-bit representation of the color suitable for blitting in software or hardware
 */
uint32_t graphics_convert_color( color_t color );

/**
 * @brief Draw a pixel to a given display context
 *
 * @note This function does not support transparency for speed purposes.  To draw
 * a transparent or translucent pixel, use #graphics_draw_pixel_trans.
 *
 * @param[in] surf
 *            The currently active display context.
 * @param[in] x
 *            The x coordinate of the pixel.
 * @param[in] y
 *            The y coordinate of the pixel.
 * @param[in] color
 *            The 32-bit RGBA color to draw to the screen.  Use #graphics_convert_color
 *            or #graphics_make_color to generate this value.
 */
void graphics_draw_pixel( surface_t* surf, int x, int y, uint32_t color );

/**
 * @brief Draw a pixel to a given display context with alpha support
 *
 * @note This function is much slower than #graphics_draw_pixel for 32-bit
 * pixels due to the need to sample the current pixel to do software alpha-blending.
 *
 * @param[in] surf
 *            The currently active display context.
 * @param[in] x
 *            The x coordinate of the pixel.
 * @param[in] y
 *            The y coordinate of the pixel.
 * @param[in] color
 *            The 32-bit RGBA color to draw to the screen.  Use #graphics_convert_color
 *            or #graphics_make_color to generate this value.
 */
void graphics_draw_pixel_trans( surface_t* surf, int x, int y, uint32_t color );

/**
 * @brief Draw a line to a given display context
 * 
 * @note This function does not support transparency for speed purposes.  To draw
 * a transparent or translucent line, use #graphics_draw_line_trans.
 *
 * @param[in] surf
 *            The currently active display context.
 * @param[in] x0
 *            The x coordinate of the start of the line.
 * @param[in] y0
 *            The y coordinate of the start of the line. 
 * @param[in] x1
 *            The x coordinate of the end of the line.
 * @param[in] y1
 *            The y coordinate of the end of the line. 
 * @param[in] color
 *            The 32-bit RGBA color to draw to the screen.  Use #graphics_convert_color
 *            or #graphics_make_color to generate this value.
 */
void graphics_draw_line( surface_t* surf, int x0, int y0, int x1, int y1, uint32_t color );

/**
 * @brief Draw a line to a given display context with alpha support
 *
 * @note This function is much slower than #graphics_draw_line for 32-bit
 * buffers due to the need to sample the current pixel to do software alpha-blending.
 *
 * @param[in] surf
 *            The currently active display context.
 * @param[in] x0
 *            The x coordinate of the start of the line.
 * @param[in] y0
 *            The y coordinate of the start of the line. 
 * @param[in] x1
 *            The x coordinate of the end of the line.
 * @param[in] y1
 *            The y coordinate of the end of the line. 
 * @param[in] color
 *            The 32-bit RGBA color to draw to the screen.  Use #graphics_convert_color
 *            or #graphics_make_color to generate this value.
 */
void graphics_draw_line_trans( surface_t* surf, int x0, int y0, int x1, int y1, uint32_t color );

/**
 * @brief Draw a filled rectangle to a display context
 *
 * @note This function does not support transparency for speed purposes.  To draw
 * a transparent or translucent box, use #graphics_draw_box_trans.
 *
 * @param[in] surf
 *            The currently active display context.
 * @param[in] x
 *            The x coordinate of the top left of the box.
 * @param[in] y
 *            The y coordinate of the top left of the box.
 * @param[in] width
 *            The width of the box in pixels.
 * @param[in] height
 *            The height of the box in pixels.
 * @param[in] color
 *            The 32-bit RGBA color to draw to the screen.  Use #graphics_convert_color
 *            or #graphics_make_color to generate this value.
 */
void graphics_draw_box( surface_t* surf, int x, int y, int width, int height, uint32_t color );

/**
 * @brief Draw a filled rectangle to a display context
 *
 * @note This function is much slower than #graphics_draw_box for 32-bit
 * buffers due to the need to sample the current pixel to do software alpha-blending.
 *
 * @param[in] surf
 *            The currently active display context.
 * @param[in] x
 *            The x coordinate of the top left of the box.
 * @param[in] y
 *            The y coordinate of the top left of the box.
 * @param[in] width
 *            The width of the box in pixels.
 * @param[in] height
 *            The height of the box in pixels.
 * @param[in] color
 *            The 32-bit RGBA color to draw to the screen.  Use #graphics_convert_color
 *            or #graphics_make_color to generate this value.
 */
void graphics_draw_box_trans( surface_t* surf, int x, int y, int width, int height, uint32_t color );

/**
 * @brief Fill the entire screen with a particular color
 *
 * @note Since this function is designed for blanking the screen, alpha values for
 * colors are ignored.
 *
 * @param[in] surf
 *            The currently active display context.
 * @param[in] c
 *            The 32-bit RGBA color to draw to the screen.  Use #graphics_convert_color
 *            or #graphics_make_color to generate this value.
 */
void graphics_fill_screen( surface_t* surf, uint32_t c );

/**
 * @brief Set the current forecolor and backcolor for text operations
 *
 * @param[in] forecolor
 *            32-bit RGBA color to use as the text color.  Use #graphics_convert_color 
 *            or #graphics_make_color to generate this value.
 * @param[in] backcolor
 *             32-bit RGBA color to use as the background color for text.  Use 
 *             #graphics_convert_color or #graphics_make_color to generate this value.
 *             Note that if the color given is transparent, text can be written over
 *             other graphics without background colors showing.
 */
void graphics_set_color( uint32_t forecolor, uint32_t backcolor );

/**
 * @brief Set the font to the default.
 */
void graphics_set_default_font( void );

/**
 * @brief Set the current font. Should be set before using any of the draw function.
 * 
 * The sprite font should be imported using hslices/vslices according to the amount of characters it has.
 * The amount of hslices vs vslices does not matter for this, but it should include the whole ASCII
 * range that you will want to use, including characters from the 0 to 32 range. Normally the sprite should have
 * 127 slices to cover the normal ASCII range.
 * 
 * During rendering, the slice used will be the same number as the char (eg.: character 'A' will use slice 65).
 * 
 * You can see an example of a sprite font (that has the default font double sized) under examples/customfont.
 *
 * @param[in] font
 *        Sprite font to be used.
 */
void graphics_set_font_sprite( sprite_t *font );

/**
 * @brief Draw a character to the screen using the built-in font
 *
 * Draw a character from the built-in font to the screen.  This function does not support alpha blending, 
 * only binary transparency.  If the background color is fully transparent, the font is drawn with no
 * background.  Otherwise, the font is drawn on a fully colored background.  The foreground and background
 * can be set using #graphics_set_color.
 *
 * @param[in] surf
 *            The currently active display context.
 * @param[in] x
 *            The X coordinate to place the top left pixel of the character drawn.
 * @param[in] y
 *            The Y coordinate to place the top left pixel of the character drawn.
 * @param[in] ch
 *            The ASCII character to draw to the screen.
 */
void graphics_draw_character( surface_t* surf, int x, int y, char ch );

/**
 * @brief Draw a null terminated string to a display context
 *
 * Draw a string to the screen, following a few simple rules.  Standard ASCII is supported, as well
 * as \\r, \\n, space and tab.  \\r and \\n will both cause the next character to be rendered one line
 * lower and at the x coordinate specified in the parameters.  The tab character inserts five spaces.
 *
 * This function does not support alpha blending, only binary transparency.  If the background color is 
 * fully transparent, the font is drawn with no background.  Otherwise, the font is drawn on a fully 
 * colored background.  The foreground and background can be set using #graphics_set_color.
 *
 * @param[in] surf
 *            The currently active display context.
 * @param[in] x
 *            The X coordinate to place the top left pixel of the character drawn.
 * @param[in] y
 *            The Y coordinate to place the top left pixel of the character drawn.
 * @param[in] msg
 *            The ASCII null terminated string to draw to the screen.
 */
void graphics_draw_text( surface_t* surf, int x, int y, const char * const msg );

/**
 * @brief Draw a sprite to a display context
 *
 * Given a sprite structure, this function will draw a sprite to the display context
 * with clipping support.
 *
 * @note This function does not support alpha blending for speed purposes.  For
 * alpha blending support, please see #graphics_draw_sprite_trans
 *
 * @param[in] surf
 *            The currently active display context.
 * @param[in] x
 *            The X coordinate to place the top left pixel of the sprite.  This can
 *            be negative if the sprite is clipped horizontally.
 * @param[in] y
 *            The Y coordinate to place the top left pixel of the sprite.  This can
 *            be negative if the sprite is clipped vertically.
 * @param[in] sprite
 *            Pointer to a sprite structure to display to the screen.
 */
void graphics_draw_sprite( surface_t* surf, int x, int y, sprite_t *sprite );

/**
 * @brief Draw a sprite from a spritemap to a display context
 *
 * Given a sprite structure, this function will draw a sprite out of a larger spritemap
 * to the display context with clipping support.  This function is useful for software
 * tilemapping.  If a sprite was generated as a spritemap (it has more than one horizontal 
 * or vertical slice), this function can display a slice of the sprite as a standalone sprite.
 * 
 * Given a sprite with 3 horizontal slices and 2 vertical slices, the offsets would be as follows:
 *
 * <pre>
 * *---*---*---*
 * | 0 | 1 | 2 |
 * *---*---*---*
 * | 3 | 4 | 5 |
 * *---*---*---*
 * </pre>
 *
 * @note This function does not support alpha blending for speed purposes.  For
 * alpha blending support, please see #graphics_draw_sprite_trans_stride
 *
 * @param[in] surf
 *            The currently active display context.
 * @param[in] x
 *            The X coordinate to place the top left pixel of the sprite.  This can
 *            be negative if the sprite is clipped horizontally.
 * @param[in] y
 *            The Y coordinate to place the top left pixel of the sprite.  This can
 *            be negative if the sprite is clipped vertically.
 * @param[in] sprite
 *            Pointer to a sprite structure to display to the screen.
 * @param[in] offset
 *            Offset of the sprite to display out of the spritemap.  The offset is counted
 *            starting from 0.  The top left sprite in the map is 0, the next one to the right 
 *            is 1, and so on.
 */
void graphics_draw_sprite_stride( surface_t* surf, int x, int y, sprite_t *sprite, int offset );

/**
 * @brief Draw a sprite to a display context with alpha transparency
 *
 * Given a sprite structure, this function will draw a sprite to the display context
 * with clipping support.
 *
 * @note This function supports alpha blending and is much slower for 32-bit sprites. 
 * If you do not need alpha blending support, please see #graphics_draw_sprite.
 *
 * @param[in] surf
 *            The currently active display context.
 * @param[in] x
 *            The X coordinate to place the top left pixel of the sprite.  This can
 *            be negative if the sprite is clipped horizontally.
 * @param[in] y
 *            The Y coordinate to place the top left pixel of the sprite.  This can
 *            be negative if the sprite is clipped vertically.
 * @param[in] sprite
 *            Pointer to a sprite structure to display to the screen.
 */
void graphics_draw_sprite_trans( surface_t* surf, int x, int y, sprite_t *sprite );

/**
 * @brief Draw a sprite from a spritemap to a display context
 *
 * Given a sprite structure, this function will draw a sprite out of a larger spritemap
 * to the display context with clipping support.  This function is useful for software
 * tilemapping.  If a sprite was generated as a spritemap (it has more than one horizontal 
 * or vertical slice), this function can display a slice of the sprite as a standalone sprite.
 *
 * Given a sprite with 3 horizontal slices and 2 vertical slices, the offsets would be as follows:
 *
 * <pre>
 * *---*---*---*
 * | 0 | 1 | 2 |
 * *---*---*---*
 * | 3 | 4 | 5 |
 * *---*---*---*
 * </pre>
 *
 * @note This function supports alpha blending and is much slower for 32-bit sprites. 
 * If you do not need alpha blending support, please see #graphics_draw_sprite_stride.
 *
 * @param[in] surf
 *            The currently active display context.
 * @param[in] x
 *            The X coordinate to place the top left pixel of the sprite.  This can
 *            be negative if the sprite is clipped horizontally.
 * @param[in] y
 *            The Y coordinate to place the top left pixel of the sprite.  This can
 *            be negative if the sprite is clipped vertically.
 * @param[in] sprite
 *            Pointer to a sprite structure to display to the screen.
 * @param[in] offset
 *            Offset of the sprite to display out of the spritemap.  The offset is counted
 *            starting from 0.  The top left sprite in the map is 0, the next one to the right 
 *            is 1, and so on.
 */
void graphics_draw_sprite_trans_stride( surface_t* surf, int x, int y, sprite_t *sprite, int offset );

#ifdef __cplusplus
}
#endif

/** @} */ /* graphics */

#endif
