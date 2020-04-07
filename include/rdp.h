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
 * @brief Bitmask flags for rdp_texture_cycle()'s mode parameter.
 */
#define ATOMIC_PRIM 0x80000000000000
#define PERSP_TEX_EN 0x08000000000000
#define SAMPLE_TYPE 0x00200000000000
#define IMAGE_READ_EN  0x00000000000040

/**
 * @brief Cycle mode type
 */
typedef enum
{
    /** @brief 1 Cycle */
    _1CYCLE,
    /** @brief 2 Cycle */
    _2CYCLE
} cycle_mode_t;

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
void rdp_attach_display( display_context_t disp );
void rdp_detach_display( void );
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
void rdp_set_tri_prim_color( uint32_t color );
void rdp_set_blend_color( uint32_t color );
void rdp_texture_cycle( cycle_mode_t cycle, uint64_t mode );
void rdp_draw_filled_rectangle( int tx, int ty, int bx, int by );
void rdp_draw_filled_triangle( float x1, float y1, float x2, float y2, float x3, float y3 );
void rdp_draw_textured_triangle( uint32_t texslot, float x1, float y1, float w1, float s1, float t1, 
                                    float x2, float y2, float w2, float s2, float t2, 
                                    float x3, float y3, float w3, float s3, float t3 );
void rdp_set_texture_flush( flush_t flush );
void rdp_close( void );

#ifdef __cplusplus
}
#endif

#endif
