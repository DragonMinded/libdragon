#ifndef __LIBDRAGON_ASSET_INTERNAL_H
#define __LIBDRAGON_ASSET_INTERNAL_H

#include <stdint.h>
#include <stdio.h>

#define ASSET_MAGIC    "DCA1"

typedef struct {
    uint16_t algo;
    uint16_t flags;
    uint32_t cmp_size;
    uint32_t orig_size;
} asset_header_t;

_Static_assert(sizeof(asset_header_t) == 12, "invalid sizeof(asset_header_t)");

#endif
