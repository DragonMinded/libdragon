#ifndef __LIBDRAGON_SPRITE_INTERNAL_H
#define __LIBDRAGON_SPRITE_INTERNAL_H

#include <stdbool.h>

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
    uint16_t pal_file_pos;      ///< Position of the palette in the file
    uint16_t __padding0;        ///< padding
    /// Information on LODs
    struct sprite_lod_s {
        uint16_t width;            ///< Width of this LOD
        uint16_t height;           ///< Height of this LOD
        uint32_t fmt_file_pos;     ///< Top 8 bits: format; lowest 24 bits: absolute offset in the file
    } lods[7];                  ///< Information on the available LODs
} sprite_ext_t;

_Static_assert(sizeof(sprite_ext_t) == 64, "invalid sizeof(sprite_ext_t)");

/** @brief Convert a sprite from the old format with implicit texture format */ 
bool __sprite_upgrade(sprite_t *sprite);

#endif
