/**
 * @file fmath.c
 * @brief Fast math routines, optimized for 3D graphics calculations
 * @ingroup fastmath
 */
#include "fmath.h"
#include <string.h>
#include <stdint.h>

static const float pi_hi            = 3.14159274e+00f; // 0x1.921fb6p+01
static const float pi_lo            =-8.74227766e-08f; // -0x1.777a5cp-24
static const float half_pi_hi       =  1.57079637e+0f; //  0x1.921fb6p+0
// static const float half_pi_lo       = -4.37113883e-8f; // -0x1.777a5cp-25

float fm_sinf(float x) {
    // Approximation of sine to 5 ULP with Chebyshev polynomials
    // http://mooooo.ooo/chebyshev-sine-approximation/
    float p, s;

    // This function has been designed to operate in the [-π, +π] range, so
    // bring the argument there. This reduction using dragon_fmodf is not
    // very accurate for large numbers, so it will introduce more error compared
    // to the 5 ULP figure.
    x = fm_fmodf(x+pi_hi, 2*pi_hi) - pi_hi;
    s = x * x;
    p =         1.32729383e-10f;
    p = p * s - 2.33177868e-8f;
    p = p * s + 2.52223435e-6f;
    p = p * s - 1.73503853e-4f;
    p = p * s + 6.62087463e-3f;
    p = p * s - 1.01321176e-1f;
    return x * ((x - pi_hi) - pi_lo) * ((x + pi_hi) + pi_lo) * p;
}

float fm_cosf(float x) {
    return fm_sinf(half_pi_hi - x);
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

