// Copyright 2023 Dietrich Epp.
// This file is part of VADPCM. VADPCM is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

// Linear predictor selection.
//
// This module generates a codebook of second-order linear predictors and
// assigns a predictor to each block of audio. This is the only difficult part
// of the encoder. The encoder does not operate on audio data, but instead
// operates on the autocorrelation matrix for each frame of 16 audio samples.

#include "codec/vadpcm.h"

#include <stddef.h>
#include <stdint.h>

// Calculate the square error, given an autocorrelation matrix and predictor
// coefficients.
inline float vadpcm_eval(const float *restrict corr,
                         const float *restrict coeff) {
    return corr[0] +                               //
           corr[2] * coeff[0] * coeff[0] +         //
           corr[5] * coeff[1] * coeff[1] +         //
           2.0f * (corr[4] * coeff[0] * coeff[1] - //
                   corr[1] * coeff[0] -            //
                   corr[3] * coeff[1]);
}

// Calculate the best-case error from a frame, given its solved coefficients.
inline double vadpcm_eval_solved(const double *restrict corr,
                                 const double *restrict coeff) {
    // Equivalent to vadpcm_eval(), for the case where coeff are optimal for
    // this autocorrelation matrix.
    //
    // matrix = [k B^T]
    //          [B A  ]
    //
    // solve(A, B) = A^-1 B
    // eval(k, A, B, x) = k - 2B^T x + x^T A x
    // eval(k, A, B, solve(A, B))
    //   = eval(k, A, B, A^-1 B)
    //   = k - 2B^T (A^-1 B) + (A^-1 B)^T A (A^-1 B)
    //   = k - 2B^T A^-1 B + B^T A^-1^T A A^-1 B
    //   = k - 2B^T A^-1 B + B^T A^-1^T B
    //   = k - B^T A^-1 B
    //   = k - B^T solve(A, B)
    return corr[0] - corr[1] * coeff[0] - corr[3] * coeff[1];
}

// Calculate the best-case error for each frame, given the autocorrelation
// matrixes.
void vadpcm_best_error(size_t frame_count, const float (*restrict corr)[6],
                       float *restrict best_error);

// Get the mean autocorrelation matrix for each predictor. If the predictor for
// a frame is out of range, that frame is ignored.
void vadpcm_meancorrs(size_t frame_count, int predictor_count,
                      const float (*restrict corr)[6],
                      const uint8_t *restrict predictors,
                      double (*restrict pcorr)[6], int *restrict count);

// Calculate the predictor coefficients, given an autocorrelation matrix. The
// coefficients are chosen to minimize vadpcm_eval.
void vadpcm_solve(const double *restrict corr, double *restrict coeff);

// Adjust predictor coefficients to make them stable. Return 0 if the input
// coefficients are stable and 1 if the input coefficients are unstable and
// were modified.
int vadpcm_stabilize(double *restrict coeff);

// Assign a predictor to each frame.
vadpcm_error vadpcm_assign_predictors(size_t frame_count, int predictor_count,
                                      const float (*restrict corr)[6],
                                      uint8_t *restrict predictors);
