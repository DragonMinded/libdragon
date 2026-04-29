// Copyright 2023 Dietrich Epp.
// This file is part of VADPCM. VADPCM is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

// Random number generator. Internal header.

#include <stdint.h>

inline uint32_t vadpcm_rng(uint32_t state) {
    // 0xd9f5: Computationally Easy, Spectrally Good Multipliers for
    // Congruential Pseudorandom Number Generators, Steele and Vigna, Table 7,
    // p.18
    //
    // 0x6487ed51: pi << 29, relatively prime.
    return state * 0xd9f5 + 0x6487ed51;
}
