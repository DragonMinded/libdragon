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

/**
 * @brief File-start signature of a Lossy-sprite Level 3 (H264I) file (8 bytes).
 *
 * The leading four zero bytes are a safety pad: if an H264I file reaches
 * #sprite_load without lspr3_init() having registered the decoder,
 * the buffer is reinterpreted as a #sprite_t — the pad forces width=height=0
 * so the situation is detectable and a clear assertion can fire instead of
 * silently producing a corrupt sprite.
 */
#define H264I_FILE_MAGIC      "\0\0\0\0H264"
/** @brief Size in bytes of #H264I_FILE_MAGIC (drops the trailing NUL). */
#define H264I_FILE_MAGIC_SIZE (sizeof(H264I_FILE_MAGIC) - 1)

/**
 * @brief File-start signature of a Lossy-sprite Level 1 (BC1Q) file (8 bytes).
 *
 * Same safety-pad rationale as #H264I_FILE_MAGIC: the leading four zero bytes
 * force width=height=0 if a BC1Q file is reinterpreted as a #sprite_t when
 * lspr1_init() has not registered the decoder, so the situation produces
 * a clear assertion instead of a silently corrupt sprite.
 */
#define BC1Q_FILE_MAGIC      "\0\0\0\0BC1Q"
/** @brief Size in bytes of #BC1Q_FILE_MAGIC (drops the trailing NUL). */
#define BC1Q_FILE_MAGIC_SIZE (sizeof(BC1Q_FILE_MAGIC) - 1)

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
        uint8_t  padding;           ///< Padding
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

/** @brief Function pointer type for a sprite decodable test function */
typedef bool (*sprite_decodable_fn)(const void *buf, int sz);

/** @brief Function pointer type for a sprite decoder function */
typedef sprite_t *(*sprite_decode_fn)(const void *buf, int sz);

/** @brief Internal structure for registered sprite decoders */
typedef struct sprite_decoder_s {
    sprite_decodable_fn decodable;
    sprite_decode_fn decode;
    struct sprite_decoder_s *next;
} sprite_decoder_t;

/** @brief Register a sprite decoder for a specific magic string. */
sprite_decoder_t *sprite_decoder_register(sprite_decodable_fn decodable, sprite_decode_fn decode);

/** @brief Unregister the sprite decoder for a specific magic string. */
int sprite_decoder_unregister(sprite_decoder_t *decoder);

#endif
