/**
 * @file fmath.c
 * @brief Fast math routines, optimized for 3D graphics calculations
 * @ingroup fastmath
 */
#include "fmath.h"
#include "debug.h"
#include <string.h>
#include <stdint.h>

/// Mark a branch as likely to be taken
#define LIKELY(x)       __builtin_expect((x),1)

static const float pi_hi            = 3.14159274e+00f; // 0x1.921fb6p+01
static const float pi_lo            =-8.74227766e-08f; // -0x1.777a5cp-24
static const float half_pi_hi       =  1.57079637e+0f; //  0x1.921fb6p+0
// static const float half_pi_lo       = -4.37113883e-8f; // -0x1.777a5cp-25

__attribute__((noinline))
static float sinf_approx(float x, int approx) {
    // Approximation of sine to 5 ULP with Chebyshev polynomials
    // http://mooooo.ooo/chebyshev-sine-approximation/
    float p, s;
    assertf(approx >= 0 && approx <= 5, "invalid approximation level %d", approx);

    p = 0;
    s = x * x;
    // Execute only a portion of the series, depending on the approximation level.
    // This generate the most efficient code among similar approaches.
    if (LIKELY(--approx < 0)) p +=   1.32729383e-10f, p *= s;
    if (LIKELY(--approx < 0)) p += - 2.33177868e-8f,  p *= s;
    if (LIKELY(--approx < 0)) p +=   2.52223435e-6f,  p *= s;
    if (LIKELY(--approx < 0)) p += - 1.73503853e-4f,  p *= s;
    if (LIKELY(--approx < 0)) p +=   6.62087463e-3f,  p *= s;
    if (LIKELY(--approx < 0)) p += - 1.01321176e-1f;
    return x * ((x - pi_hi) - pi_lo) * ((x + pi_hi) + pi_lo) * p;   
}

float fm_sinf_approx(float x, int approx) {
    // sinf_approx has been designed to operate in the [-π, +π] range, so
    // bring the argument there. This reduction using fm_fmodf is not
    // very accurate for large numbers, so it will introduce more error compared
    // to the 5 ULP figure.
    x = fm_fmodf(x+pi_hi, 2*pi_hi) - pi_hi;
    return sinf_approx(x, approx);
}

float fm_sinf(float x) {
    return fm_sinf_approx(x, 0);
}

float fm_cosf(float x) {
    return fm_sinf(half_pi_hi - x);
}

void fm_sincosf(float x, float *sin, float *cos) {
    x = fm_fmodf(x+pi_hi, 2*pi_hi) - pi_hi;
    float sy = sinf_approx(x, 0);
    float cy = sqrtf(1.0f - sy * sy);
    if (fabsf(x) > half_pi_hi) {
        cy = -cy;
    }
    *sin = sy;
    *cos = cy;
}

float fm_atan2f(float y, float x) {
    // Approximation of atan2f using a polynomial minmax approximation in [0,1]
    // calculated via the Remez algorithm (https://math.stackexchange.com/a/1105038).
    // The reported error is 6.14e-4, so it's precise for at least three decimal
    // digits which is usually more than enough for angles.
    float ay = fabsf(y);
    float ax = fabsf(x);
    float a = (ay < ax) ? ay/ax : ax/ay;
    float s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax)
        r = half_pi_hi - r;
    if (BITCAST_F2I(x) < 0) r = pi_hi - r;
    return copysignf(r, y);
}

