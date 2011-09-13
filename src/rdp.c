#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include "libdragon.h"

#define __get_buffer( x ) __safe_buffer[(x)-1]

#define RINGBUFFER_SIZE  4096
#define RINGBUFFER_SLACK 1024

typedef struct
{
    uint32_t s;
    uint32_t t;
    uint32_t width;
    uint32_t height;
} sprite_cache;

extern uint32_t __bitdepth;
extern uint32_t __width;
extern uint32_t __height;
extern void *__safe_buffer[];

static uint32_t rdp_ringbuffer[RINGBUFFER_SIZE / 4];
static uint32_t rdp_start = 0;
static uint32_t rdp_end = 0;

static flush_t flush_strategy = FLUSH_STRATEGY_AUTOMATIC;

static volatile uint32_t wait_intr = 0;

static sprite_cache cache[8];

static void rdp_interrupt()
{
    /* Flag that the interrupt happened */
    wait_intr++;
}

static inline uint32_t rdp_round_to_power( uint32_t number )
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

static inline uint32_t rdp_log2( uint32_t number )
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

static inline uint32_t rdp_ringbuffer_size( void )
{
    /* Normal length */
    return rdp_end - rdp_start;
}

static void rdp_ringbuffer_queue( uint32_t data )
{
    /* Only add commands if we have room */
    if( rdp_ringbuffer_size() + sizeof(uint32_t) >= RINGBUFFER_SIZE ) { return; }

    /* Add data to queue to be sent to RDP */
    rdp_ringbuffer[rdp_end / 4] = data;
    rdp_end += 4;
}

static void rdp_ringbuffer_send( void )
{
    /* Don't send nothingness */
    if( rdp_ringbuffer_size() == 0 ) { return; }

    /* Ensure the cache is fixed up */
    data_cache_writeback_invalidate(&rdp_ringbuffer[rdp_start / 4], rdp_ringbuffer_size());

    /* Clear XBUS/Flush/Freeze */
    ((uint32_t *)0xA4100000)[3] = 0x15;

    /* Don't saturate the RDP command buffer */
    while( (((volatile uint32_t *)0xA4100000)[3] & 0x600) ) ;

    /* Send start and end of buffer location to kick off the command transfer */
    ((volatile uint32_t *)0xA4100000)[0] = ((uint32_t)rdp_ringbuffer | 0xA0000000) + rdp_start;
    ((volatile uint32_t *)0xA4100000)[1] = ((uint32_t)rdp_ringbuffer | 0xA0000000) + rdp_end;

    /* Commands themselves can't wrap around */
    if( rdp_end > (RINGBUFFER_SIZE - RINGBUFFER_SLACK) )
    {
        /* Wrap around before a command can be split */
        rdp_start = 0;
        rdp_end = 0;
    }
    else
    {
        /* Advance the start to not allow clobbering current command */
        rdp_start = rdp_end;
    }
}

void rdp_init( void )
{
    /* Default to flushing automatically */
    flush_strategy = FLUSH_STRATEGY_AUTOMATIC;

    /* Set up interrupt for SYNC_FULL */
    enable_interrupts();

    register_DP_handler( rdp_interrupt );
    set_DP_interrupt( 1 );
}

void rdp_attach_display( display_context_t disp )
{
    if( disp == 0 ) { return; }

    /* Set the rasterization buffer */
    rdp_ringbuffer_queue( 0xFF000000 | ((__bitdepth == 2) ? 0x00100000 : 0x00180000) | (__width - 1) );
    rdp_ringbuffer_queue( (uint32_t)__get_buffer( disp ) );
    rdp_ringbuffer_send();
}

void rdp_detach_display( void )
{
    /* Wait for SYNC_FULL to finish */
    wait_intr = 0;

    rdp_sync( SYNC_FULL );
    while( !wait_intr ) { ; }

    /* Set back to zero */
    wait_intr = 0;
}

void rdp_sync( sync_t sync )
{
    switch( sync )
    {
        case SYNC_FULL:
            rdp_ringbuffer_queue( 0xE9000000 );
            break;
        case SYNC_PIPE:
            rdp_ringbuffer_queue( 0xE7000000 );
            break;
        case SYNC_TILE:
            rdp_ringbuffer_queue( 0xE8000000 );
            break;
        case SYNC_LOAD:
            rdp_ringbuffer_queue( 0xE6000000 );
            break;
    }
    rdp_ringbuffer_queue( 0x00000000 );
    rdp_ringbuffer_send();
}

void rdp_set_clipping( uint32_t tx, uint32_t ty, uint32_t bx, uint32_t by )
{
    /* Convert pixel space to screen space in command */
    rdp_ringbuffer_queue( 0xED000000 | (tx << 14) | (ty << 2) );
    rdp_ringbuffer_queue( (bx << 14) | (by << 2) );
    rdp_ringbuffer_send();
}

void rdp_set_default_clipping( void )
{
    /* Clip box is the whole screen */
    rdp_set_clipping( 0, 0, __width, __height );
}

void rdp_enable_primitive_fill( void )
{
    /* Set other modes to fill and other defaults */
    rdp_ringbuffer_queue( 0xEFB000FF );
    rdp_ringbuffer_queue( 0x00004000 );
    rdp_ringbuffer_send();
}

void rdp_enable_texture_copy( void )
{
    /* Set other modes to copy and other defaults */
    rdp_ringbuffer_queue( 0xEFA000FF );
    rdp_ringbuffer_queue( 0x00004001 );
    rdp_ringbuffer_send();
}

uint32_t __rdp_load_texture( uint32_t texslot, uint32_t texloc, mirror_t mirror_enabled, sprite_t *sprite, int sl, int tl, int sh, int th )
{
    /* Invalidate data associated with sprite in cache */
    if( flush_strategy == FLUSH_STRATEGY_AUTOMATIC )
    {
        data_cache_writeback_invalidate( sprite->data, sprite->width * sprite->height * sprite->bitdepth );
    }

    /* Point the RDP at the actual sprite data */
    rdp_ringbuffer_queue( 0xFD000000 | ((sprite->bitdepth == 2) ? 0x00100000 : 0x00180000) | (sprite->width - 1) );
    rdp_ringbuffer_queue( (uint32_t)sprite->data );
    rdp_ringbuffer_send();

    /* Figure out the s,t coordinates of the sprite we are copying out of */
    int twidth = sh - sl + 1;
    int theight = th - tl + 1;

    /* Figure out the power of two this sprite fits into */
    uint32_t real_width  = rdp_round_to_power( twidth );
    uint32_t real_height = rdp_round_to_power( theight );
    uint32_t wbits = rdp_log2( real_width );
    uint32_t hbits = rdp_log2( real_height );

    /* Because we are dividing by 8, we want to round up if we have a remainder */
    int round_amount = (real_width % 8) ? 1 : 0;

    /* Instruct the RDP to copy the sprite data out */
    rdp_ringbuffer_queue( 0xF5000000 | ((sprite->bitdepth == 2) ? 0x00100000 : 0x00180000) | 
                                       (((((real_width / 8) + round_amount) * sprite->bitdepth) & 0x1FF) << 9) | ((texloc / 8) & 0x1FF) );
    rdp_ringbuffer_queue( ((texslot & 0x7) << 24) | (mirror_enabled == MIRROR_ENABLED ? 0x40100 : 0) | (hbits << 14 ) | (wbits << 4) );
    rdp_ringbuffer_send();

    /* Copying out only a chunk this time */
    rdp_ringbuffer_queue( 0xF4000000 | (((sl << 2) & 0xFFF) << 12) | ((tl << 2) & 0xFFF) );
    rdp_ringbuffer_queue( (((sh << 2) & 0xFFF) << 12) | ((th << 2) & 0xFFF) );
    rdp_ringbuffer_send();

    /* Save sprite width and height for managed sprite commands */
    cache[texslot & 0x7].width = twidth - 1;
    cache[texslot & 0x7].height = theight - 1;
    cache[texslot & 0x7].s = sl;
    cache[texslot & 0x7].t = tl;
    
    /* Return the amount of texture memory consumed by this texture */
    return ((real_width / 8) + round_amount) * 8 * real_height * sprite->bitdepth;
}

uint32_t rdp_load_texture( uint32_t texslot, uint32_t texloc, mirror_t mirror_enabled, sprite_t *sprite )
{
    if( !sprite ) { return 0; }

    return __rdp_load_texture( texslot, texloc, mirror_enabled, sprite, 0, 0, sprite->width - 1, sprite->height - 1 );
}

uint32_t rdp_load_texture_stride( uint32_t texslot, uint32_t texloc, mirror_t mirror_enabled, sprite_t *sprite, int offset )
{
    if( !sprite ) { return 0; }

    /* Figure out the s,t coordinates of the sprite we are copying out of */
    int twidth = sprite->width / sprite->hslices;
    int theight = sprite->height / sprite->vslices;

    int sl = (offset % sprite->hslices) * twidth;
    int tl = (offset / sprite->hslices) * theight;
    int sh = sl + twidth - 1;
    int th = tl + theight - 1;

    return __rdp_load_texture( texslot, texloc, mirror_enabled, sprite, sl, tl, sh, th );
}

void rdp_draw_textured_rectangle_scaled( uint32_t texslot, int tx, int ty, int bx, int by, double x_scale, double y_scale )
{
    uint16_t s = cache[texslot & 0x7].s << 5;
    uint16_t t = cache[texslot & 0x7].t << 5;

    /* Cant display < 0, so must clip size and move S,T coord accordingly */
    if( tx < 0 )
    {
        s += (int)((double)((-tx) << 5) * (1.0 / x_scale));
        tx = 0;
    }

    if( ty < 0 )
    {
        t += (int)((double)((-ty) << 5) * (1.0 / y_scale));
        ty = 0;
    }

    /* Calculate the scaling constants based on a 6.10 fixed point system */
    int xs = (int)((1.0 / x_scale) * 4096.0);
    int ys = (int)((1.0 / y_scale) * 1024.0);

    /* Set up rectangle position in screen space */
    rdp_ringbuffer_queue( 0xE4000000 | (bx << 14) | (by << 2) );
    rdp_ringbuffer_queue( ((texslot & 0x7) << 24) | (tx << 14) | (ty << 2) );

    /* Set up texture position and scaling to 1:1 copy */
    rdp_ringbuffer_queue( (s << 16) | t );
    rdp_ringbuffer_queue( (xs & 0xFFFF) << 16 | (ys & 0xFFFF) );

    /* Send command */
    rdp_ringbuffer_send();
}

void rdp_draw_textured_rectangle( uint32_t texslot, int tx, int ty, int bx, int by )
{
    /* Simple wrapper */
    rdp_draw_textured_rectangle_scaled( texslot, tx, ty, bx, by, 1.0, 1.0 );
}

void rdp_draw_sprite( uint32_t texslot, int x, int y )
{
    /* Just draw a rectangle the size of the sprite */
    rdp_draw_textured_rectangle_scaled( texslot, x, y, x + cache[texslot & 0x7].width, y + cache[texslot & 0x7].height, 1.0, 1.0 );
}

void rdp_draw_sprite_scaled( uint32_t texslot, int x, int y, double x_scale, double y_scale )
{
    /* Since we want to still view the whole sprite, we must resize the rectangle area too */
    int new_width = (int)(((double)cache[texslot & 0x7].width * x_scale) + 0.5);
    int new_height = (int)(((double)cache[texslot & 0x7].height * y_scale) + 0.5);
    
    /* Draw a rectangle the size of the new sprite */
    rdp_draw_textured_rectangle_scaled( texslot, x, y, x + new_width, y + new_height, x_scale, y_scale );
}
void rdp_set_primitive_color( uint32_t color )
{
    /* Set packed color */
    rdp_ringbuffer_queue( 0xF7000000 );
    rdp_ringbuffer_queue( color );
    rdp_ringbuffer_send();
}

void rdp_draw_filled_rectangle( int tx, int ty, int bx, int by )
{
    if( tx < 0 ) { tx = 0; }
    if( ty < 0 ) { ty = 0; }

    rdp_ringbuffer_queue( 0xF6000000 | ( bx << 14 ) | ( by << 2 ) ); 
    rdp_ringbuffer_queue( ( tx << 14 ) | ( ty << 2 ) );
    rdp_ringbuffer_send();
}

void rdp_set_texture_flush( flush_t flush )
{
    flush_strategy = flush;
}

