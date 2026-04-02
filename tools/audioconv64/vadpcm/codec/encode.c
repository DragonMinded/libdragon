// Copyright 2022 Dietrich Epp.
// This file is part of VADPCM. VADPCM is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "codec/encode.h"

#include "codec/autocorr.h"
#include "codec/predictor.h"
#include "codec/random.h"
#include "codec/vadpcm.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    // Order of predictor to use. Other orders are not supported.
    kVADPCMOrder = 2,

    // Number of predictors to use, by default.
    kVADPCMDefaultPredictorCount = 4,
};

void vadpcm_make_vectors(const double *restrict coeff,
                         struct vadpcm_vector *restrict vectors) {
    double scale = (double)(1 << 11);
    for (int i = 0; i < 2; i++) {
        double x1 = 0.0, x2 = 0.0;
        if (i == 0) {
            x2 = scale;
        } else {
            x1 = scale;
        }
        for (int j = 0; j < 8; j++) {
            double x = coeff[0] * x1 + coeff[1] * x2;
            int value;
            if (x > 0x7fff) {
                value = 0x7fff;
            } else if (x < -0x8000) {
                value = -0x8000;
            } else {
                value = lrint(x);
            }
            vectors[i].v[j] = value;
            x2 = x1;
            x1 = x;
        }
    }
}

void vadpcm_make_codebook(size_t frame_count, int predictor_count,
                          const float (*restrict corr)[6],
                          const uint8_t *restrict predictors,
                          struct vadpcm_vector *restrict codebook) {
    double pcorr[kVADPCMMaxPredictorCount][6];
    int count[kVADPCMMaxPredictorCount];
    vadpcm_meancorrs(frame_count, predictor_count, corr, predictors, pcorr,
                     count);
    for (int i = 0; i < predictor_count; i++) {
        if (count[i] > 0) {
            double coeff[2];
            vadpcm_solve(pcorr[i], coeff);
            vadpcm_make_vectors(coeff, codebook + 2 * i);
        } else {
            memset(codebook + 2 * i, 0, sizeof(struct vadpcm_vector) * 2);
        }
    }
}

static int vadpcm_getshift(int min, int max) {
    int shift = 0;
    while (shift < 12 && (min < -8 || 7 < max)) {
        min >>= 1;
        max >>= 1;
        shift++;
    }
    return shift;
}

// Encode audio as VADPCM, given the assignment of each frame to a predictor.
void vadpcm_encode_data(size_t frame_count, void *restrict dest,
                        const int16_t *restrict src,
                        const uint8_t *restrict predictors,
                        const struct vadpcm_vector *restrict codebook,
                        struct vadpcm_stats *restrict stats,
                        struct vadpcm_encoder_state *restrict encoder_state) {
    uint32_t rng_state = encoder_state->rng;
    uint8_t *destptr = dest;
    int state[4];
    state[0] = encoder_state->data[0];
    state[1] = encoder_state->data[1];
    stats->signal_mean_square = 0.0;
    stats->error_mean_square = 0.0;
    for (size_t frame = 0; frame < frame_count; frame++) {
        unsigned predictor = predictors[frame];
        const struct vadpcm_vector *restrict pvec = codebook + 2 * predictor;
        int accumulator[8], s0, s1, s, a, r, min, max;

        double sum_square = 0.0;
        for (int i = 0; i < kVADPCMFrameSampleCount; i++) {
            double value = (double)src[frame * 16 + i];
            sum_square += value * value;
        }
        stats->signal_mean_square += sum_square;

        // Calculate the residual with full precision, and figure out the
        // scaling factor necessary to encode it.
        state[2] = src[frame * 16 + 6];
        state[3] = src[frame * 16 + 7];
        min = 0;
        max = 0;
        for (int vector = 0; vector < 2; vector++) {
            s0 = state[vector * 2];
            s1 = state[vector * 2 + 1];
            for (int i = 0; i < 8; i++) {
                accumulator[i] =
                    (src[frame * 16 + vector * 8 + i] * (1 << 11)) -
                    s0 * pvec[0].v[i] - s1 * pvec[1].v[i];
            }
            for (int i = 0; i < 8; i++) {
                s = accumulator[i] >> 11;
                if (s < min) {
                    min = s;
                }
                if (s > max) {
                    max = s;
                }
                for (int j = 0; j < 7 - i; j++) {
                    accumulator[i + 1 + j] -= s * pvec[1].v[j];
                }
            }
        }
        int shift = vadpcm_getshift(min, max);

        // Try a range of 3 shift values, and use the shift value that produces
        // the lowest error.
        double best_error = 0.0;
        int min_shift = shift > 0 ? shift - 1 : 0;
        int max_shift = shift < 12 ? shift + 1 : 12;
        uint32_t init_state = rng_state;
        for (shift = min_shift; shift <= max_shift; shift++) {
            rng_state = init_state;
            uint8_t fout[8];
            double error = 0.0;
            s0 = state[0];
            s1 = state[1];
            for (int vector = 0; vector < 2; vector++) {
                for (int i = 0; i < 8; i++) {
                    accumulator[i] = s0 * pvec[0].v[i] + s1 * pvec[1].v[i];
                }
                for (int i = 0; i < 8; i++) {
                    s = src[frame * 16 + vector * 8 + i];
                    a = accumulator[i] >> 11;
                    // Calculate the residual, encode as 4 bits.
                    int bias = (rng_state >> 16) >> (16 - shift);
                    rng_state = vadpcm_rng(rng_state);
                    r = (s - a + bias) >> shift;
                    if (r > 7) {
                        r = 7;
                    } else if (r < -8) {
                        r = -8;
                    }
                    accumulator[i] = r;
                    // Update state to match decoder. The accumulator has
                    // 32-bit precision, but the state carried from vector to
                    // vector is just the 16-bit output values.
                    int sout = r * (1 << shift);
                    for (int j = 0; j < 7 - i; j++) {
                        accumulator[i + 1 + j] += sout * pvec[1].v[j];
                    }
                    sout += a;
                    if (sout > 0x7fff) {
                        sout = 0x7fff;
                    } else if (sout < -0x8000) {
                        sout = -0x8000;
                    }
                    s0 = s1;
                    s1 = sout;
                    // Track encoding error.
                    double serror = s - sout;
                    error += serror * serror;
                }
                for (int i = 0; i < 4; i++) {
                    fout[vector * 4 + i] = ((accumulator[2 * i] & 15) << 4) |
                                           (accumulator[2 * i + 1] & 15);
                }
            }
            if (shift == min_shift || error < best_error) {
                destptr[frame * 9] = (shift << 4) | predictor;
                memcpy(destptr + frame * 9 + 1, fout, 8);
                state[2] = s0;
                state[3] = s1;
                best_error = error;
            }
        }
        state[0] = state[2];
        state[1] = state[3];
        stats->error_mean_square += best_error;
    }
    *encoder_state = (struct vadpcm_encoder_state){
        .data = {state[0], state[1]},
        .rng = rng_state,
    };
    double factor = 1.0 / ((double)(frame_count * kVADPCMFrameSampleCount) *
                           (32768.0 * 32768.0));
    stats->signal_mean_square *= factor;
    stats->error_mean_square *= factor;
}

vadpcm_error vadpcm_encode(const struct vadpcm_params *restrict params,
                           struct vadpcm_vector *restrict codebook,
                           size_t frame_count, void *restrict dest,
                           const int16_t *restrict src,
                           struct vadpcm_stats *stats) {
    int predictor_count = params->predictor_count;
    if (predictor_count < 1 || kVADPCMMaxPredictorCount < predictor_count) {
        return kVADPCMErrInvalidParams;
    }

    // Early exit if there is no data to encode.
    if (frame_count == 0) {
        memset(codebook, 0,
               sizeof(*codebook) * kVADPCMEncodeOrder * predictor_count);
        if (stats != NULL) {
            stats->signal_mean_square = 0.0;
            stats->error_mean_square = 0.0;
        }
        return 0;
    }

    // Scratch memory buffers.
    float (*corr)[6] = NULL;
    uint8_t *predictors = NULL;
    if (frame_count > ((size_t)-1) / (sizeof(*corr))) {
        return kVADPCMErrMemory;
    }

    // Get autocorrelation matrix for each frame.
    corr = malloc(frame_count * sizeof(*corr));
    if (corr == NULL) {
        return kVADPCMErrMemory;
    }
    predictors = malloc(frame_count);
    if (predictors == NULL) {
        free(corr);
        return kVADPCMErrMemory;
    }
    vadpcm_autocorr(frame_count, corr, src);

    // Assign predictors to each frame.
    vadpcm_error err = vadpcm_assign_predictors(frame_count, predictor_count,
                                                corr, predictors);

    if (err == 0) {
        // Create optimal codebook, given predictor assignments.
        vadpcm_make_codebook(frame_count, predictor_count, corr, predictors,
                             codebook);

        // Encode.
        struct vadpcm_stats stats_buf;
        struct vadpcm_encoder_state encoder_state = {{0, 0}, 0};
        vadpcm_encode_data(frame_count, dest, src, predictors, codebook,
                           stats != NULL ? stats : &stats_buf, &encoder_state);
    }

    free(corr);
    free(predictors);
    return err;
}
