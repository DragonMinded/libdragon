// Copyright 2023 Dietrich Epp.
// This file is part of VADPCM. VADPCM is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

// Autocorrelation matrixes for blocks of audio
//
// Autocorrelation is a symmetric 3x3 matrix. The upper triangle is stored.
// Indexes:
//
// [0 1 3]
// [_ 2 4]
// [_ _ 5]

#include <stddef.h>
#include <stdint.h>

// Calculate the autocorrelation matrix for each frame.
void vadpcm_autocorr(size_t frame_count, float (*restrict corr)[6],
                     const int16_t *restrict src);
