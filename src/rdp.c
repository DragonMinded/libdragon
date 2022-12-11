/**
 * @file rdp.c
 * @brief Deprecated RDP library
 * @ingroup rdp
 */
#include "rspq.h"
#include "rdp.h"
#include "rdpq.h"
#include "rdpq_macros.h"
#include "interrupt.h"
#include "display.h"
#include "debug.h"
#include "n64sys.h"
#include "utils.h"
#include "sprite.h"
#include <stdint.h>
#include <malloc.h>
#include <string.h>

/**
 * @defgroup rdp Deprecated RDP library
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
 * This function will take a texture from a sprite and place it into RDP TMEM at the offset and 
 * texture slot specified.  It is capable of pulling out a smaller texture from a larger sprite
 * map.
 *
 * @param[in] texslot
 *            The texture slot (0-7) to assign this texture to
 * @param[in] texloc
 *            The offset in RDP TMEM to place this texture
 * @param[in] mirror_enabled
 *            Whether to mirror this texture when displaying
 * @param[in] sprite
 *            Pointer to the sprite structure to load the texture out of
 * @param[in] sl
 *            The pixel offset S of the top left of the texture relative to sprite space
 * @param[in] tl
 *            The pixel offset T of the top left of the texture relative to sprite space
 * @param[in] sh
 *            The pixel offset S of the bottom right of the texture relative to sprite space
 * @param[in] th
 *            The pixel offset T of the bottom right of the texture relative to sprite space
 *
 * @return The amount of texture memory in bytes that was consumed by this texture.
 */
static uint32_t __rdp_load_texture( uint32_t texslot, uint32_t texloc, mirror_t mirror_enabled, sprite_t *sprite, int sl, int tl, int sh, int th )
{
    /* Invalidate data associated with sprite in cache */
    if( flush_strategy == FLUSH_STRATEGY_AUTOMATIC )
    {
        data_cache_hit_writeback_invalidate( sprite->data, sprite->width * sprite->height * TEX_FORMAT_BITDEPTH(sprite_get_format(sprite)) / 8 );
    }

    /* Point the RDP at the actual sprite data */
    rdpq_set_texture_image_raw(0, PhysicalAddr(sprite->data), sprite_get_format(sprite), sprite->width, sprite->height);

    /* Figure out the s,t coordinates of the sprite we are copying out of */
    int twidth = sh - sl + 1;
    int theight = th - tl + 1;

    /* Figure out the power of two this sprite fits into */
    uint32_t real_width  = __rdp_round_to_power( twidth );
    uint32_t real_height = __rdp_round_to_power( theight );
    uint32_t wbits = __rdp_log2( real_width );
    uint32_t hbits = __rdp_log2( real_height );

    uint32_t tmem_pitch = ROUND_UP(real_width * TEX_FORMAT_BITDEPTH(sprite_get_format(sprite)) / 8, 8);

    /* Instruct the RDP to copy the sprite data out */
    rdpq_set_tile_full(
        texslot,
        sprite_get_format(sprite),
        texloc,
        tmem_pitch,
        0, 
        0, 
        mirror_enabled != MIRROR_DISABLED ? 1 : 0,
        hbits,
        0,
        0,
        mirror_enabled != MIRROR_DISABLED ? 1 : 0,
        wbits,
        0);

    /* Copying out only a chunk this time */
    rdpq_load_tile(0, sl, tl, sh+1, th+1);

    /* Save sprite width and height for managed sprite commands */
    cache[texslot & 0x7].width = twidth - 1;
    cache[texslot & 0x7].height = theight - 1;
    cache[texslot & 0x7].s = sl;
    cache[texslot & 0x7].t = tl;
    cache[texslot & 0x7].real_width = real_width;
    cache[texslot & 0x7].real_height = real_height;
    
    /* Return the amount of texture memory consumed by this texture */
    return tmem_pitch * real_height;
}

uint32_t rdp_load_texture( uint32_t texslot, uint32_t texloc, mirror_t mirror, sprite_t *sprite )
{
    if( !sprite ) { return 0; }
    assertf(sprite_get_format(sprite) == FMT_RGBA16 || sprite_get_format(sprite) == FMT_RGBA32,
        "only sprites in FMT_RGBA16 or FMT_RGBA32 are supported");

    return __rdp_load_texture( texslot, texloc, mirror, sprite, 0, 0, sprite->width - 1, sprite->height - 1 );
}

uint32_t rdp_load_texture_stride( uint32_t texslot, uint32_t texloc, mirror_t mirror, sprite_t *sprite, int offset )
{
    if( !sprite ) { return 0; }
    assertf(sprite_get_format(sprite) == FMT_RGBA16 || sprite_get_format(sprite) == FMT_RGBA32,
        "only sprites in FMT_RGBA16 or FMT_RGBA32 are supported");

    /* Figure out the s,t coordinates of the sprite we are copying out of */
    int twidth = sprite->width / sprite->hslices;
    int theight = sprite->height / sprite->vslices;

    int sl = (offset % sprite->hslices) * twidth;
    int tl = (offset / sprite->hslices) * theight;
    int sh = sl + twidth - 1;
    int th = tl + theight - 1;

    return __rdp_load_texture( texslot, texloc, mirror, sprite, sl, tl, sh, th );
}

void rdp_draw_textured_rectangle_scaled( uint32_t texslot, int tx, int ty, int bx, int by, double x_scale, double y_scale,  mirror_t mirror)
{
    uint16_t s = cache[texslot & 0x7].s << 5;
    uint16_t t = cache[texslot & 0x7].t << 5;
    uint32_t width = cache[texslot & 0x7].width;
    uint32_t height = cache[texslot & 0x7].height;
   
    /* Cant display < 0, so must clip size and move S,T coord accordingly */
    if( tx < 0 )
    {
        if ( tx < -(width * x_scale) ) { return; }
        s += (int)(((double)((-tx) << 5)) * (1.0 / x_scale));
        tx = 0;
    }

    if( ty < 0 )
    {
        if ( ty < -(height * y_scale) ) { return; }
        t += (int)(((double)((-ty) << 5)) * (1.0 / y_scale));
        ty = 0;
    }

     // mirror horizontally or vertically
    if (mirror != MIRROR_DISABLED)
    {	
        if (mirror == MIRROR_X || mirror == MIRROR_XY)
            s += ( (width+1) + ((cache[texslot & 0x7].real_width-(width+1))<<1) ) << 5;
	
        if (mirror == MIRROR_Y || mirror == MIRROR_XY)
            t += ( (height+1) + ((cache[texslot & 0x7].real_height-(height+1))<<1) ) << 5;	
    }	

    /* Calculate the scaling constants based on a 6.10 fixed point system */
    int xs = (int)((1.0 / x_scale) * 1024.0);
    int ys = (int)((1.0 / y_scale) * 1024.0);

    /* Set up rectangle position in screen space */
    /* Set up texture position and scaling to 1:1 copy */
    rdpq_texture_rectangle_fx(texslot, tx << 2, ty << 2, (bx+1) << 2, (by+1) << 2, s, t, xs, ys);
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
    rdpq_triangle(0, 0, 0, false, -1, -1, -1, v1, v2, v3);
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
