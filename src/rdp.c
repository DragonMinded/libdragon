/**
 * @file rdp.c
 * @brief (Deprecated) Old RDP library
 * @ingroup rdp
 */
#include "rspq.h"
#include "rdp.h"
#include "rdpq.h"
#include "rdpq_tri.h"
#include "rdpq_rect.h"
#include "rdpq_macros.h"
#include "interrupt.h"
#include "display.h"
#include "debug.h"
#include "n64sys.h"
#include "utils.h"
#include "sprite.h"
#include "sprite_internal.h"
#include <stdint.h>
#include <malloc.h>
#include <string.h>

/**
 * @defgroup rdp (Deprecated) Old RDP library
 * @ingroup display
 * @brief Interface to the hardware sprite/triangle rasterizer (RDP).
 * 
 * @deprecated This module is now deprecated. Please use the new RDPQ API instead.
 * 
 * This module contains an old API to draw using the RDP. The API was not extensible
 * enough and in general did not provide a good enough foundation for RDP programming.
 * So it has been deprecated in favor of the new RDPQ API, which is much more flexible.
 * 
 * All RDP functions are now implemented as wrappers of the RDPQ API. They continue
 * to work just like before, but there will be no further work on them. Also, most of
 * them are explicitly marked as deprecated, and will generate a warning at compile
 * time. The warning suggests the alternative RDPQ API to use instead. In most cases,
 * the change should be straightforward.
 * 
 * Functions not explicitly marked as deprecated do not have a direct equivalent in
 * RDPQ API yet.
 *
 * @{
 */

/**
 * @brief Cached sprite structure
 * */
typedef struct
{
    /** @brief S location of the top left of the texture relative to the original texture */
    uint32_t s;
    /** @brief T location of the top left of the texture relative to the original texture */
    uint32_t t;
    /** @brief Width of the texture */
    uint32_t width;
    /** @brief Height of the texture */
    uint32_t height;
    /** @brief Width of the texture rounded up to next power of 2 */
    uint16_t real_width;
    /** @brief Height of the texture rounded up to next power of 2 */
    uint16_t real_height;
} sprite_cache;

/** @brief The current cache flushing strategy */
static flush_t flush_strategy = FLUSH_STRATEGY_AUTOMATIC;

/** @brief Interrupt wait flag */
static volatile uint32_t wait_intr = 0;

/** @brief Array of cached textures in RDP TMEM indexed by the RDP texture slot */
static sprite_cache cache[8];

/**
 * @brief Given a number, rount to a power of two
 *
 * @param[in] number
 *            A number that needs to be rounded
 * 
 * @return The next power of two that is greater than the number passed in.
 */
static inline uint32_t __rdp_round_to_power( uint32_t number )
{
    if( number <= 4   ) { return 4;   }
    if( number <= 8   ) { return 8;   }
    if( number <= 16  ) { return 16;  }
    if( number <= 32  ) { return 32;  }
    if( number <= 64  ) { return 64;  }
    if( number <= 128 ) { return 128; }

    /* Any thing bigger than 256 not supported */
    return 256;
}

/**
 * @brief Integer log base two of a number
 *
 * @param[in] number
 *            Number to take the log base two of
 *
 * @return Log base two of the number passed in.
 */
static inline uint32_t __rdp_log2( uint32_t number )
{
    switch( number )
    {
        case 4:
            return 2;
        case 8:
            return 3;
        case 16:
            return 4;
        case 32:
            return 5;
        case 64:
            return 6;
        case 128:
            return 7;
        default:
            /* Don't support more than 256 */
            return 8;
    }
}

/**
 * @brief Load a texture from RDRAM into RDP TMEM
 *
 * This function will take a texture from a surface and place it into RDP TMEM at the offset and 
 * texture slot specified.  It is capable of pulling out a smaller texture from a larger surface
 * map.
 *
 * @param[in] texslot
 *            The texture slot (0-7) to assign this texture to
 * @param[in] texloc
 *            The offset in RDP TMEM to place this texture
 * @param[in] mirror_enabled
 *            Whether to mirror this texture when displaying
 * @param[in] surface
 *            Pointer to the surface structure to load the texture out of
 * @param[in] sl
 *            The pixel offset S of the top left of the texture relative to surface space
 * @param[in] tl
 *            The pixel offset T of the top left of the texture relative to surface space
 * @param[in] sh
 *            The pixel offset S of the bottom right of the texture relative to surface space
 * @param[in] th
 *            The pixel offset T of the bottom right of the texture relative to surface space
 *
 * @return The amount of texture memory in bytes that was consumed by this texture.
 */
static uint32_t __rdp_load_texture( uint32_t texslot, uint32_t texloc, mirror_t mirror_enabled, surface_t *surface, int sl, int tl, int sh, int th )
{
    /* Invalidate data associated with surface in cache */
    if( flush_strategy == FLUSH_STRATEGY_AUTOMATIC )
    {
        data_cache_hit_writeback_invalidate( surface->buffer, surface->width * surface->height * TEX_FORMAT_BITDEPTH(surface_get_format(surface)) / 8 );
    }

    /* Figure out the s,t coordinates of the surface we are copying out of */
    int twidth = sh - sl;
    int theight = th - tl;

    /* Figure out the power of two this surface fits into */
    uint32_t real_width  = __rdp_round_to_power( twidth );
    uint32_t real_height = __rdp_round_to_power( theight );
    uint32_t wbits = __rdp_log2( real_width );
    uint32_t hbits = __rdp_log2( real_height );
    tex_format_t fmt = surface_get_format(surface);

    int pitch_shift = fmt == FMT_RGBA32 ? 1 : 0;
    int tmem_pitch = ROUND_UP(TEX_FORMAT_PIX2BYTES(fmt, twidth) >> pitch_shift, 8);

    /* Save sprite width and height for managed sprite commands */
    cache[texslot & 0x7].width = twidth - 1;
    cache[texslot & 0x7].height = theight - 1;
    cache[texslot & 0x7].s = sl;
    cache[texslot & 0x7].t = tl;
    cache[texslot & 0x7].real_width = real_width;
    cache[texslot & 0x7].real_height = real_height;

    /* Configure the tile */
    rdpq_set_tile(texslot, surface_get_format(surface), texloc, tmem_pitch, &(rdpq_tileparms_t){
        .s.mirror = mirror_enabled != MIRROR_DISABLED ? true : false,
        .s.mask = hbits,
        .t.mirror = mirror_enabled != MIRROR_DISABLED ? true : false,
        .t.mask = wbits,
    });

    /* Instruct the RDP to copy the sprite data out */
    rdpq_set_texture_image(surface);
    rdpq_load_tile(texslot, sl, tl, sh, th);

    /* Return the amount of texture memory consumed by this texture */
    return theight * tmem_pitch;
}

uint32_t rdp_load_texture( uint32_t texslot, uint32_t texloc, mirror_t mirror, sprite_t *sprite )
{
    if( !sprite ) { return 0; }
    __sprite_upgrade(sprite);
    assertf(sprite_get_format(sprite) == FMT_RGBA16 || sprite_get_format(sprite) == FMT_RGBA32,
        "only sprites in FMT_RGBA16 or FMT_RGBA32 are supported");

    surface_t surface = sprite_get_pixels(sprite);
    return __rdp_load_texture( texslot, texloc, mirror, &surface, 0, 0, surface.width, surface.height);
}

uint32_t rdp_load_texture_stride( uint32_t texslot, uint32_t texloc, mirror_t mirror, sprite_t *sprite, int offset )
{
    if( !sprite ) { return 0; }
    __sprite_upgrade(sprite);
    assertf(sprite_get_format(sprite) == FMT_RGBA16 || sprite_get_format(sprite) == FMT_RGBA32,
        "only sprites in FMT_RGBA16 or FMT_RGBA32 are supported");

    int ox = offset % sprite->hslices;
    int oy = offset / sprite->hslices;
    int tile_width = sprite->width / sprite->hslices;
    int tile_height = sprite->height / sprite->vslices;
    int s0 = ox * tile_width;
    int t0 = oy * tile_height;
    int s1 = s0 + tile_width;
    int t1 = t0 + tile_height;

    surface_t surface = sprite_get_pixels(sprite);
    return __rdp_load_texture( texslot, texloc, mirror, &surface, s0, t0, s1, t1);
}

void rdp_draw_textured_rectangle_scaled( uint32_t texslot, int tx, int ty, int bx, int by, double x_scale, double y_scale,  mirror_t mirror)
{
    uint16_t s = cache[texslot & 0x7].s;
    uint16_t t = cache[texslot & 0x7].t;
    uint32_t width = cache[texslot & 0x7].width;
    uint32_t height = cache[texslot & 0x7].height;
   
    if ( tx < -(width * x_scale) ) { return; }
    if ( ty < -(height * y_scale) ) { return; }

    // mirror horizontally or vertically
    if (mirror != MIRROR_DISABLED)
    {	
        if (mirror == MIRROR_X || mirror == MIRROR_XY)
            s += ( (width+1) + ((cache[texslot & 0x7].real_width-(width+1))<<1));
	
        if (mirror == MIRROR_Y || mirror == MIRROR_XY)
            t += ( (height+1) + ((cache[texslot & 0x7].real_height-(height+1))<<1));	
    }

    /* Set up rectangle position in screen space */
    /* Set up texture position and scaling to 1:1 copy */
    rdpq_texture_rectangle_scaled(texslot, tx, ty, bx+1, by+1, s, t, s + width +1, t + height +1);
}

void rdp_draw_textured_rectangle( uint32_t texslot, int tx, int ty, int bx, int by, mirror_t mirror )
{
    /* Simple wrapper */
    rdp_draw_textured_rectangle_scaled( texslot, tx, ty, bx, by, 1.0, 1.0, mirror );
}

void rdp_draw_sprite( uint32_t texslot, int x, int y, mirror_t mirror )
{
    /* Just draw a rectangle the size of the sprite */
    rdp_draw_textured_rectangle_scaled( texslot, x, y, x + cache[texslot & 0x7].width, y + cache[texslot & 0x7].height, 1.0, 1.0, mirror );
}

void rdp_draw_sprite_scaled( uint32_t texslot, int x, int y, double x_scale, double y_scale, mirror_t mirror )
{
    /* Since we want to still view the whole sprite, we must resize the rectangle area too */
    int new_width = (int)(((double)cache[texslot & 0x7].width * x_scale) + 0.5);
    int new_height = (int)(((double)cache[texslot & 0x7].height * y_scale) + 0.5);
    
    /* Draw a rectangle the size of the new sprite */
    rdp_draw_textured_rectangle_scaled( texslot, x, y, x + new_width, y + new_height, x_scale, y_scale, mirror );
}

void rdp_set_blend_color( uint32_t color )
{
    rdpq_set_blend_color(color_from_packed32(color));
}

void rdp_draw_filled_triangle( float x1, float y1, float x2, float y2, float x3, float y3 )
{
    float v1[] = {x1, y1};
    float v2[] = {x2, y2};
    float v3[] = {x3, y3};
    rdpq_triangle(&TRIFMT_FILL, v1, v2, v3);
}

void rdp_set_texture_flush( flush_t flush )
{
    flush_strategy = flush;
}

/**************************************
 *  DEPRECATED FUNCTIONS 
 **************************************/

///@cond

void rdp_init( void )
{
    /* Default to flushing automatically */
    flush_strategy = FLUSH_STRATEGY_AUTOMATIC;

    rdpq_init();
}

void rdp_close( void )
{
    rdpq_close();
}

void rdp_detach(void)
{
    // Historically, this function has behaved asynchronously when run with
    // interrupts disabled, and synchronously otherwise. Keep the behavior.
    rdpq_detach();
    if (get_interrupts_state() == INTERRUPTS_ENABLED)
        rspq_wait();
}

void rdp_sync( sync_t sync )
{
    switch( sync )
    {
        case SYNC_FULL:
            rdpq_sync_full(NULL, NULL);
            break;
        case SYNC_PIPE:
            rdpq_sync_pipe();
            break;
        case SYNC_TILE:
            rdpq_sync_tile();
            break;
        case SYNC_LOAD:
            rdpq_sync_load();
            break;
    }
}

void rdp_set_clipping( uint32_t tx, uint32_t ty, uint32_t bx, uint32_t by )
{
    /* Convert pixel space to screen space in command */
    rdpq_set_scissor(tx, ty, bx, by);
}

void rdp_set_default_clipping( void )
{
    /* Clip box is the whole screen */
    rdpq_set_scissor( 0, 0, display_get_width(), display_get_height() );
}

void rdp_draw_filled_rectangle( int tx, int ty, int bx, int by )
{
    if( tx < 0 ) { tx = 0; }
    if( ty < 0 ) { ty = 0; }

    rdpq_fill_rectangle(tx, ty, bx, by);
}

void rdp_enable_primitive_fill( void )
{
    /* Set other modes to fill and other defaults */
    rdpq_set_other_modes_raw(SOM_CYCLE_FILL | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | SOM_BLENDING);
}

void rdp_enable_blend_fill( void )
{
    // Set a "blend fill mode": we use the alpha channel coming from the combiner
    // multiplied by the BLEND register (that must be configured).
    rdpq_set_other_modes_raw(SOM_CYCLE_1 | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | 
        RDPQ_BLENDER((BLEND_RGB, IN_ALPHA, IN_RGB, INV_MUX_ALPHA)));
}

void rdp_enable_texture_copy( void )
{
    /* Set other modes to copy and other defaults */
    rdpq_set_other_modes_raw(SOM_CYCLE_COPY | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | SOM_BLENDING | SOM_ALPHACOMPARE_THRESHOLD);
}


///@endcond


/** @} */
