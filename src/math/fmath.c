/**
 * @file fmath.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Fast math routines, optimized for 3D graphics calculations
 * @ingroup fastmath
 */
#include "fmath.h"
#include "debug.h"
#include "utils.h"
#include <string.h>
#include <stdint.h>

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
    if (LIKELY(--approx < 0)) p +=   1.3291330536e-10f, p *= s;
    if (LIKELY(--approx < 0)) p += - 2.3317808128e-8f,  p *= s;
    if (LIKELY(--approx < 0)) p +=   2.5222900603e-6f,  p *= s;
    if (LIKELY(--approx < 0)) p += - 1.7350520647e-4f,  p *= s;
    if (LIKELY(--approx < 0)) p +=   6.6208802163e-3f,  p *= s;
    if (LIKELY(--approx < 0)) p += - 1.0132116824e-1f;
    x = x * ((x - pi_hi) - pi_lo) * ((x + pi_hi) + pi_lo) * p;
    return x;
}

float fm_sinf_approx(float x, int approx) {
    // sinf_approx has been designed to operate in the [-π, +π] range, so
    // bring the argument there. This reduction using fm_fmodf is not
    // very accurate for large numbers, so it will introduce more error compared
    // to the 5 ULP figure.
    x = fm_wrapf(x+pi_hi, 2*pi_hi) - pi_hi;
    x = sinf_approx(x, approx);
    return x;
}

float fm_sinf(float x) {
    return fm_sinf_approx(x, 0);
}

float fm_cosf(float x) {
    return fm_sinf(half_pi_hi - x);
}

void fm_sincosf(float x, float *sin, float *cos) {
    x = fm_wrapf(x+pi_hi, 2*pi_hi) - pi_hi;
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

float fm_expf(float x){
    // Approximation of exp(x) with a relative error <3%.
    // This is several times faster than exp(x). The implementation uses a
    // method by Nicole Schraudolph.
    // Note that there's no bounds check and x will overflow if x is not in ~(-85,85) range
    // Source: https://gist.github.com/jrade/293a73f89dfef51da6522428c857802d
    const float a = (1 << 23) / 0.69314718f;
    const float b = (1 << 23) * (127 - 0.043677448f);
    x = a * x + b;

    uint32_t i = (uint32_t)x;
    return BITCAST_I2F(i);
}

float fm_lerp_angle(float a, float b, float t)
{
    float diff = fm_fmodf((b - a), FM_PI*2);
    float dist = fm_fmodf(diff*2, FM_PI*2) - diff;
    return a + dist * t;
}

float fm_wrap_angle(float angle)
{
    return fm_wrapf(angle, FM_PI*2);
}

extern inline float fm_lerp(float a, float b, float t);
