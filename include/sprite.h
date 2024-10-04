/**
 * @file sprite.h
 * @brief 2D Graphics
 * @ingroup graphics
 */
#ifndef __LIBDRAGON_SPRITE_H
#define __LIBDRAGON_SPRITE_H

#include <stdint.h>
#include <stdbool.h>
#include <surface.h>

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct rdpq_texparms_s rdpq_texparms_t;
///@endcond


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
    union {
    /** @brief DEPRECATED: do not use this field. Use sprite_get_format(sprite) instead.  */
    uint8_t format __attribute__((deprecated("use sprite_get_format() instead")));
    /** @brief Various flags, including texture format */
    uint8_t flags;
    };
    /** @brief Number of horizontal sub-tiles  */
    uint8_t hslices;
    /** @brief Number of vertical sub-tiles */
    uint8_t vslices;

    /** @brief Start of graphics data */
    uint32_t data[0];
} sprite_t;

/** 
 * @brief Sprite detail texture information structure.
 * 
 * A "detail texture" is a 2D image with metadata attached to it 
 * to increase the perceived resolution of the main sprite when rendering
 * with little to no additional TMEM usage.
 * 
 * If the sprite uses a detail texture, its information can be retreived
 * using the #sprite_get_detail_pixels function.
 * 
 * To include a detail texture to libdragon's sprite format, use
 * the mksprite tool with --detail argument. 
 * 
 * #rdpq_sprite_upload automatically uploads detail textures associated with
 * the sprite.
 */
typedef struct sprite_detail_s
{
    /** @brief Is the detail texture the same as the main surface of the sprite, used for fractal detailing */
    bool            use_main_tex;
    /** @brief Blend factor of the detail texture in range of 0 to 1 */
    float           blend_factor;
} sprite_detail_t;

#define SPRITE_FLAGS_TEXFORMAT      0x1F    ///< Pixel format of the sprite
#define SPRITE_FLAGS_OWNEDBUFFER    0x20    ///< Flag specifying that the sprite buffer must be freed by sprite_free
#define SPRITE_FLAGS_EXT            0x80    ///< Sprite contains extended information (new format)


/**
 * @brief Load a sprite from a filesystem (eg: ROM)
 * 
 * This function loads a full sprite from a filesystem. Notice that there is no
 * streaming support, so the file is fully loaded into RDRAM, in its final
 * uncompressed format.
 * 
 * sprite_load internally uses the asset API (#asset_load), so the sprite file
 * is transparently uncompressed if needed.
 * 
 * @param fn           Filename of the sprite, including filesystem specifier.
 *                     For instance: "rom:/hero.sprite" to load from DFS.
 * @return sprite_t*   The loaded sprite
 */
sprite_t *sprite_load(const char *fn);

/**
 * @brief Load a sprite from a buffer
 * 
 * This function loads a sprite from a buffer corresponding to sprite
 * file data in memory. The function also performs any necessary processing
 * to load the sprite file data.
 * 
 * sprite_load_buf functions in-place which means it does not allocate another
 * buffer for the loaded sprite. So, sprite_free will not remove the sprite data
 * from memory. This means that the input buffer must be freed manually after
 * sprite_free is called.
 *
 * @param buf           Pointer to the sprite file data
 * @param sz            Size of the sprite file buffer
 * @return sprite_t*    The loaded sprite
 */
sprite_t *sprite_load_buf(void *buf, int sz);

/** @brief Deallocate a sprite */
void sprite_free(sprite_t *sprite);

/** 
 * @brief Get the sprite texture format
 * 
 * @param sprite    The sprite
 * @return          The texture format
 */
inline tex_format_t sprite_get_format(sprite_t *sprite) {
    return (tex_format_t)(sprite->flags & SPRITE_FLAGS_TEXFORMAT);
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
 * @brief Create a surface_t pointing to the contents of a detail texture.
 * 
 * This function can be used to access detail texture within a sprite file.
 * It is useful for sprites created by mksprite containing one.
 * 
 * If there isn't a detail texture, the returned surface is 0.
 * 
 * Additional detail information such as factor or texparms are accessible 
 * through the filled sprite_detail_t and rdpq_texparms_t structure.
 * If you don't wish to use this information, pass NULL to the info argument(s).
 * 
 * Notice that no memory allocations or copies are performed:
 * the returned surface will point to the sprite contents.
 * 
 * @param sprite        The sprite to access
 * @param info          The detail information struct to fill if needed
 * @param infoparms     The detail texture sampling struct to fill if needed
 * @return surface_t    The surface containing the data.
 */
surface_t sprite_get_detail_pixels(sprite_t *sprite, sprite_detail_t *info, rdpq_texparms_t *infoparms);

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
 * @brief Access the sprite palette (if any)
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

/**
 * @brief Get a copy of the RDP texparms, optionally stored within the sprite.
 * 
 * This function allows to obtain the RDP texparms structure stored within the
 * sprite, if any. This structure is used by the RDP to set texture properties
 * such as wrapping, mirroring, etc. It can be added to the sprite via
 * the mksprite tool, using the `--texparms` option.
 * 
 * @param sprite        The sprite to access
 * @param parms         The texparms structure to fill
 * @return              true if the sprite contain RDP texparms, false otherwise
 */
bool sprite_get_texparms(sprite_t *sprite, rdpq_texparms_t *parms);

/**
 * @brief Return the number of LOD levels stored within the sprite (including the main image).
 * 
 * @param sprite        The sprite to access
 * @return              The number of LOD levels
 */
int sprite_get_lod_count(sprite_t *sprite);

/**
 * @brief Return true if the sprite fits in TMEM without splitting
 * 
 * This function returns true if the sprite can be fully uploaded in TMEM
 * (including all its LODs, detail texture and palettes).
 * 
 * When working on 3D graphics, each texture must fit into RDP TMEM (4 KiB),
 * otherwise it cannot be used. All sprites that are meant to be used as
 * textures should fit in TMEM. 
 * 
 * In case of 2D graphics, it is more common to have images of arbitrary size.
 * They can be drawn with #rdpq_sprite_blit (accelerated) or #graphics_draw_sprite
 * (CPU) without specific limits (the RDP accelerated
 * version does internally need to split the sprite in multiple parts, but
 * that is indeed possible).
 * 
 * This function is mostly for debugging purposes, as it can help validating
 * whether a sprite can be used as a texture or not.
 * 
 * @param sprite        The sprite to access
 * @return              True if the sprite fits TMEM, false otherwise
 */
bool sprite_fits_tmem(sprite_t *sprite);

/** 
 * @brief Return true if the sprite is in SHQ format
 * 
 * This is a special sprite made of two mipmaps (one I4 and one RGBA16)
 * that must be displayed using subtractive blending.
 * 
 * @param sprite        The sprite to access
 * @return              True if the sprite is in SHQ format, false otherwise
 */
bool sprite_is_shq(sprite_t *sprite);

#ifdef __cplusplus
}
#endif

#endif
