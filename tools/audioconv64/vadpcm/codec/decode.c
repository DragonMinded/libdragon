// Copyright 2022 Dietrich Epp.
// This file is part of VADPCM. VADPCM is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "codec/vadpcm.h"

#include <limits.h>

// Extend the sign bit of a 4-bit integer.
static int vadpcm_ext4(int x) {
    return x > 7 ? x - 16 : x;
}

// Clamp an integer to a 16-bit range.
static int vadpcm_clamp16(int x) {
    if (x < -0x8000 || 0x7fff < x) {
        return (x >> (sizeof(int) * CHAR_BIT - 1)) ^ 0x7fff;
    }
    return x;
}

vadpcm_error vadpcm_decode(int predictor_count, int order,
                           const struct vadpcm_vector *restrict codebook,
                           struct vadpcm_vector *restrict state,
                           size_t frame_count, int16_t *restrict dest,
                           const void *restrict src) {
    const uint8_t *sptr = src;
    for (size_t frame = 0; frame < frame_count; frame++) {
        const uint8_t *fin = sptr + kVADPCMFrameByteSize * frame;

        // Control byte: scaling & predictor index.
        int control = fin[0];
        int scaling = control >> 4;
        int predictor_index = control & 15;
        if (predictor_index >= predictor_count) {
            return kVADPCMErrInvalidData;
        }
        const struct vadpcm_vector *predictor =
            codebook + order * predictor_index;

        // Decode each of the two vectors within the frame.
        for (int vector = 0; vector < 2; vector++) {
            int32_t accumulator[8];
            for (int i = 0; i < 8; i++) {
                accumulator[i] = 0;
            }

            // Accumulate the part of the predictor from the previous block.
            for (int k = 0; k < order; k++) {
                int sample = state->v[8 - order + k];
                for (int i = 0; i < 8; i++) {
                    accumulator[i] += sample * predictor[k].v[i];
                }
            }

            // Decode the ADPCM residual.
            int residuals[8];
            for (int i = 0; i < 4; i++) {
                int byte = fin[1 + 4 * vector + i];
                residuals[2 * i] = vadpcm_ext4(byte >> 4);
                residuals[2 * i + 1] = vadpcm_ext4(byte & 15);
            }

            // Accumulate the residual and predicted values.
            const struct vadpcm_vector *v = &predictor[order - 1];
            for (int k = 0; k < 8; k++) {
                int residual = residuals[k] * (1 << scaling);
                accumulator[k] += residual * (1 << 11);
                for (int i = 0; i < 7 - k; i++) {
                    accumulator[k + 1 + i] += residual * v->v[i];
                }
            }

            // Discard fractional part and clamp to 16-bit range.
            for (int i = 0; i < 8; i++) {
                int sample = vadpcm_clamp16(accumulator[i] >> 11);
                dest[kVADPCMFrameSampleCount * frame + 8 * vector + i] = sample;
                state->v[i] = sample;
            }
        }
    }
    return 0;
}
