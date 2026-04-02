// Copyright 2023 Dietrich Epp.
// This file is part of VADPCM. VADPCM is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "codec/predictor.h"

#include "codec/vadpcm.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// A record of the predictor error if a frame is encoded with a given predictor.
struct vadpcm_error_record {
    int predictor;
    float error;
};

// Scratch data for a frame when assigning predictors.
struct vadpcm_scratch {
    struct vadpcm_error_record p[2];
};

enum {
    // Iterations for predictor assignment.
    kVADPCMIterations = 20,
};

float vadpcm_eval(const float *restrict corr, const float *restrict coeff);

double vadpcm_eval_solved(const double *restrict corr,
                          const double *restrict coeff);

void vadpcm_best_error(size_t frame_count, const float (*restrict corr)[6],
                       float *restrict best_error) {
    for (size_t frame = 0; frame < frame_count; frame++) {
        double fcorr[6];
        for (int i = 0; i < 6; i++) {
            fcorr[i] = (double)corr[frame][i];
        }
        double coeff[2];
        vadpcm_solve(fcorr, coeff);
        float error;
        if (vadpcm_stabilize(coeff) == 0) {
            error = (float)vadpcm_eval_solved(fcorr, coeff);
        } else {
            float fcoeff[2] = {(float)coeff[0], (float)coeff[1]};
            error = (float)vadpcm_eval(corr[frame], fcoeff);
        }
        best_error[frame] = error;
    }
}

// Get the mean autocorrelation matrix for each predictor. If the predictor for
// a frame is out of range, that frame is ignored.
void vadpcm_meancorrs(size_t frame_count, int predictor_count,
                      const float (*restrict corr)[6],
                      const uint8_t *restrict predictors,
                      double (*restrict pcorr)[6], int *restrict count) {
    for (int i = 0; i < predictor_count; i++) {
        count[i] = 0;
        for (int j = 0; j < 6; j++) {
            pcorr[i][j] = 0.0;
        }
    }
    for (size_t frame = 0; frame < frame_count; frame++) {
        int predictor = predictors[frame];
        if (predictor < predictor_count) {
            count[predictor]++;
            // REVIEW: This is naive summation. Is that good enough?
            for (int j = 0; j < 6; j++) {
                pcorr[predictor][j] += (double)corr[frame][j];
            }
        }
    }
    for (int i = 0; i < predictor_count; i++) {
        if (count[i] > 0) {
            double a = 1.0 / count[i];
            for (int j = 0; j < 6; j++) {
                pcorr[i][j] *= a;
            }
        }
    }
}

void vadpcm_solve(const double *restrict corr, double *restrict coeff) {
    // For the autocorrelation matrix A, we want vector v which minimizes the
    // residual \epsilon,
    //
    // \epsilon = [1|v]^T A [1|v]
    //
    // We can rewrite this as:
    //
    // \epsilon = B + 2 C v + v^T D v
    //
    // Where B, C, and D are submatrixes of A. The minimum value, v, satisfies:
    //
    // D v + C = 0.

    double rel_epsilon = 1.0 / 4096.0;
    coeff[0] = 0.0;
    coeff[1] = 0.0;

    // The element with maximum absolute value is on the diagonal, by the
    // Cauchy-Schwarz inequality.
    double max = corr[0];
    if (corr[2] > max) {
        max = corr[2];
    }
    if (corr[5] > max) {
        max = corr[5];
    }
    double epsilon = max * rel_epsilon;

    // Solve using Gaussian elimination.
    //
    // [a b | x]
    // [b c | y]
    double a = corr[2];
    double b = corr[4];
    double c = corr[5];
    double x = corr[1];
    double y = corr[3];

    // Partial pivoting. Note that a, c are non-negative.
    int pivot = c > a;
    if (pivot) {
        double t;
        t = a;
        a = c;
        c = t;
        t = x;
        x = y;
        y = t;
    }

    // Multiply first row by 1/a: [1 b/a | x/a]
    if (a <= epsilon) {
        // Matrix is close to zero. Just use zero for the predictor
        // coefficients.
        return;
    }
    double a1 = 1.0 / a;
    double b1 = b * a1;
    double x1 = x * a1;

    // Subtract first row * b from second row: [0 c-b1*b | y - x1*b]
    double c2 = c - b1 * b;
    double y2 = y - x1 * b;

    // Multiply second row by 1/c. [0 1 | y2/c2]
    if (fabs(c2) <= epsilon) {
        // Matrix is poorly conditioned or singular. Solve as a first-order
        // system.
        coeff[pivot] = x1;
        return;
    }
    double y3 = y2 / c2;

    // Backsubstitute.
    double x4 = x1 - y3 * b1;

    coeff[pivot] = x4;
    coeff[!pivot] = y3;
}

int vadpcm_stabilize(double *restrict coeff) {
    // TODO: The new coefficient calculations have not been checked closely.
    if (coeff[1] < -1.0) {
        coeff[1] = -1.0;
        if (coeff[0] < -1.0) {
            coeff[0] = -1.0;
        } else if (coeff[0] > 1.0) {
            coeff[0] = 1.0;
        }
        return 1;
    }
    if (coeff[0] > 0.0) {
        if (coeff[1] + coeff[0] > 1.0) {
            double d = coeff[1] - coeff[0];
            if (d < -3.0) {
                d = -3.0;
            } else if (d > 1.0) {
                d = 1.0;
            }
            coeff[0] = 0.5 - 0.5 * d;
            coeff[1] = 0.5 + 0.5 * d;
            return 1;
        }
    } else {
        if (coeff[1] - coeff[0] > 1.0) {
            double d = coeff[1] + coeff[0];
            if (d < -3.0) {
                d = -3.0;
            } else if (d > 1.0) {
                d = 1.0;
            }
            coeff[0] = 0.5 * d - 0.5;
            coeff[1] = 0.5 * d + 0.5;
            return 1;
        }
    }
    return 0;
}

// Refine (improve) the existing predictor assignments. Does not assign
// unassigned predictors. Record the amount of error, squared, for each frame.
// Returns the index of an unassigned predictor, or predictor_count, if no
// predictor is unassigned.
static int vadpcm_refine_predictors(size_t frame_count, int predictor_count,
                                    const float (*restrict corr)[6],
                                    float *restrict error,
                                    uint8_t *restrict predictors) {
    // Calculate optimal predictor coefficients for each predictor.
    double pcorr[kVADPCMMaxPredictorCount][6];
    int count[kVADPCMMaxPredictorCount];
    vadpcm_meancorrs(frame_count, predictor_count, corr, predictors, pcorr,
                     count);

    float coeff[kVADPCMMaxPredictorCount][2];
    int active_count = 0;
    for (int i = 0; i < predictor_count; i++) {
        if (count[i] > 0) {
            double dcoeff[2];
            vadpcm_solve(pcorr[i], dcoeff);
            vadpcm_stabilize(dcoeff);
            for (int j = 0; j < 2; j++) {
                coeff[active_count][j] = (float)dcoeff[j];
            }
            active_count++;
        }
    }

    // Assign frames to the best predictor for each frame, and record the amount
    // of error.
    int count2[kVADPCMMaxPredictorCount];
    for (int i = 0; i < active_count; i++) {
        count2[i] = 0;
    }
    for (size_t frame = 0; frame < frame_count; frame++) {
        int fpredictor = 0;
        float ferror = 0.0f;
        for (int i = 0; i < active_count; i++) {
            float e = vadpcm_eval(corr[frame], coeff[i]);
            if (i == 0 || e < ferror) {
                fpredictor = i;
                ferror = e;
            }
        }
        predictors[frame] = fpredictor;
        error[frame] = ferror;
        count2[fpredictor]++;
    }
    for (int i = 0; i < active_count; i++) {
        if (count2[i] == 0) {
            return i;
        }
    }
    return active_count;
}

// Find the frame where the error is highest, relative to the best case.
static size_t vadpcm_worst_frame(size_t frame_count,
                                 const float *restrict best_error,
                                 const float *restrict error) {
    float best_improvement = error[0] - best_error[0];
    size_t best_index = 0;
    for (size_t frame = 1; frame < frame_count; frame++) {
        float improvement = error[frame] - best_error[frame];
        if (improvement > best_improvement) {
            best_improvement = improvement;
            best_index = frame;
        }
    }
    return best_index;
}

vadpcm_error vadpcm_assign_predictors(size_t frame_count, int predictor_count,
                                      const float (*restrict corr)[6],
                                      uint8_t *restrict predictors) {
    memset(predictors, 0, frame_count);
    if (predictor_count <= 1) {
        return 0;
    }
    float *best_error = malloc(frame_count * sizeof(*best_error));
    if (best_error == NULL) {
        return kVADPCMErrMemory;
    }
    vadpcm_best_error(frame_count, corr, best_error);
    float *error = malloc(frame_count * sizeof(*error));
    if (error == NULL) {
        free(best_error);
        return kVADPCMErrMemory;
    }
    int unassigned = predictor_count;
    int active_count = 1;
    for (int iter = 0; iter < kVADPCMIterations; iter++) {
        if (unassigned < predictor_count) {
            size_t worst = vadpcm_worst_frame(frame_count, best_error, error);
            predictors[worst] = unassigned;
            if (unassigned >= active_count) {
                active_count = unassigned + 1;
            }
        }
        unassigned = vadpcm_refine_predictors(frame_count, active_count, corr,
                                              error, predictors);
    }
    free(best_error);
    free(error);
    return 0;
}
