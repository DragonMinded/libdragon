#ifndef __LIBDRAGON_ASSET_INTERNAL_H
#define __LIBDRAGON_ASSET_INTERNAL_H

#include <stdint.h>

#define ASSET_MAGIC    "DCA1"   ///< Magic compressed asset header

/** @brief Header of a compressed asset */
typedef struct {
    char magic[4];          ///< Magic header
    uint16_t algo;          ///< Compression algorithm
    uint16_t flags;         ///< Flags (unused for now)
    uint32_t cmp_size;      ///< Compressed size in bytes
    uint32_t orig_size;     ///< Original size in bytes
} asset_header_t;

_Static_assert(sizeof(asset_header_t) == 16, "invalid sizeof(asset_header_t)");

#endif
