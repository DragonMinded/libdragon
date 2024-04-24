// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "vadpcm.h"

#include <limits.h>

#include <stdio.h>

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
                int residual = residuals[k] << scaling;
                accumulator[k] += residual << 11;
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

#if TEST
#include "lib/vadpcm/test.h"

#include <stdio.h>
#include <stdlib.h>

void test_decode(const char *name, int predictor_count, int order,
                 struct vadpcm_vector *codebook, size_t frame_count,
                 const void *vadpcm, const int16_t *pcm) {
    size_t sample_count = frame_count * kVADPCMFrameSampleCount;
    int16_t *out_pcm = xmalloc(sizeof(*out_pcm) * sample_count);
    struct vadpcm_vector state = {{0}};
    vadpcm_error err = vadpcm_decode(predictor_count, order, codebook, &state,
                                     frame_count, out_pcm, vadpcm);
    if (err != 0) {
        fprintf(stderr, "error: test_decode %s: %s", name,
                vadpcm_error_name2(err));
        test_failure_count++;
        goto done;
    }
    for (size_t i = 0; i < sample_count; i++) {
        if (pcm[i] != out_pcm[i]) {
            fprintf(stderr,
                    "error: test_decode %s: output does not match, "
                    "index = %zu\n",
                    name, i);
            size_t frame = i / kVADPCMFrameSampleCount;
            show_pcm_diff(pcm + frame * kVADPCMFrameSampleCount,
                          out_pcm + frame * kVADPCMFrameSampleCount);
            test_failure_count++;
            goto done;
        }
    }
done:
    free(out_pcm);
}

#endif
