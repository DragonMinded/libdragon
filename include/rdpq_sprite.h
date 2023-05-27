/**
 * @file rdpq_sprite.h
 * @brief RDP Command queue: high-level sprite loading and blitting
 * @ingroup rdpq
 * 
 * This file contains high-level functions for uploading and drawing sprites.
 * They are similar in nature to the functions in rdpq_tex.h, but they should
 * be preferred when manipulating sprites as they can benefit from advanced
 * functionality such as optimized sprites, mipmapping, palette configuration, etc.
 */

#ifndef LIBDRAGON_RDPQ_SPRITE_H
#define LIBDRAGON_RDPQ_SPRITE_H

#include <stdint.h>

///@cond
typedef struct sprite_s sprite_t;
typedef struct rdpq_texparms_s rdpq_texparms_t;
typedef struct rdpq_blitparms_s rdpq_blitparms_t;
typedef struct rdpq_blit_transform_s rdpq_blit_transform_t;
///@endcond

/**
 * @brief Upload a sprite to TMEM, making it ready for drawing
 * 
 * This function will upload a sprite to TMEM, making it ready for drawing.
 * It is similar to #rdpq_tex_load which can be used for any surface, but
 * it builds upon it with sprite-specific features:
 * 
 *  * If the sprite contains mipmaps, the whole mipmap chain is uploaded to TMEM
 *    as well.
 *  * If the sprite contains a palette, it is uploaded to TMEM as well, and the
 *    palette is also activated in the render mode (via #rdpq_mode_tlut).
 *  * If the sprite is optimized (via mksprite --optimize), the upload function
 *    will be faster.
 * 
 * After calling this function, the specified tile descriptor will be ready
 * to be used in drawing primitives like #rdpq_triangle or #rdpq_texture_rectangle.
 * 
 * This function is meant for sprites that can be loaded in full into TMEM; it
 * will assert if the sprite does not fit TMEM. For larger sprites, either
 * use #rdpq_sprite_blit to directly draw then (handling partial uploads transparently),
 * or use #rdpq_tex_load_sub to manually upload a smaller portion of the sprite.
 * 
 * @param tile      Tile descriptor that will be initialized with this sprite
 * @param sprite    Sprite to upload
 * @param parms     Texture upload parameters to use
 * @return          Number of bytes used in TMEM for this sprite (excluding palette)
 * 
 * @see #rdpq_tex_load
 * @see #rdpq_tex_load_sub
 * @see #rdpq_sprite_blit
 */
int rdpq_sprite_upload(rdpq_tile_t tile, sprite_t *sprite, const rdpq_texparms_t *parms);


/**
 * @brief Blit a sprite to the active framebuffer
 * 
 * This function will perform a blit of a sprite to the active framebuffer,
 * with several features like source rectangle selection, scaling, rotation, etc.
 * 
 * The function is similar to #rdpq_tex_blit, but it works on a sprite rather than
 * a generic surface. In addition to the standard features of #rdpq_tex_blit,
 * it will also handle sprite-specific features:
 * 
 *  * If the sprite contains a palette, it is uploaded to TMEM as well, and the
 *    palette is also activated in the render mode (via #rdpq_mode_tlut).
 *  * If the sprite is optimized (via mksprite --optimize), the upload function
 *    will be faster.
 * 
 * Just like #rdpq_tex_blit, this function is designed to work with sprites of
 * arbitrary sizes; those that won't fit in TMEM will be automatically split
 * in multiple chunks to perform the requested operation.
 * 
 * Please refer to #rdpq_tex_blit for a full overview of the features.
 * 
 * @param sprite        Sprite to blit
 * @param x0            X coordinate on the framebuffer where to draw the surface
 * @param y0            Y coordinate on the framebuffer where to draw the surface
 * @param parms         Parameters for the blit operation (or NULL for default)
 * @param transform     Transformation for the blit operation (or NULL for default)
 */
void rdpq_sprite_blit(sprite_t *sprite, float x0, float y0, const rdpq_blitparms_t *parms, const rdpq_blit_transform_t *transform);

#endif
