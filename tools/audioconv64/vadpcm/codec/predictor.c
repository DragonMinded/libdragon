// Copyright 2023 Dietrich Epp.
// This file is part of VADPCM. VADPCM is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "codec/predictor.h"

#include "codec/vadpcm.h"

#include <float.h>
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
        float error;
        if (vadpcm_solve_stable(fcorr, coeff) == 0) {
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

    // The autocorrelation matrix is accumulated in single precision, so its
    // entries carry roughly 1e-7 relative accuracy, and a pivot smaller than
    // that is noise. This solve runs in double precision, so there is no
    // reason to give up any earlier than that. Bailing out sooner is not
    // conservative: a smooth signal is an ill-conditioned system precisely
    // because consecutive samples are correlated, which is when second-order
    // prediction is worth the most.
    double rel_epsilon = 1.0e-5;
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

// Calculate the square error for a pair of coefficients. This is vadpcm_eval,
// in double precision.
static double vadpcm_eval_at(const double *restrict corr, double c0,
                             double c1) {
    return corr[0] + corr[2] * c0 * c0 + corr[5] * c1 * c1 +
           2.0 * (corr[4] * c0 * c1 - corr[1] * c0 - corr[3] * c1);
}

// Minimize the square error over the segment from (x0,y0) to (x1,y1). If the
// minimum is lower than *best_error, record it there and in coeff.
static void vadpcm_solve_edge(const double *restrict corr, double x0, double y0,
                              double x1, double y1,
                              double *restrict best_error,
                              double *restrict coeff) {
    // Writing the matrix as [k B^T; B A], the error along p + t v is a
    // quadratic in t,
    //
    // g(t) = g(0) + 2 t (v^T A p - B^T v) + t^2 v^T A v
    //
    // which is minimized at t = v^T (B - A p) / (v^T A v).
    double vx = x1 - x0, vy = y1 - y0;
    double apx = corr[2] * x0 + corr[4] * y0;
    double apy = corr[4] * x0 + corr[5] * y0;
    double num = vx * (corr[1] - apx) + vy * (corr[3] - apy);
    double den = vx * (corr[2] * vx + corr[4] * vy) +
                 vy * (corr[4] * vx + corr[5] * vy);
    double t;
    if (den > 0.0) {
        t = num / den;
        if (t < 0.0) {
            t = 0.0;
        } else if (t > 1.0) {
            t = 1.0;
        }
    } else {
        // The matrix is positive semidefinite, so v^T A v is never negative,
        // and zero means the error is linear along this edge. Take whichever
        // endpoint the gradient points toward.
        t = num > 0.0 ? 1.0 : 0.0;
    }
    double c0 = x0 + t * vx, c1 = y0 + t * vy;
    double error = vadpcm_eval_at(corr, c0, c1);
    if (error < *best_error) {
        *best_error = error;
        coeff[0] = c0;
        coeff[1] = c1;
    }
}

int vadpcm_solve_stable(const double *restrict corr, double *restrict coeff) {
    vadpcm_solve(corr, coeff);
    // A second-order predictor is stable exactly when its coefficients lie
    // within the triangle with vertices (2,-1), (-2,-1), and (0,1).
    if (coeff[1] >= -1.0 && coeff[0] + coeff[1] <= 1.0 &&
        coeff[1] - coeff[0] <= 1.0) {
        return 0;
    }
    // The error is a convex quadratic and the triangle is convex, so the
    // constrained minimum lies on an edge. Minimize along each of the three
    // edges and keep the best result.
    //
    // Note that the coefficients land on the boundary of the triangle, which
    // is marginally stable rather than strictly stable. Contracting the
    // triangle to leave a margin is not worthwhile: vadpcm_make_vectors
    // rounds the coefficients onto a 1/2048 grid, and near the (2,-1) vertex
    // the poles move as the square root of that perturbation, so no margin
    // survives quantization until it is large enough to cost real accuracy on
    // low-frequency signals.
    double best_error = DBL_MAX;
    coeff[0] = 0.0;
    coeff[1] = 0.0;
    vadpcm_solve_edge(corr, -2.0, -1.0, 2.0, -1.0, &best_error, coeff);
    vadpcm_solve_edge(corr, 2.0, -1.0, 0.0, 1.0, &best_error, coeff);
    vadpcm_solve_edge(corr, 0.0, 1.0, -2.0, -1.0, &best_error, coeff);
    return 1;
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
            vadpcm_solve_stable(pcorr[i], dcoeff);
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
