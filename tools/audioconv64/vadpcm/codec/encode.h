// Copyright 2023 Dietrich Epp.
// This file is part of VADPCM. VADPCM is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

#include "codec/vadpcm.h"

#include <stddef.h>
#include <stdint.h>

struct vadpcm_vector;
struct vadpcm_stats;

// Calculate codebook vectors for one predictor, given the predictor
// coefficients.
void vadpcm_make_vectors(const double *restrict coeff,
                         struct vadpcm_vector *restrict vectors);

// Create a codebook, given the frame autocorrelation matrixes and the
// assignment from frames to predictors.
void vadpcm_make_codebook(size_t frame_count, int predictor_count,
                          const float (*restrict corr)[6],
                          const uint8_t *restrict predictors,
                          struct vadpcm_vector *restrict codebook);

// Current state of the encoder. The state can be initialized to zero.
struct vadpcm_encoder_state {
    int16_t data[2];
    uint32_t rng;
};

// Encode audio as VADPCM, given the assignment of each frame to a predictor.
void vadpcm_encode_data(size_t frame_count, void *restrict dest,
                        const int16_t *restrict src,
                        const uint8_t *restrict predictors,
                        const struct vadpcm_vector *restrict codebook,
                        struct vadpcm_stats *restrict stats,
                        struct vadpcm_encoder_state *restrict encoder_state);
