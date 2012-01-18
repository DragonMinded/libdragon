/**
 * @file graphics.h
 * @brief 2D Graphics Subsystem
 * @ingroup graphics
 */
#ifndef __LIBDRAGON_GRAPHICS_H
#define __LIBDRAGON_GRAPHICS_H

#include "display.h"

/**
 * @addtogroup graphics
 * @{
 */

/** @brief Generic color structure */
typedef struct
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

/** @brief Sprite structure */
typedef struct
{
    /** @brief Width in pixels */
    uint16_t width;
    /** @brief Height in pixels */
    uint16_t height;
    /** 
     * @brief Bit depth expressed in bytes
     *
     * A 32 bit sprite would have a value of '4' here
     */
    uint8_t bitdepth;
    /** 
     * @brief Sprite format
     * @note Currently unused
     */
    uint8_t format;
    /** @brief Number of horizontal slices for spritemaps */
    uint8_t hslices;
    /** @brief Number of vertical slices for spritemaps */
    uint8_t vslices;

    /** @brief Start of graphics data */
    uint32_t data[0];
} sprite_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Convert a RGBA color to a valid color of the current bitdepth */
uint32_t graphics_make_color( int r, int g, int b, int a );
uint32_t graphics_convert_color( color_t color );

/* Draw a pixel to a particular display context */
void graphics_draw_pixel( display_context_t disp, int x, int y, uint32_t c );
void graphics_draw_pixel_trans( display_context_t disp, int x, int y, uint32_t c );

/* Draw a line to a particular display context */
void graphics_draw_line( display_context_t disp, int x0, int y0, int x1, int y1, uint32_t c );
void graphics_draw_line_trans( display_context_t disp, int x0, int y0, int x1, int y1, uint32_t c );

/* Draw a filled box to a particular display context */
void graphics_draw_box( display_context_t disp, int x, int y, int width, int height, uint32_t color );
void graphics_draw_box_trans( display_context_t disp, int x, int y, int width, int height, uint32_t color );

/* Fill the buffer with a color */
void graphics_fill_screen( display_context_t disp, uint32_t c );

/* Draw text to the screen */
void graphics_set_color( uint32_t forecolor, uint32_t backcolor );
void graphics_draw_character( display_context_t disp, int x, int y, uint32_t fc, uint32_t bc, char c );
void graphics_draw_text( display_context_t disp, int x, int y, const char * const msg );

/* Draw sprite to the screen */
void graphics_draw_sprite( display_context_t disp, int x, int y, sprite_t *sprite );
void graphics_draw_sprite_stride( display_context_t disp, int x, int y, sprite_t *sprite, int offset );
void graphics_draw_sprite_trans( display_context_t disp, int x, int y, sprite_t *sprite );
void graphics_draw_sprite_trans_stride( display_context_t disp, int x, int y, sprite_t *sprite, int offset );

#ifdef __cplusplus
}
#endif

/** @} */ /* graphics */

#endif
