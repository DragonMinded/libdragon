/**
 * @file sprite.h
 * @brief 2D Graphics
 * @ingroup graphics
 */
#ifndef __LIBDRAGON_SPRITE_H
#define __LIBDRAGON_SPRITE_H

#include <stdint.h>
#include <surface.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * @brief Sprite structure.
 * 
 * A "sprite" (as saved in a `.sprite` file) is a 2D image with
 * metadata attached to them to facilitate drawing it onto N64.
 * 
 * Despite the name, a libdragon sprite is basically the basic format
 * to handle assets for images. It is commonly used for handling
 * textures, full screen images like splash screens, tile maps,
 * font pictures, and even "real" 2D sprites.
 * 
 * If the sprite uses a color-indexed format like #FMT_CI4 or #FMT_CI8,
 * the sprite contains also the corresponding palette.
 * 
 * To convert an image file to libdragon's sprite format, use
 * the mksprite tool. To load a sprite into memory, use #sprite_load.
 */
typedef struct sprite_s
{
    /** @brief Width in pixels */
    uint16_t width;
    /** @brief Height in pixels */
    uint16_t height;
    /** @brief DEPRECATED: do not use this field. Use TEX_FORMAT_BITDEPTH(sprite->format) instead.  */
    uint8_t bitdepth __attribute__((deprecated("use TEX_FORMAT_BITDEPTH(sprite->format) instead")));
    /** @brief Various flags, including texture format */
    uint8_t flags;
    /** @brief Number of horizontal sub-tiles  */
    uint8_t hslices;
    /** @brief Number of vertical sub-tiles */
    uint8_t vslices;

    /** @brief Start of graphics data */
    uint32_t data[0];
} sprite_t;

#define SPRITE_FLAGS_TEXFORMAT     0x1F  ///< Pixel format of the sprite
#define SPRITE_FLAGS_EXT           0x80  ///< Sprite contains extended information (new format)

/**
 * @brief Load a sprite from disk
 * 
 * @param fn           Filename of the sprite, including filesystem specifier.
 *                     For instance: "rom:/hero.sprite" to load from DFS.
 * @return sprite_t*   The loaded sprite
 */
sprite_t *sprite_load(const char *fn);

/** @brief Deallocate a sprite */
void sprite_free(sprite_t *sprite);

/** @brief Get the sprite tex format */
inline tex_format_t sprite_get_format(sprite_t *sprite) {
    return sprite->flags & SPRITE_FLAGS_TEXFORMAT;
}

/** 
 * @brief Create a surface_t pointing to the full sprite contents.
 * 
 * This function can be used to pass a full sprite to functions accepting
 * a #surface_t. 
 * 
 * Notice that no memory allocations or copies are performed:
 * the returned surface will point to the sprite contents.
 * 
 * @param  sprite      The sprite
 * @return             The surface pointing to the sprite
 */
surface_t sprite_get_pixels(sprite_t *sprite);

/**
 * @brief Create a surface_t pointing to the contents of a LOD level.
 * 
 * This function can be used to access LOD images within a sprite file.
 * It is useful for sprites created by mksprite containing multiple
 * mipmap levels.
 * 
 * LOD levels are indexed from 1 upward. 0 refers to the main sprite,
 * so calling `sprite_get_lod_pixels(s, 0)` is equivalent to 
 * `sprite_get_pixels(s)`.
 * 
 * Notice that no memory allocations or copies are performed:
 * the returned surface will point to the sprite contents.
 * 
 * @param sprite        The sprite to access
 * @param num_level     The number of LOD level. 0 is the main sprite.
 * @return surface_t    The surface containing the data.
 */
surface_t sprite_get_lod_pixels(sprite_t *sprite, int num_level);

/** 
 * @brief Return a surface_t pointing to a specific tile of the spritemap.
 * 
 * A sprite can be used as a spritemap, that is a collection of multiple
 * smaller images of equal size, called "tiles". In this case, the number
 * of tiles is stored in the members `hslices` and `vslices` of the
 * sprite structure.
 * 
 * This function allows to get a surface that points to the specific sub-tile,
 * so that it can accessed directly.
 * 
 * @param   sprite      The sprite used as spritemap
 * @param   h           Horizontal index of the tile to access
 * @param   v           Vertical index of the tile to access
 * @return              A surface pointing to the tile 
 */
surface_t sprite_get_tile(sprite_t *sprite, int h, int v);

/**
 * @brief Access the sprite palette
 * 
 * A sprite can also contain a palette, in case the sprite data is color-indexed
 * (that is, the format is either #FMT_CI4 or #FMT_CI8).
 * 
 * This function returns a pointer to the raw palette data contained in the sprite.
 * 
 * @param   sprite      The sprite to access
 * @return              A pointer to the palette data, or NULL if the sprite does not have a palette
 */
uint16_t* sprite_get_palette(sprite_t *sprite);

#ifdef __cplusplus
}
#endif

#endif
