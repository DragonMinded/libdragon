/**
 * @file sprite_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef __LIBDRAGON_SPRITE_INTERNAL_H
#define __LIBDRAGON_SPRITE_INTERNAL_H

#include <stdbool.h>
#include <surface.h>

#define SPRITE_EXT_VERSION                  6        ///< Current version of the sprite extended structure

#define SPRITE_FLAG_NUMLODS                 0x0007   ///< Number of LODs, including detail texture if any (0 = no LODs)
#define SPRITE_FLAG_HAS_TEXPARMS            0x0008   ///< Sprite contains texture parameters
#define SPRITE_FLAG_HAS_DETAIL              0x0010   ///< Sprite contains detail texture
#define SPRITE_FLAG_FITS_TMEM               0x0020   ///< Set if the sprite does fit TMEM without splitting
#define SPRITE_FLAG_SHQ                     0x0040   ///< Sprite is in special SHQ format (2 mipmap levels with subtractive blending)
#define SPRITE_FLAG_YUV_PLANAR              0x0080   ///< Sprite stores YUV 4:2:0 in 3 separate planes (Y, U, V); render via #rdpq_sprite_blit (which routes to #yuv_tex_blit)

/** @brief YUV colorspace identifier stored in #sprite_ext_t::colorspace.
 *
 * Only meaningful for #FMT_YUV16 sprites; ignored otherwise. The default
 * value (0) maps to BT.601 TV-range to match the previous behavior of
 * #rdpq_set_mode_yuv when no colorspace was specified. */
enum sprite_colorspace_e {
    SPRITE_COLORSPACE_BT601_TV   = 0,
    SPRITE_COLORSPACE_BT601_FULL = 1,
    SPRITE_COLORSPACE_BT709_TV   = 2,
    SPRITE_COLORSPACE_BT709_FULL = 3,
};

/** 
 * @brief Internal structure used as additional sprite header
 * 
 * This data is put at the end of the main image data of the sprite. This allows
 * the library to stay backward compatible with old sprites created before this
 * structure existed.
 * 
 * The existence of the structure in the sprite can be checked via #SPRITE_FLAGS_EXT.
 */
typedef struct sprite_ext_s {
    uint16_t size;              ///< Size of the structure itself (for forward compatibility)
    uint16_t version;           ///< Version of the structure (#SPRITE_EXT_VERSION)
    uint32_t pal_file_pos;      ///< Position of the palette in the file
    /// Information on LODs
    struct sprite_lod_s {
        uint16_t width;            ///< Width of this LOD
        uint16_t height;           ///< Height of this LOD
        uint32_t fmt_file_pos;     ///< Top 8 bits: format; lowest 24 bits: absolute offset in the file
    } lods[7];                  ///< Information on the available LODs (if detail is present, it's always at position 6)
    struct {
        uint16_t flags;             ///< Generic Flags for the sprite
        uint8_t  pal_used_colors;   ///< Number of colors actually used in palette
        uint8_t  colorspace;        ///< YUV colorspace ID (#sprite_colorspace_e); only used for FMT_YUV16
    };
    /// @brief RDP texture parameters
    struct texparms_s {
        struct {
            float   translate;      ///< Translate the texture in pixels
            float   repeats;        ///< Number of repetitions (default: 1)
            int16_t scale_log;      ///< Power of 2 scale modifier of the texture (default: 0)
            bool    mirror;         ///< Repetition mode (default: MIRROR_NONE)
            int8_t  padding;
        } s, t; // S/T directions of texture parameters
    } texparms;                ///< RDP texture parameters
    /// @brief Detail texture parameters
    struct detail_s {
        struct texparms_s texparms;      ///< Detail LOD RDP texture parameters
        float             blend_factor;  ///< Blending factor for the detail texture at maximum zoom (0=hidden, 1=opaque)
        bool              use_main_texture; ///< True if the detail texture is the same as the LOD0 of the main texture
        uint8_t           padding[3];    ///< Padding
    } detail;                    ///< Detail texture parameters

    uint32_t data_ptr;          ///< Optional offset (from sprite base) to pixel data
} sprite_ext_t;

_Static_assert(sizeof(sprite_ext_t) == 128, "invalid sizeof(sprite_ext_t)");

/** @brief Convert a sprite from the old format with implicit texture format */
bool __sprite_upgrade(sprite_t *sprite);

/** @brief Access the sprite extended structure, or NULL if it does not exist. */
sprite_ext_t *__sprite_ext(sprite_t *sprite);

/** @brief Decode an in-memory LSPR buffer into a freshly allocated sprite.
 *
 * The caller retains ownership of @p buf — the decoder does not free it.
 * The returned sprite owns its pixel storage and must be released with
 * #sprite_free. */
sprite_t *__lossysprite_decode_buf(const void *buf, int sz);

#endif
