/**
 * @file graphics.h
 * @brief 2D Graphics
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

_Static_assert(sizeof(color_t) == 4, "invalid sizeof for color_t");

#define RGBA16(rx,gx,bx,ax) ({ \
    int rx1 = rx, gx1 = gx, bx1 = bx; \
    (color_t){.r=(rx1<<3)|(rx1>>3), .g=(gx1<<3)|(gx1>>3), .b=(bx1<<3)|(bx1>>3), .a=ax ? 0xFF : 0}; \
})

#define RGBA32(rx,gx,bx,ax) ({ \
    (color_t){.r=rx, .g=gx, .b=bx, .a=ax}; \
})

inline uint16_t color_to_packed16(color_t c) {
    return (((int)c.r >> 3) << 11) | (((int)c.g >> 3) << 6) | (((int)c.b >> 3) << 1) | (c.a >> 7);
}

inline uint32_t color_to_packed32(color_t c) {
    return *(uint32_t*)&c;
}

inline color_t color_from_packed32(uint32_t c) {
    return (color_t){ .r=(c>>24)&0xFF, .g=(c>>16)&0xFF, .b=(c>>8)&0xFF, .a=c&0xFF };
}

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

uint32_t graphics_make_color( int r, int g, int b, int a );
uint32_t graphics_convert_color( color_t color );
void graphics_draw_pixel( display_context_t disp, int x, int y, uint32_t c );
void graphics_draw_pixel_trans( display_context_t disp, int x, int y, uint32_t c );
void graphics_draw_line( display_context_t disp, int x0, int y0, int x1, int y1, uint32_t c );
void graphics_draw_line_trans( display_context_t disp, int x0, int y0, int x1, int y1, uint32_t c );
void graphics_draw_box( display_context_t disp, int x, int y, int width, int height, uint32_t color );
void graphics_draw_box_trans( display_context_t disp, int x, int y, int width, int height, uint32_t color );
void graphics_fill_screen( display_context_t disp, uint32_t c );
void graphics_set_color( uint32_t forecolor, uint32_t backcolor );
void graphics_set_default_font( void );
void graphics_set_font_sprite( sprite_t *font );
void graphics_draw_character( display_context_t disp, int x, int y, char c );
void graphics_draw_text( display_context_t disp, int x, int y, const char * const msg );
void graphics_draw_sprite( display_context_t disp, int x, int y, sprite_t *sprite );
void graphics_draw_sprite_stride( display_context_t disp, int x, int y, sprite_t *sprite, int offset );
void graphics_draw_sprite_trans( display_context_t disp, int x, int y, sprite_t *sprite );
void graphics_draw_sprite_trans_stride( display_context_t disp, int x, int y, sprite_t *sprite, int offset );

#ifdef __cplusplus
}
#endif

/** @} */ /* graphics */

#endif
