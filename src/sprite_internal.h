/**
 * @file sprite_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef __LIBDRAGON_SPRITE_INTERNAL_H
#define __LIBDRAGON_SPRITE_INTERNAL_H

#include <stdbool.h>
#include <surface.h>
#include "yuv.h"

#define SPRITE_EXT_VERSION                  6        ///< Current version of the sprite extended structure

#define SPRITE_FLAG_NUMLODS                 0x0007   ///< Number of LODs, including detail texture if any (0 = no LODs)
#define SPRITE_FLAG_HAS_TEXPARMS            0x0008   ///< Sprite contains texture parameters
#define SPRITE_FLAG_HAS_DETAIL              0x0010   ///< Sprite contains detail texture
#define SPRITE_FLAG_FITS_TMEM               0x0020   ///< Set if the sprite does fit TMEM without splitting
#define SPRITE_FLAG_SHQ                     0x0040   ///< Sprite is in special SHQ format (2 mipmap levels with subtractive blending)

/**
 * @anchor SPRITE_YUV
 * @name Bit allocation for #sprite_ext_t::yuv_attrs.
 * 
 * Only meaningful for #FMT_YUV16 sprites; ignored otherwise. Three orthogonal
 * fields are packed into a single byte: the YUV colorspace ID, the chroma
 * subsampling, and the chroma plane layout. The all-zero value (BT.601 TV /
 * 4:2:0 / packed) decodes as packed UYVY, matching the historical behavior of
 * lossless YUV16 sprites that left this byte unset.
 *
 * These bit positions and the matching enums must mirror the encoder side
 * in tools/mksprite/mksprite_lossy.cpp.
 * 
 * @{
 */
//                                                  bit
//                                                  76543210
#define SPRITE_YUV_COLORSPACE_SHIFT  0           // ......XX
#define SPRITE_YUV_COLORSPACE_MASK   (0x3 << SPRITE_YUV_COLORSPACE_SHIFT)
#define SPRITE_YUV_CHROMA_SHIFT      2           // ....XX..
#define SPRITE_YUV_CHROMA_MASK       (0x3 << SPRITE_YUV_CHROMA_SHIFT)
#define SPRITE_YUV_LAYOUT_SHIFT      4           // ..XX....
#define SPRITE_YUV_LAYOUT_MASK       (0x3 << SPRITE_YUV_LAYOUT_SHIFT)
// bits 7:6 reserved for future expansion.
/** @} */

/**
 * @brief YUV colorspace identifiers for #sprite_ext_t::yuv_attrs.
 * 
 * This must mirror enum sprite_yuv_colorspace_e in tools/mksprite/mksprite_lossy.cpp
 */
enum sprite_yuv_colorspace_e {
    SPRITE_YUV_COLORSPACE_BT601_TV   = 0,
    SPRITE_YUV_COLORSPACE_BT601_FULL = 1,
    SPRITE_YUV_COLORSPACE_BT709_TV   = 2,
    SPRITE_YUV_COLORSPACE_BT709_FULL = 3,
};

/** @brief Chroma subsampling for a YUV sprite.
 *
 * Encoded into bits [3:2] of #sprite_ext_t::yuv_attrs. Only 4:2:0 and 4:2:2
 * have producers today; 4:4:4 and 4:0:0 (Y-only) are reserved.
 */
enum sprite_yuv_chroma_e {
    SPRITE_YUV_CHROMA_420 = 0,    ///< 4:2:0 (chroma at half width and half height)
    SPRITE_YUV_CHROMA_422 = 1,    ///< 4:2:2 (chroma at half width, full height)
    SPRITE_YUV_CHROMA_444 = 2,    ///< Reserved: 4:4:4 (chroma at full resolution)
    SPRITE_YUV_CHROMA_400 = 3,    ///< Reserved: 4:0:0 (luma only / grayscale)
};

/** @brief Memory layout of the YUV planes in a sprite.
 *
 * Encoded into bits [5:4] of #sprite_ext_t::yuv_attrs. PACKED is the only
 * layout that uses #FMT_YUV16 directly; SEMIPLANAR sprites store a Y plane
 * followed by an interleaved UV plane. PLANAR is reserved.
 */
enum sprite_yuv_layout_e {
    SPRITE_YUV_LAYOUT_PACKED      = 0,  ///< Single FMT_YUV16 surface (UYVY)
    SPRITE_YUV_LAYOUT_SEMIPLANAR  = 1,  ///< Y plane + interleaved UV plane (NV12 / NV16)
    SPRITE_YUV_LAYOUT_PLANAR      = 2,  ///< Reserved: separate Y, U, V planes (I420 / I422)
};

/**
 * @brief Get a pointer to the #yuv_colorspace_t identified by the colorspace
 *        bits of #sprite_ext_t::yuv_attrs.
 */
static inline const yuv_colorspace_t *__sprite_yuv_colorspace(uint8_t yuv_attrs)
{
    switch ((yuv_attrs & SPRITE_YUV_COLORSPACE_MASK) >> SPRITE_YUV_COLORSPACE_SHIFT) {
    case SPRITE_YUV_COLORSPACE_BT601_FULL: return &YUV_BT601_FULL;
    case SPRITE_YUV_COLORSPACE_BT709_TV:   return &YUV_BT709_TV;
    case SPRITE_YUV_COLORSPACE_BT709_FULL: return &YUV_BT709_FULL;
    default:                               return &YUV_BT601_TV;
    }
}

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
        uint8_t  yuv_attrs;         ///< Packed YUV attributes: colorspace, chroma subsampling, layout (only used for FMT_YUV16)
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

/** @brief Get the YUV colorspace of a sprite from its extended header */
static inline const yuv_colorspace_t *sprite_ext_yuv_colorspace(sprite_ext_t *sx)
{
    if (!sx) return &YUV_BT601_TV;
    return __sprite_yuv_colorspace(sx->yuv_attrs);
}

/** @brief Function pointer type for a sprite decoder function */
typedef sprite_t *(*sprite_decode_fn)(const void *buf, int sz);

/** @brief Internal structure for registered sprite decoders */
struct sprite_decoder_s {
    const char *magic;
    int magic_len;
    sprite_decode_fn decode;
    struct sprite_decoder_s *next;
};

/** @brief Register a sprite decoder for a specific magic string. */
void sprite_decoder_register(const char *magic, sprite_decode_fn decode);

/** @brief Unregister the sprite decoder for a specific magic string. */
int sprite_decoder_unregister(const char *magic);

#endif
