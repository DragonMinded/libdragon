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

/**
 * @brief Initialize the RDP system
 */
void rdp_init( void );

/**
 * @brief Attach the RDP to a display context
 *
 * This function allows the RDP to operate on display contexts fetched with #display_lock.
 * This should be performed before any other operations to ensure that the RDP has a valid
 * output buffer to operate on.
 *
 * @param[in] disp
 *            A display context as returned by #display_lock
 */
void rdp_attach_display( display_context_t disp );

/**
 * @brief Detach the RDP from a display context
 *
 * @note This function requires interrupts to be enabled to operate properly.
 *
 * This function will ensure that all hardware operations have completed on an output buffer
 * before detaching the display context.  This should be performed before displaying the finished
 * output using #display_show
 */
void rdp_detach_display( void );

/**
 * @brief Check if the RDP is currently attached to a display context
 */
bool rdp_is_display_attached();

/**
 * @brief Check if it is currently possible to attach a new display context to the RDP.
 *
 * Since #rdp_detach_display_async will not detach a display context immediately, but asynchronously,
 * it may still be attached when trying to attach the next one. Attempting to attach a display context
 * while another is already attached will lead to an error, so use this function to check whether it
 * is possible first. It will return true if no display context is currently attached, and false otherwise.
 */
#define rdp_can_attach_display() (!rdp_is_display_attached())

/**
 * @brief Detach the RDP from a display context after asynchronously waiting for the RDP interrupt
 *
 * @note This function requires interrupts to be enabled to operate properly.
 *
 * This function will ensure that all hardware operations have completed on an output buffer
 * before detaching the display context. As opposed to #rdp_detach_display, this function will
 * not block until the RDP interrupt is raised and takes a callback function instead.
 * 
 * @param[in] cb
 *            The callback that will be called when the RDP interrupt is raised.
 */
void rdp_detach_display_async(void (*cb)(display_context_t disp));

/**
 * @brief Asynchronously detach the current display from the RDP and automatically call #display_show on it
 *
 * This macro is just a shortcut for `rdp_detach_display_async(display_show)`. Use this if you
 * are done rendering with the RDP and just want to submit the attached display context to be shown without
 * any further postprocessing.
 */
#define rdp_auto_show_display() ({ \
    rdp_detach_display_async(display_show); \
})

/**
 * @brief Perform a sync operation
 *
 * Do not use excessive sync operations between commands as this can
 * cause the RDP to stall.  If the RDP stalls due to too many sync
 * operations, graphics may not be displayed until the next render
 * cycle, causing bizarre artifacts.  The rule of thumb is to only add
 * a sync operation if the data you need is not yet available in the
 * pipeline.
 *
 * @param[in] sync
 *            The sync operation to perform on the RDP
 */
void rdp_sync( sync_t sync );

/**
 * @brief Set the hardware clipping boundary
 *
 * @param[in] tx
 *            Top left X coordinate in pixels
 * @param[in] ty
 *            Top left Y coordinate in pixels
 * @param[in] bx
 *            Bottom right X coordinate in pixels
 * @param[in] by
 *            Bottom right Y coordinate in pixels
 */
void rdp_set_clipping( uint32_t tx, uint32_t ty, uint32_t bx, uint32_t by );

/**
 * @brief Set the hardware clipping boundary to the entire screen
 */
void rdp_set_default_clipping( void );

/**
 * @brief Enable display of 2D filled (untextured) rectangles
 *
 * This must be called before using #rdp_draw_filled_rectangle.
 */
void rdp_enable_primitive_fill( void );

/**
 * @brief Enable display of 2D filled (untextured) triangles
 *
 * This must be called before using #rdp_draw_filled_triangle.
 */
void rdp_enable_blend_fill( void );

/**
 * @brief Enable display of 2D sprites
 *
 * This must be called before using #rdp_draw_textured_rectangle_scaled,
 * #rdp_draw_textured_rectangle, #rdp_draw_sprite or #rdp_draw_sprite_scaled.
 */
void rdp_enable_texture_copy( void );

/**
 * @brief Load a sprite into RDP TMEM
 *
 * @param[in] texslot
 *            The RDP texture slot to load this sprite into (0-7)
 * @param[in] texloc
 *            The RDP TMEM offset to place the texture at
 * @param[in] mirror
 *            Whether the sprite should be mirrored when displaying past boundaries
 * @param[in] sprite
 *            Pointer to sprite structure to load the texture from
 *
 * @return The number of bytes consumed in RDP TMEM by loading this sprite
 */
uint32_t rdp_load_texture( uint32_t texslot, uint32_t texloc, mirror_t mirror, sprite_t *sprite );

/**
 * @brief Load part of a sprite into RDP TMEM
 *
 * Given a sprite with vertical and horizontal slices defined, this function will load the slice specified in
 * offset into texture memory.  This is usefl for treating a large sprite as a tilemap.
 *
 * Given a sprite with 3 horizontal slices and two vertical slices, the offsets are as follows:
 *
 * <pre>
 * *---*---*---*
 * | 0 | 1 | 2 |
 * *---*---*---*
 * | 3 | 4 | 5 |
 * *---*---*---*
 * </pre>
 *
 * @param[in] texslot
 *            The RDP texture slot to load this sprite into (0-7)
 * @param[in] texloc
 *            The RDP TMEM offset to place the texture at
 * @param[in] mirror
 *            Whether the sprite should be mirrored when displaying past boundaries
 * @param[in] sprite
 *            Pointer to sprite structure to load the texture from
 * @param[in] offset
 *            Offset of the particular slice to load into RDP TMEM.
 *
 * @return The number of bytes consumed in RDP TMEM by loading this sprite
 */
uint32_t rdp_load_texture_stride( uint32_t texslot, uint32_t texloc, mirror_t mirror, sprite_t *sprite, int offset );

/**
 * @brief Draw a textured rectangle
 *
 * Given an already loaded texture, this function will draw a rectangle textured with the loaded texture.
 * If the rectangle is larger than the texture, it will be tiled or mirrored based on the* mirror setting
 * given in the load texture command.
 *
 * Before using this command to draw a textured rectangle, use #rdp_enable_texture_copy to set the RDP
 * up in texture mode.
 *
 * @param[in] texslot
 *            The texture slot that the texture was previously loaded into (0-7)
 * @param[in] tx
 *            The pixel X location of the top left of the rectangle
 * @param[in] ty
 *            The pixel Y location of the top left of the rectangle
 * @param[in] bx
 *            The pixel X location of the bottom right of the rectangle
 * @param[in] by
 *            The pixel Y location of the bottom right of the rectangle
 * @param[in] mirror
 *            Whether the texture should be mirrored
 */
void rdp_draw_textured_rectangle( uint32_t texslot, int tx, int ty, int bx, int by,  mirror_t mirror );

/**
 * @brief Draw a textured rectangle with a scaled texture
 *
 * Given an already loaded texture, this function will draw a rectangle textured with the loaded texture
 * at a scale other than 1.  This allows rectangles to be drawn with stretched or squashed textures.
 * If the rectangle is larger than the texture after scaling, it will be tiled or mirrored based on the
 * mirror setting given in the load texture command.
 *
 * Before using this command to draw a textured rectangle, use #rdp_enable_texture_copy to set the RDP
 * up in texture mode.
 *
 * @param[in] texslot
 *            The texture slot that the texture was previously loaded into (0-7)
 * @param[in] tx
 *            The pixel X location of the top left of the rectangle
 * @param[in] ty
 *            The pixel Y location of the top left of the rectangle
 * @param[in] bx
 *            The pixel X location of the bottom right of the rectangle
 * @param[in] by
 *            The pixel Y location of the bottom right of the rectangle
 * @param[in] x_scale
 *            Horizontal scaling factor
 * @param[in] y_scale
 *            Vertical scaling factor
 * @param[in] mirror
 *            Whether the texture should be mirrored
 */
void rdp_draw_textured_rectangle_scaled( uint32_t texslot, int tx, int ty, int bx, int by, double x_scale, double y_scale,  mirror_t mirror );

/**
 * @brief Draw a texture to the screen as a sprite
 *
 * Given an already loaded texture, this function will draw a rectangle textured with the loaded texture.
 *
 * Before using this command to draw a textured rectangle, use #rdp_enable_texture_copy to set the RDP
 * up in texture mode.
 *
 * @param[in] texslot
 *            The texture slot that the texture was previously loaded into (0-7)
 * @param[in] x
 *            The pixel X location of the top left of the sprite
 * @param[in] y
 *            The pixel Y location of the top left of the sprite
 * @param[in] mirror
 *            Whether the texture should be mirrored
 */
void rdp_draw_sprite( uint32_t texslot, int x, int y ,  mirror_t mirror);

/**
 * @brief Draw a texture to the screen as a scaled sprite
 *
 * Given an already loaded texture, this function will draw a rectangle textured with the loaded texture.
 *
 * Before using this command to draw a textured rectangle, use #rdp_enable_texture_copy to set the RDP
 * up in texture mode.
 *
 * @param[in] texslot
 *            The texture slot that the texture was previously loaded into (0-7)
 * @param[in] x
 *            The pixel X location of the top left of the sprite
 * @param[in] y
 *            The pixel Y location of the top left of the sprite
 * @param[in] x_scale
 *            Horizontal scaling factor
 * @param[in] y_scale
 *            Vertical scaling factor
 * @param[in] mirror
 *            Whether the texture should be mirrored
 */
void rdp_draw_sprite_scaled( uint32_t texslot, int x, int y, double x_scale, double y_scale,  mirror_t mirror);

/**
 * @brief Set the primitive draw color for subsequent filled primitive operations
 *
 * This function sets the color of all #rdp_draw_filled_rectangle operations that follow.
 * Note that in 16 bpp mode, the color must be a packed color.  This means that the high
 * 16 bits and the low 16 bits must both be the same color.  Use #graphics_make_color or
 * #graphics_convert_color to generate valid colors.
 *
 * @param[in] color
 *            Color to draw primitives in
 */
void rdp_set_primitive_color( uint32_t color );

/**
 * @brief Set the blend draw color for subsequent filled primitive operations
 *
 * This function sets the color of all #rdp_draw_filled_triangle operations that follow.
 *
 * @param[in] color
 *            Color to draw primitives in
 */
void rdp_set_blend_color( uint32_t color );

/**
 * @brief Draw a filled rectangle
 *
 * Given a color set with #rdp_set_primitive_color, this will draw a filled rectangle
 * to the screen.  This is most often useful for erasing a buffer before drawing to it
 * by displaying a black rectangle the size of the screen.  This is much faster than
 * setting the buffer blank in software.  However, if you are planning on drawing to
 * the entire screen, blanking may be unnecessary.  
 *
 * Before calling this function, make sure that the RDP is set to primitive mode by
 * calling #rdp_enable_primitive_fill.
 *
 * @param[in] tx
 *            Pixel X location of the top left of the rectangle
 * @param[in] ty
 *            Pixel Y location of the top left of the rectangle
 * @param[in] bx
 *            Pixel X location of the bottom right of the rectangle
 * @param[in] by
 *            Pixel Y location of the bottom right of the rectangle
 */
void rdp_draw_filled_rectangle( int tx, int ty, int bx, int by );

/**
 * @brief Draw a filled triangle
 *
 * Given a color set with #rdp_set_blend_color, this will draw a filled triangle
 * to the screen. Vertex order is not important.
 *
 * Before calling this function, make sure that the RDP is set to blend mode by
 * calling #rdp_enable_blend_fill.
 *
 * @param[in] x1
 *            Pixel X1 location of triangle
 * @param[in] y1
 *            Pixel Y1 location of triangle
 * @param[in] x2
 *            Pixel X2 location of triangle
 * @param[in] y2
 *            Pixel Y2 location of triangle
 * @param[in] x3
 *            Pixel X3 location of triangle
 * @param[in] y3
 *            Pixel Y3 location of triangle
 */
void rdp_draw_filled_triangle( float x1, float y1, float x2, float y2, float x3, float y3 );

/**
 * @brief Set the flush strategy for texture loads
 *
 * If textures are guaranteed to be in uncached RDRAM or the cache
 * is flushed before calling load operations, the RDP can be told
 * to skip flushing the cache.  This affords a good speedup.  However,
 * if you are changing textures in memory on the fly or otherwise do
 * not want to deal with cache coherency, set the cache strategy to
 * automatic to have the RDP flush cache before texture loads.
 *
 * @param[in] flush
 *            The cache strategy, either #FLUSH_STRATEGY_NONE or
 *            #FLUSH_STRATEGY_AUTOMATIC.
 */
void rdp_set_texture_flush( flush_t flush );

/**
 * @brief Close the RDP system
 *
 * This function closes out the RDP system and cleans up any internal memory
 * allocated by #rdp_init.
 */
void rdp_close( void );

/**
 * @brief Low level function to draw a textured rectangle
 */
void rdp_texture_rectangle_raw(uint8_t tile, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t s, int16_t t, int16_t dsdx, int16_t dtdy);

/**
 * @brief Low level function to draw a textured rectangle (s and t coordinates flipped)
 */
void rdp_texture_rectangle_flip_raw(uint8_t tile, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t s, int16_t t, int16_t dsdx, int16_t dtdy);

/**
 * @brief Low level function to sync the RDP pipeline
 */
void rdp_sync_pipe_raw();

/**
 * @brief Low level function to sync RDP tile operations
 */
void rdp_sync_tile_raw();

/**
 * @brief Wait for any operation to complete before causing a DP interrupt
 */
void rdp_sync_full_raw();

/**
 * @brief Low level function to set the green and blue components of the chroma key
 */
void rdp_set_key_gb_raw(uint16_t wg, uint8_t wb, uint8_t cg, uint16_t sg, uint8_t cb, uint8_t sb);

/**
 * @brief Low level function to set the red component of the chroma key
 */
void rdp_set_key_r_raw(uint16_t wr, uint8_t cr, uint8_t sr);

/**
 * @brief Low level functions to set the matrix coefficients for texture format conversion
 */
void rdp_set_convert_raw(uint16_t k0, uint16_t k1, uint16_t k2, uint16_t k3, uint16_t k4, uint16_t k5);

/**
 * @brief Low level function to set the scissoring region
 */
void rdp_set_scissor_raw(int16_t xh, int16_t yh, int16_t xl, int16_t yl);

/**
 * @brief Low level function to set the primitive depth
 */
void rdp_set_prim_depth_raw(uint16_t primitive_z, uint16_t primitive_delta_z);

/**
 * @brief Low level function to set the "other modes"
 */
void rdp_set_other_modes_raw(uint64_t modes);

/**
 * @brief Low level function to load a texture palette into TMEM
 */
void rdp_load_tlut_raw(uint8_t tile, uint8_t lowidx, uint8_t highidx);

/**
 * @brief Low level function to synchronize RDP texture load operations
 */
void rdp_sync_load_raw();

/**
 * @brief Low level function to set the size of a tile descriptor
 */
void rdp_set_tile_size_raw(uint8_t tile, int16_t s0, int16_t t0, int16_t s1, int16_t t1);

/**
 * @brief Low level function to load a texture image into TMEM in a single memory transfer
 */
void rdp_load_block_raw(uint8_t tile, uint16_t s0, uint16_t t0, uint16_t s1, uint16_t dxt);

/**
 * @brief Low level function to load a texture image into TMEM
 */
void rdp_load_tile_raw(uint8_t tile, int16_t s0, int16_t t0, int16_t s1, int16_t t1);

/**
 * @brief Low level function to set the properties of a tile descriptor
 */
void rdp_set_tile_raw(uint8_t format, uint8_t size, uint16_t line, uint16_t tmem_addr,
                      uint8_t tile, uint8_t palette, uint8_t ct, uint8_t mt, uint8_t mask_t, uint8_t shift_t,
                      uint8_t cs, uint8_t ms, uint8_t mask_s, uint8_t shift_s);

/**
 * @brief Low level function to render a rectangle filled with a solid color
 */
void rdp_fill_rectangle_raw(int16_t x0, int16_t y0, int16_t x1, int16_t y1);

/**
 * @brief Low level function to set the fill color
 */
void rdp_set_fill_color_raw(uint32_t color);

/**
 * @brief Low level function to set the fog color
 */
void rdp_set_fog_color_raw(uint32_t color);

/**
 * @brief Low level function to set the blend color
 */
void rdp_set_blend_color_raw(uint32_t color);

/**
 * @brief Low level function to set the primitive color
 */
void rdp_set_prim_color_raw(uint32_t color);

/**
 * @brief Low level function to set the environment color
 */
void rdp_set_env_color_raw(uint32_t color);

/**
 * @brief Low level function to set the color combiner parameters
 */
void rdp_set_combine_mode_raw(uint64_t flags);

/**
 * @brief Low level function to set RDRAM pointer to a texture image
 */
void rdp_set_texture_image_raw(uint32_t dram_addr, uint8_t format, uint8_t size, uint16_t width);

/**
 * @brief Low level function to set RDRAM pointer to the depth buffer
 */
void rdp_set_z_image_raw(uint32_t dram_addr);

/**
 * @brief Low level function to set RDRAM pointer to the color buffer
 */
void rdp_set_color_image_raw(uint32_t dram_addr, uint32_t format, uint32_t size, uint32_t width);

#ifdef __cplusplus
}
#endif

#endif
