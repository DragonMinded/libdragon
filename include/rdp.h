/**
 * @file rdp.h
 * @brief Hardware Display Interface
 * @ingroup rdp
 */
#ifndef __LIBDRAGON_RDP_H
#define __LIBDRAGON_RDP_H

#include "display.h"
#include "graphics.h"

/**
 * @addtogroup rdp
 * @{
 */

/**
 * @brief Mirror settings for textures
 */
typedef enum
{
    /** @brief Disable texture mirroring */
    MIRROR_DISABLED,
    /** @brief Enable texture mirroring on x axis */
    MIRROR_X,
    /** @brief Enable texture mirroring on y axis */
    MIRROR_Y,
    /** @brief Enable texture mirroring on both x & y axis */
    MIRROR_XY
} mirror_t;

/**
 * @brief RDP sync operations
 */
typedef enum
{
    /** @brief Wait for any operation to complete before causing a DP interrupt */
    SYNC_FULL,
    /** @brief Sync the RDP pipeline */
    SYNC_PIPE,
    /** @brief Block until all texture load operations are complete */
    SYNC_LOAD,
    /** @brief Block until all tile operations are complete */
    SYNC_TILE
} sync_t;

/**
 * @brief Caching strategy for loaded textures
 */
typedef enum
{
    /** @brief Textures are assumed to be pre-flushed */
    FLUSH_STRATEGY_NONE,
    /** @brief Cache will be flushed on all incoming textures */
    FLUSH_STRATEGY_AUTOMATIC
} flush_t;

/** @} */

#ifdef __cplusplus
extern "C" {
#endif

void rdp_init( void );
void rdp_texture_rectangle(uint8_t tile, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t s, int16_t t, int16_t dsdx, int16_t dtdy);
void rdp_texture_rectangle_flip(uint8_t tile, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t s, int16_t t, int16_t dsdx, int16_t dtdy);
void rdp_sync_pipe();
void rdp_sync_tile();
void rdp_sync_full();
void rdp_set_key_gb(uint16_t wg, uint8_t wb, uint8_t cg, uint16_t sg, uint8_t cb, uint8_t sb);
void rdp_set_key_r(uint16_t wr, uint8_t cr, uint8_t sr);
void rdp_set_convert(uint16_t k0, uint16_t k1, uint16_t k2, uint16_t k3, uint16_t k4, uint16_t k5);
void rdp_set_scissor(int16_t xh, int16_t yh, int16_t xl, int16_t yl);
void rdp_set_prim_depth(uint16_t primitive_z, uint16_t primitive_delta_z);
void rdp_set_other_modes(uint64_t modes);
void rdp_load_tlut(uint8_t tile, uint8_t lowidx, uint8_t highidx);
void rdp_sync_load();
void rdp_set_tile_size(uint8_t tile, int16_t s0, int16_t t0, int16_t s1, int16_t t1);
void rdp_load_block(uint8_t tile, uint16_t s0, uint16_t t0, uint16_t s1, uint16_t dxt);
void rdp_load_tile(uint8_t tile, int16_t s0, int16_t t0, int16_t s1, int16_t t1);
void rdp_set_tile(uint8_t format, uint8_t size, uint16_t line, uint16_t tmem_addr,
                  uint8_t tile, uint8_t palette, uint8_t ct, uint8_t mt, uint8_t mask_t, uint8_t shift_t,
                  uint8_t cs, uint8_t ms, uint8_t mask_s, uint8_t shift_s);
void rdp_fill_rectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void rdp_set_fill_color(uint32_t color);
void rdp_set_fog_color(uint32_t color);
void rdp_set_blend_color(uint32_t color);
void rdp_set_prim_color(uint32_t color);
void rdp_set_env_color(uint32_t color);
void rdp_set_combine_mode(uint64_t flags);
void rdp_set_texture_image(uint32_t dram_addr, uint8_t format, uint8_t size, uint16_t width);
void rdp_set_z_image(uint32_t dram_addr);
void rdp_set_color_image(uint32_t dram_addr, uint32_t format, uint32_t size, uint32_t width);
void rdp_attach_display( display_context_t disp );
void rdp_detach_display( void );
void rdp_detach_display_auto_show();
void rdp_sync( sync_t sync );
void rdp_set_clipping( uint32_t tx, uint32_t ty, uint32_t bx, uint32_t by );
void rdp_set_default_clipping( void );
void rdp_enable_primitive_fill( void );
void rdp_enable_blend_fill( void );
void rdp_enable_texture_copy( void );
uint32_t rdp_load_texture( uint32_t texslot, uint32_t texloc, mirror_t mirror, sprite_t *sprite );
uint32_t rdp_load_texture_stride( uint32_t texslot, uint32_t texloc, mirror_t mirror, sprite_t *sprite, int offset );
void rdp_draw_textured_rectangle( uint32_t texslot, int tx, int ty, int bx, int by,  mirror_t mirror );
void rdp_draw_textured_rectangle_scaled( uint32_t texslot, int tx, int ty, int bx, int by, double x_scale, double y_scale,  mirror_t mirror );
void rdp_draw_sprite( uint32_t texslot, int x, int y ,  mirror_t mirror);
void rdp_draw_sprite_scaled( uint32_t texslot, int x, int y, double x_scale, double y_scale,  mirror_t mirror);
void rdp_set_primitive_color( uint32_t color );
void rdp_draw_filled_rectangle( int tx, int ty, int bx, int by );
void rdp_draw_filled_triangle( float x1, float y1, float x2, float y2, float x3, float y3 );
void rdp_set_texture_flush( flush_t flush );
void rdp_close( void );

#ifdef __cplusplus
}
#endif

#endif
