/**
 * @file rdp.c
 * @brief Hardware Display Interface
 * @ingroup rdp
 */
#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include "libdragon.h"

/**
 * @defgroup rdp Hardware Display Interface
 * @ingroup display
 * @brief Interface to the hardware sprite/triangle rasterizer (RDP).
 *
 * The hardware display interface sets up and talks with the RDP in order to render
 * hardware sprites, triangles and rectangles.  The RDP is a very low level rasterizer
 * and needs data in a very specific format.  The hardware display interface handles
 * this by building commands to be sent to the RDP.
 *
 * Before attempting to draw anything using the RDP, the hardware display interface
 * should be initialized with #rdp_init.  After the RDP is no longer needed, be sure
 * to free all resources using #rdp_close.
 *
 * Code wishing to use the hardware rasterizer should first acquire a display context
 * using #display_lock.  Once a display context has been acquired, the RDP can be
 * attached to the display context with #rdp_attach_display.  Once the display has been
 * attached, the RDP can be used to draw sprites, rectangles and textured/untextured
 * triangles to the display context.  Note that some functions require additional setup,
 * so read the descriptions for each function before use.  After code has finished
 * rendering hardware assisted graphics to the display context, the RDP can be detached
 * from the context using #rdp_detach_display.  After calling thie function, it is safe
 * to immediately display the rendered graphics to the screen using #display_show, or
 * additional software graphics manipulation can take place using functions from the
 * @ref graphics.
 *
 * Careful use of the #rdp_sync operation is required for proper rasterization.  Before
 * performing settings changes such as clipping changes or setting up texture or solid
 * fill modes, code should perform a #SYNC_PIPE.  A #SYNC_PIPE should be performed again
 * before any new texture load.  This is to ensure that the last texture operation is
 * completed before attempting to change texture memory.  Careful execution of texture
 * operations can allow code to skip some sync operations.  Be careful with excessive
 * sync operations as it can stall the pipeline and cause triangles/rectangles to be
 * drawn on the next display context instead of the current.
 *
 * #rdp_detach_display will automatically perform a #SYNC_FULL to ensure that everything
 * has been completed in the RDP.  This call generates an interrupt when complete which
 * signals the main thread that it is safe to detach.  Consequently, interrupts must be
 * enabled for proper operation.  This also means that code should under normal circumstances
 * never use #SYNC_FULL.
 * @{
 */

/**
 * @brief Grab the texture buffer given a display context
 *
 * @param[in] x
 *            The display context returned from #display_lock
 *
 * @return A pointer to the drawing surface for that display context.
 */
#define __get_buffer( x ) __safe_buffer[(x)-1]

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

extern uint32_t __bitdepth;
extern uint32_t __width;
extern uint32_t __height;
extern void *__safe_buffer[];


/** @brief The current cache flushing strategy */
static flush_t flush_strategy = FLUSH_STRATEGY_AUTOMATIC;

/** @brief Interrupt wait flag */
static volatile uint32_t wait_intr = 0;

/** @brief Array of cached textures in RDP TMEM indexed by the RDP texture slot */
static sprite_cache cache[8];

/** @brief Length of the queue that is used to keep track of display contexts that should be auto shown upon RDP interrupt. */
#define AUTO_SHOW_QUEUE_LENGTH 4
static display_context_t auto_show_queue[AUTO_SHOW_QUEUE_LENGTH];
static int auto_show_ridx = 0;
static int auto_show_widx = 0;
static display_context_t current_display = 0;

/**
 * @brief RDP interrupt handler
 *
 * This interrupt is called when a Sync Full operation has completed and it is safe to
 * use the output buffer with software
 */
static void __rdp_interrupt()
{
    /* Flag that the interrupt happened */
    wait_intr++;

    if (auto_show_widx != auto_show_ridx)
    {
        display_show(auto_show_queue[auto_show_ridx]);
        auto_show_ridx = (auto_show_ridx + 1) % AUTO_SHOW_QUEUE_LENGTH;
    }
}

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

void rdp_init( void )
{
    /* Default to flushing automatically */
    flush_strategy = FLUSH_STRATEGY_AUTOMATIC;

    /* Set up interrupt for SYNC_FULL */
    register_DP_handler( __rdp_interrupt );
    set_DP_interrupt( 1 );

    ugfx_init();
}

void rdp_close( void )
{
    set_DP_interrupt( 0 );
    unregister_DP_handler( __rdp_interrupt );
}

void rdp_texture_rectangle(uint8_t tile, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t s, int16_t t, int16_t ds, int16_t dt)
{
    uint64_t w0 = RdpTextureRectangle1FX(tile, x0, y0, x1, y1);
    uint64_t w1 = RdpTextureRectangle2FX(s, t, ds, dt);
    RSPQ_WRITE_BEGIN(ptr, w0>>56);
    *ptr++ = (w0 >> 32) & 0x00FFFFFF;
    *ptr++ = w0 & 0xFFFFFFFF;
    *ptr++ = w1 >> 32;
    *ptr++ = w1 & 0xFFFFFFFF;
    RSPQ_WRITE_END(ptr);
}

void rdp_texture_rectangle_flip(uint8_t tile, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t s, int16_t t, int16_t ds, int16_t dt)
{
    uint64_t w0 = RdpTextureRectangleFlip1FX(tile, x0, y0, x1, y1);
    uint64_t w1 = RdpTextureRectangle2FX(s, t, ds, dt);
    RSPQ_WRITE_BEGIN(ptr, w0>>56);
    *ptr++ = w0 >> 32;
    *ptr++ = w0 & 0xFFFFFFFF;
    *ptr++ = w1 >> 32;
    *ptr++ = w1 & 0xFFFFFFFF;
    RSPQ_WRITE_END(ptr);
}

void rdp_sync_pipe()
{
    rspq_queue_u64(RdpSyncPipe());
}

void rdp_sync_tile()
{
    rspq_queue_u64(RdpSyncTile());
}

void rdp_sync_full()
{
    rspq_queue_u64(RdpSyncFull());
    rspq_flush();
}

void rdp_set_key_gb(uint16_t wg, uint8_t wb, uint8_t cg, uint16_t sg, uint8_t cb, uint8_t sb)
{
    rspq_queue_u64(RdpSetKeyGb(wg, wb, cg, sg, cb, sb));
}

void rdp_set_key_r(uint16_t wr, uint8_t cr, uint8_t sr)
{
    rspq_queue_u64(RdpSetKeyR(wr, cr, sr));
}

void rdp_set_convert(uint16_t k0, uint16_t k1, uint16_t k2, uint16_t k3, uint16_t k4, uint16_t k5)
{
    rspq_queue_u64(RdpSetConvert(k0, k1, k2, k3, k4, k5));
}

void rdp_set_scissor(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    rspq_queue_u64(RdpSetClippingFX(x0, y0, x1, y1));
}

void rdp_set_prim_depth(uint16_t primitive_z, uint16_t primitive_delta_z)
{
    rspq_queue_u64(RdpSetPrimDepth(primitive_z, primitive_delta_z));
}

void rdp_set_other_modes(uint64_t modes)
{
    rspq_queue_u64(RdpSetOtherModes(modes));
}

void rdp_load_tlut(uint8_t tile, uint8_t lowidx, uint8_t highidx)
{
    rspq_queue_u64(RdpLoadTlut(tile, lowidx, highidx));
}

void rdp_sync_load()
{
    rspq_queue_u64(RdpSyncLoad());
}

void rdp_set_tile_size(uint8_t tile, int16_t s0, int16_t t0, int16_t s1, int16_t t1)
{
    rspq_queue_u64(RdpSetTileSizeFX(tile, s0, t0, s1, t1));
}

void rdp_load_block(uint8_t tile, uint16_t s0, uint16_t t0, uint16_t s1, uint16_t dxt)
{
    rspq_queue_u64(RdpLoadBlock(tile, s0, t0, s1, dxt));
}

void rdp_load_tile(uint8_t tile, int16_t s0, int16_t t0, int16_t s1, int16_t t1)
{
    rspq_queue_u64(RdpLoadTileFX(tile, s0, t0, s1, t1));
}

void rdp_set_tile(uint8_t format, uint8_t size, uint16_t line, uint16_t tmem_addr,
                  uint8_t tile, uint8_t palette, uint8_t ct, uint8_t mt, uint8_t mask_t, uint8_t shift_t,
                  uint8_t cs, uint8_t ms, uint8_t mask_s, uint8_t shift_s)
{
    rspq_queue_u64(RdpSetTile(format, size, line, tmem_addr, tile, palette, ct, mt, mask_t, shift_t, cs, ms, mask_s, shift_s));
}

void rdp_fill_rectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    rspq_queue_u64(RdpFillRectangleFX(x0, y0, x1, y1));
}

void rdp_set_fill_color(uint32_t color)
{
    rspq_queue_u64(RdpSetFillColor(color));
}

void rdp_set_fog_color(uint32_t color)
{
    rspq_queue_u64(RdpSetFogColor(color));
}

void rdp_set_blend_color(uint32_t color)
{
    rspq_queue_u64(RdpSetBlendColor(color));
}

void rdp_set_prim_color(uint32_t color)
{
    rspq_queue_u64(RdpSetPrimColor(color));
}

void rdp_set_env_color(uint32_t color)
{
    rspq_queue_u64(RdpSetEnvColor(color));
}

void rdp_set_combine_mode(uint64_t flags)
{
    rspq_queue_u64(RdpSetCombine(flags));
}

void rdp_set_texture_image(uint32_t dram_addr, uint8_t format, uint8_t size, uint16_t width)
{
    rspq_queue_u64(RdpSetTexImage(format, size, dram_addr, width));
}

void rdp_set_z_image(uint32_t dram_addr)
{
    rspq_queue_u64(RdpSetDepthImage(dram_addr));
}

void rdp_set_color_image(uint32_t dram_addr, uint32_t format, uint32_t size, uint32_t width)
{
    rspq_queue_u64(RdpSetColorImage(format, size, width, dram_addr));
}

void rdp_attach_display( display_context_t disp )
{
    if( disp == 0 ) { return; }

    /* Set the rasterization buffer */
    uint32_t size = (__bitdepth == 2) ? RDP_TILE_SIZE_16BIT : RDP_TILE_SIZE_32BIT;
    rdp_set_color_image((uint32_t)__get_buffer(disp), RDP_TILE_FORMAT_RGBA, size, __width);

    current_display = disp;
}

void rdp_detach_display( void )
{
    /* Wait for SYNC_FULL to finish */
    wait_intr = 0;

    /* Force the RDP to rasterize everything and then interrupt us */
    rdp_sync_full();

    if( INTERRUPTS_ENABLED == get_interrupts_state() )
    {
        /* Only wait if interrupts are enabled */
        while( !wait_intr ) { ; }
    }

    /* Set back to zero for next detach */
    wait_intr = 0;
    current_display = 0;
}

void rdp_detach_display_auto_show()
{
    assertf(current_display != 0, "No display is currently attached!");

    uint32_t next_widx = (auto_show_widx + 1) % AUTO_SHOW_QUEUE_LENGTH;
    assertf(next_widx != auto_show_ridx, "Display auto show queue is full!");
    auto_show_queue[auto_show_widx] = current_display;
    auto_show_widx = next_widx;

    rdp_sync_full();

    current_display = 0;
}

void rdp_sync( sync_t sync )
{
    switch( sync )
    {
        case SYNC_FULL:
            rdp_sync_full();
            break;
        case SYNC_PIPE:
            rdp_sync_pipe();
            break;
        case SYNC_TILE:
            rdp_sync_tile();
            break;
        case SYNC_LOAD:
            rdp_sync_load();
            break;
    }
}

void rdp_set_clipping( uint32_t tx, uint32_t ty, uint32_t bx, uint32_t by )
{
    /* Convert pixel space to screen space in command */
    rdp_set_scissor(tx << 2, ty << 2, bx << 2, by << 2);
}

void rdp_set_default_clipping( void )
{
    /* Clip box is the whole screen */
    rdp_set_clipping( 0, 0, __width, __height );
}

void rdp_enable_primitive_fill( void )
{
    /* Set other modes to fill and other defaults */
    rdp_set_other_modes(SOM_ATOMIC_PRIM | SOM_CYCLE_FILL | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | SOM_BLENDING);
}

void rdp_enable_blend_fill( void )
{
    // TODO: Macros for blend modes (this sets blend rgb times input alpha on cycle 0)
    rdp_set_other_modes(SOM_CYCLE_1 | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | 0x80000000);
}

void rdp_enable_texture_copy( void )
{
    /* Set other modes to copy and other defaults */
    rdp_set_other_modes(SOM_ATOMIC_PRIM | SOM_CYCLE_COPY | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | SOM_BLENDING | SOM_ALPHA_COMPARE);
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
        data_cache_hit_writeback_invalidate( sprite->data, sprite->width * sprite->height * sprite->bitdepth );
    }

    /* Point the RDP at the actual sprite data */
    rdp_set_texture_image((uint32_t)sprite->data, RDP_TILE_FORMAT_RGBA, (sprite->bitdepth == 2) ? RDP_TILE_SIZE_16BIT : RDP_TILE_SIZE_32BIT, sprite->width);

    /* Figure out the s,t coordinates of the sprite we are copying out of */
    int twidth = sh - sl + 1;
    int theight = th - tl + 1;

    /* Figure out the power of two this sprite fits into */
    uint32_t real_width  = __rdp_round_to_power( twidth );
    uint32_t real_height = __rdp_round_to_power( theight );
    uint32_t wbits = __rdp_log2( real_width );
    uint32_t hbits = __rdp_log2( real_height );

    /* Because we are dividing by 8, we want to round up if we have a remainder */
    int round_amount = (real_width % 8) ? 1 : 0;

    /* Instruct the RDP to copy the sprite data out */
    rdp_set_tile(
        RDP_TILE_FORMAT_RGBA, 
        (sprite->bitdepth == 2) ? RDP_TILE_SIZE_16BIT : RDP_TILE_SIZE_32BIT, 
        (((real_width / 8) + round_amount) * sprite->bitdepth) & 0x1FF,
        (texloc / 8) & 0x1FF,
        texslot & 0x7,
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
    rdp_load_tile(0, (sl << 2) & 0xFFF, (tl << 2) & 0xFFF, (sh << 2) & 0xFFF, (th << 2) & 0xFFF);

    /* Save sprite width and height for managed sprite commands */
    cache[texslot & 0x7].width = twidth - 1;
    cache[texslot & 0x7].height = theight - 1;
    cache[texslot & 0x7].s = sl;
    cache[texslot & 0x7].t = tl;
    cache[texslot & 0x7].real_width = real_width;
    cache[texslot & 0x7].real_height = real_height;
    
    /* Return the amount of texture memory consumed by this texture */
    return ((real_width / 8) + round_amount) * 8 * real_height * sprite->bitdepth;
}

uint32_t rdp_load_texture( uint32_t texslot, uint32_t texloc, mirror_t mirror, sprite_t *sprite )
{
    if( !sprite ) { return 0; }

    return __rdp_load_texture( texslot, texloc, mirror, sprite, 0, 0, sprite->width - 1, sprite->height - 1 );
}

uint32_t rdp_load_texture_stride( uint32_t texslot, uint32_t texloc, mirror_t mirror, sprite_t *sprite, int offset )
{
    if( !sprite ) { return 0; }

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
    int xs = (int)((1.0 / x_scale) * 4096.0);
    int ys = (int)((1.0 / y_scale) * 1024.0);

    /* Set up rectangle position in screen space */
    /* Set up texture position and scaling to 1:1 copy */
    rdp_texture_rectangle(texslot & 0x7, tx << 2, ty << 2, bx << 2, by << 2, s, t, xs & 0xFFFF, ys & 0xFFFF);
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

void rdp_set_primitive_color( uint32_t color )
{
    /* Set packed color */
    rdp_set_fill_color(color);
}

void rdp_draw_filled_rectangle( int tx, int ty, int bx, int by )
{
    if( tx < 0 ) { tx = 0; }
    if( ty < 0 ) { ty = 0; }

    rdp_fill_rectangle(tx << 2, ty << 2, bx << 2, by << 2);
}

void rdp_draw_filled_triangle( float x1, float y1, float x2, float y2, float x3, float y3 )
{
    float temp_x, temp_y;
    const float to_fixed_11_2 = 4.0f;
    const float to_fixed_16_16 = 65536.0f;
    
    /* sort vertices by Y ascending to find the major, mid and low edges */
    if( y1 > y2 ) { temp_x = x2, temp_y = y2; y2 = y1; y1 = temp_y; x2 = x1; x1 = temp_x; }
    if( y2 > y3 ) { temp_x = x3, temp_y = y3; y3 = y2; y2 = temp_y; x3 = x2; x2 = temp_x; }
    if( y1 > y2 ) { temp_x = x2, temp_y = y2; y2 = y1; y1 = temp_y; x2 = x1; x1 = temp_x; }

    /* calculate Y edge coefficients in 11.2 fixed format */
    int yh = y1 * to_fixed_11_2;
    int ym = (int)( y2 * to_fixed_11_2 ) << 16; // high word
    int yl = y3 * to_fixed_11_2;
    
    /* calculate X edge coefficients in 16.16 fixed format */
    int xh = x1 * to_fixed_16_16;
    int xm = x1 * to_fixed_16_16;
    int xl = x2 * to_fixed_16_16;
    
    /* calculate inverse slopes in 16.16 fixed format */
    int dxhdy = ( y3 == y1 ) ? 0 : ( ( x3 - x1 ) / ( y3 - y1 ) ) * to_fixed_16_16;
    int dxmdy = ( y2 == y1 ) ? 0 : ( ( x2 - x1 ) / ( y2 - y1 ) ) * to_fixed_16_16;
    int dxldy = ( y3 == y2 ) ? 0 : ( ( x3 - x2 ) / ( y3 - y2 ) ) * to_fixed_16_16;
    
    /* determine the winding of the triangle */
    int winding = ( x1 * y2 - x2 * y1 ) + ( x2 * y3 - x3 * y2 ) + ( x3 * y1 - x1 * y3 );
    int flip = ( winding > 0 ? 1 : 0 ) << 23;

    RSPQ_WRITE_BEGIN(rspq, 0x20);
    *rspq++ = flip | yl;
    *rspq++ = ym | yh;
    *rspq++ = xl;
    *rspq++ = dxldy;
    *rspq++ = xh;
    *rspq++ = dxhdy;
    *rspq++ = xm;
    *rspq++ = dxmdy;
    RSPQ_WRITE_END(rspq);
}

void rdp_set_texture_flush( flush_t flush )
{
    flush_strategy = flush;
}

/** @} */
