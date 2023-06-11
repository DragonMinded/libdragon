#ifndef __LIBDRAGON_SPRITE_INTERNAL_H
#define __LIBDRAGON_SPRITE_INTERNAL_H

#include <stdbool.h>
#include <surface.h>

#define SPRITE_FLAG_NUMLODS                 0x0007   ///< Number of LODs (0 = no LODs)
#define SPRITE_FLAG_HAS_TEXPARMS            0x0008   ///< Sprite contains texture parameters
#define SPRITE_FLAG_HAS_DETAIL              0x0010   ///< Sprite contains detail texture
#define SPRITE_FLAG_DETAIL_USE_LOD0         0x0020   ///< Detail texture is the same as LOD0 (fractal detailing)
#define SPRITE_FLAG_DETAIL_HAS_TEXPARMS     0x0040   ///< Detail texture has its own texparms

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
    uint16_t version;           ///< Version of the structure (currently 1)
    uint32_t pal_file_pos;      ///< Position of the palette in the file
    /// Information on LODs
    struct sprite_lod_s {
        uint16_t width;            ///< Width of this LOD
        uint16_t height;           ///< Height of this LOD
        uint32_t fmt_file_pos;     ///< Top 8 bits: format; lowest 24 bits: absolute offset in the file
    } lods[8];                  ///< Information on the available LODs (0-6 LODs, 7 = detail texture)
    struct {
        uint16_t flags;             ///< Generic Flags for the sprite
        uint16_t padding;           ///< Padding
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
        tex_format_t      format;        ///< Detail LOD format
        float             blend_factor;  ///< Blending factor for the detail texture at maximum zoom (0=hidden, 1=opaque)
        //bool              use_main_texture; ///< True if the detail texture is the same as the LOD0 of the main texture
    } detail;                    ///< Detail texture parameters
} sprite_ext_t;

//_Static_assert(sizeof(sprite_ext_t) == 104, "invalid sizeof(sprite_ext_t)");

/** @brief Convert a sprite from the old format with implicit texture format */ 
bool __sprite_upgrade(sprite_t *sprite);

#endif
