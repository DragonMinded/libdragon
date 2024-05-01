/**
 * @file graphics.h
 * @brief 2D Graphics
 * @ingroup graphics
 */
#ifndef __LIBDRAGON_GRAPHICS_H
#define __LIBDRAGON_GRAPHICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct surface_s surface_t;
typedef struct sprite_s sprite_t;
///@endcond

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
    return (color_t){ .r=((c>>11)&0x1F)<<3, .g=((c>>6)&0x1F)<<3, .b=((c>>1)&0x1F)<<3, .a=(c&0x1) ? 0xFF : 0 };
}

/** @brief Create a #color_t from the 32-bit packed format used by a #FMT_RGBA32 surface (RGBA 8888) */
inline color_t color_from_packed32(uint32_t c) {
    return (color_t){ .r=(c>>24)&0xFF, .g=(c>>16)&0xFF, .b=(c>>8)&0xFF, .a=c&0xFF };
}

uint32_t graphics_make_color( int r, int g, int b, int a );
uint32_t graphics_convert_color( color_t color );
void graphics_draw_pixel( surface_t* surf, int x, int y, uint32_t c );
void graphics_draw_pixel_trans( surface_t* surf, int x, int y, uint32_t c );
void graphics_draw_line( surface_t* surf, int x0, int y0, int x1, int y1, uint32_t c );
void graphics_draw_line_trans( surface_t* surf, int x0, int y0, int x1, int y1, uint32_t c );
void graphics_draw_box( surface_t* surf, int x, int y, int width, int height, uint32_t color );
void graphics_draw_box_trans( surface_t* surf, int x, int y, int width, int height, uint32_t color );
void graphics_fill_screen( surface_t* surf, uint32_t c );
void graphics_set_color( uint32_t forecolor, uint32_t backcolor );
void graphics_set_default_font( void );
void graphics_set_font_sprite( sprite_t *font );
void graphics_draw_character( surface_t* surf, int x, int y, char c );
void graphics_draw_text( surface_t* surf, int x, int y, const char * const msg );
void graphics_draw_sprite( surface_t* surf, int x, int y, sprite_t *sprite );
void graphics_draw_sprite_stride( surface_t* surf, int x, int y, sprite_t *sprite, int offset );
void graphics_draw_sprite_trans( surface_t* surf, int x, int y, sprite_t *sprite );
void graphics_draw_sprite_trans_stride( surface_t* surf, int x, int y, sprite_t *sprite, int offset );

#ifdef __cplusplus
}
#endif

/** @} */ /* graphics */

#endif
