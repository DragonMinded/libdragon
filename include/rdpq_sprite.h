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

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct sprite_s sprite_t;
typedef struct rdpq_texparms_s rdpq_texparms_t;
typedef struct rdpq_blitparms_s rdpq_blitparms_t;
///@endcond

/**
 * @brief Upload a sprite to TMEM, making it ready for drawing
 * 
 * This function will upload a sprite to TMEM, making it ready for drawing.
 * It is similar to #rdpq_tex_upload which can be used for any surface, but
 * it builds upon it with sprite-specific features:
 * 
 *  * If the sprite contains mipmaps, the whole mipmap chain is uploaded to TMEM
 *    as well. Moreover, mipmaps are automatically enabled in the render mode
 *    (via #rdpq_mode_mipmap).
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
 * or use #rdpq_tex_upload_sub to manually upload a smaller portion of the sprite.
 * 
 * To load multiple sprites in TMEM at once (for instance, for multitexturing),
 * you can manually specify the @p parms->tmem_addr for the second sprite, or
 * call #rdpq_tex_multi_begin / #rdpq_tex_multi_end around multiple calls to
 * #rdpq_sprite_upload. For instance:
 * 
 * @code{.c}
 *      // Load multiple sprites in TMEM, with auto-TMEM allocation.
 *      rdpq_tex_multi_begin();
 *      rdpq_sprite_upload(TILE0, sprite0, NULL);
 *      rdpq_sprite_upload(TILE1, sprite1, NULL);
 *      rdpq_tex_multi_end();
 * @endcode
 * 
 * To speed up loading of a sprite, you can record the loading sequence in
 * a rspq block and replay it any time later. For instance:
 * 
 * @code{.c}
 *      sprite_t *hero = sprite_load("rom:/hero.sprite");
 * 
 *      // Record the loading sequence in a rspq block
 *      rspq_block_begin();
 *          rdpq_sprite_upload(TILE0, hero, NULL);
 *      rspq_block_t *hero_load = rspq_block_end();
 * 
 *      // Later, load the sprite
 *      rspq_block_run(hero_load);
 * 
 *      // Remember to free the block when you don't need it anymore
 *      rspq_wait();     // wait until RSP is idle
 *      rspq_block_free(hero_load);
 *      sprite_free(hero);
 * @endcode 
 *      
 * @param tile      Tile descriptor that will be initialized with this sprite
 * @param sprite    Sprite to upload
 * @param parms     Texture upload parameters to use
 * @return          Number of bytes used in TMEM for this sprite (excluding palette)
 * 
 * @see #rdpq_tex_upload
 * @see #rdpq_tex_upload_sub
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
 * @param sprite    Sprite to blit
 * @param x0        X coordinate on the framebuffer where to draw the surface
 * @param y0        Y coordinate on the framebuffer where to draw the surface
 * @param parms     Parameters for the blit operation (or NULL for default)
 */
void rdpq_sprite_blit(sprite_t *sprite, float x0, float y0, const rdpq_blitparms_t *parms);

#ifdef __cplusplus
}
#endif

#endif
