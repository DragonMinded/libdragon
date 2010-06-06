#ifndef __LIBDRAGON_RDP_H
#define __LIBDRAGON_RDP_H

#include "display.h"
#include "graphics.h"

typedef enum
{
    MIRROR_DISABLED,
    MIRROR_ENABLED
} mirror_t;

typedef enum
{
    SYNC_FULL,
    SYNC_PIPE,
    SYNC_LOAD,
    SYNC_TILE
} sync_t;

void rdp_init( void );
void rdp_attach_display( display_context_t disp );
void rdp_detach_display( void );
void rdp_sync( sync_t sync );
void rdp_set_clipping( uint32_t tx, uint32_t ty, uint32_t bx, uint32_t by );
void rdp_set_default_clipping( void );
void rdp_enable_primitive_fill( void );
void rdp_enable_texture_copy( void );
uint32_t rdp_load_texture( uint32_t texslot, uint32_t texloc, mirror_t mirror_enabled, sprite_t *sprite );
void rdp_draw_textured_rectangle( uint32_t texslot, int tx, int ty, int bx, int by );
void rdp_draw_textured_rectangle_scaled( uint32_t texslot, int tx, int ty, int bx, int by, double x_scale, double y_scale );
void rdp_draw_sprite( uint32_t texslot, int x, int y );
void rdp_draw_sprite_scaled( uint32_t texslot, int x, int y, double x_scale, double y_scale );
void rdp_set_primitive_color( uint32_t color );
void rdp_draw_filled_rectangle( int tx, int ty, int bx, int by );

#endif
