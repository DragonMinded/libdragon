// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once
// Binary encoding and decodeng functions. Internal header.

#include <stdint.h>

// Read a big-endian 16-bit integer.
inline uint16_t vadpcm_read16(const void *ptr) {
    const uint8_t *d = ptr;
    return (d[0] << 8) | d[1];
}

// Read a big-endian 32-bit integer.
inline uint32_t vadpcm_read32(const void *ptr) {
    const uint8_t *d = ptr;
    return ((uint32_t)d[0] << 24) | ((uint32_t)d[1] << 16) |
           ((uint32_t)d[2] << 8) | (uint32_t)d[3];
}
