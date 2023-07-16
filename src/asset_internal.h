#ifndef __LIBDRAGON_ASSET_INTERNAL_H
#define __LIBDRAGON_ASSET_INTERNAL_H

#include <stdint.h>
#include <stdio.h>

#define ASSET_MAGIC    "DCA"   ///< Magic compressed asset header

/** @brief Header of a compressed asset */
typedef struct {
    char magic[3];          ///< Magic header
    uint8_t version;        ///< Version of the asset header
    uint16_t algo;          ///< Compression algorithm
    uint16_t flags;         ///< Flags (unused for now)
    uint32_t cmp_size;      ///< Compressed size in bytes
    uint32_t orig_size;     ///< Original size in bytes
} asset_header_t;

_Static_assert(sizeof(asset_header_t) == 16, "invalid sizeof(asset_header_t)");

FILE *must_fopen(const char *fn);

#endif
