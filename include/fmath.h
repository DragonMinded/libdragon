/**
 * @file fmath.h
 * @brief Fast math routines, optimized for 3D graphics calculations
 * @ingroup fastmath
 */

/**
 * @defgroup fastmath Fast math routines
 * @ingroup libdragon
 * @brief Fast math routines, optimized for 3D graphics calculations
 *
 * This module collects mathematical functions operating a single-precision
 * floating point numbers (float) that are useful in the context of 3D
 * graphics algorithm. The provided algorithms have prototypes that are
 * identical to the C standard ones (provided by libm), but their implementation
 * has been optimized in a way that is normally useful in the context
 * of graphics programming in games. In particular, compared to the C standard:
 *
 *  * Infinites are not handled, the resulting value is undefined.
 *  * Signed zeros are not respected.
 *  * Denormals are not handled (also because the VR3000 is unable to produce them,
 *    and it is configured to flush them to zero, see cop1.c).
 *  * errno is never generated or modified.
 *  * The numerical error is much higher than 1 ULP, but still much smaller than
 *    that introduced by converting floating point values into the fixed point
 *    representation required by RSP. Obviously, errors in numbers accumulate
 *    over multiple calculations, but the idea is that they should still stay
 *    small enough to rarely affect what it is being sent to RSP.
 *
 * The first four compromises above are similar and in-line with those that
 * are usually accepted by programmers that compile their floating point code
 * using `-ffast-math`.
 *
 * As for the numerical error, there is no single good trade-off that can be
 * generally taken when deciding how much we want to approximate an inverse
 * square root or a trigonometric function. Using the general understanding that
 * most 3D games on N64 are fill-rate limited rather than CPU or RSP limited,
 * this library stays on the side of spending more CPU cycles more than the
 * most basic version, while still offering a couple of orders of magnitudes
 * of speed improvement over the standard C versions (that are fully accurate
 * for all inputs).
 *
 * All the functions defined by this library prefixed with "fm_" (eg: #fm_sinf).
 * It is possible to define the preprocess macro LIBDRAGON_FAST_MATH to
 * additionally define macros that override the standard library functions,
 * so that calling `sinf(x)` will actually invoke `fm_sinf(x)`.
 * 
 * The following C99 functions have been tested and the default implementation
 * is already very good (eg: they are intrinsified):
 * 
 *   * fabsf
 *   * copysignf
 *   * sqrtf (uses the sqrt.s opcode). Also 1.0f/sqrtf(x) is fast enough not to
 *     worry about using a fast inverse square root. 
 * 
 */

#ifndef __LIBDRAGON_FMATH_H
#define __LIBDRAGON_FMATH_H

#include <math.h>
#include <string.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Reinterpret the bits composing a float as a int32. 
 *
 * This version is type-punning safe and produces optimal code when optimizing. 
 **/
#define BITCAST_F2I(f) ({ int32_t i; memcpy(&i, &f, 4); i; })

/** @brief Reinterpret the bits composing a int32 as a float.
 *
 * This version is type-punning safe and produces optimal code when optimizing. 
 **/
#define BITCAST_I2F(i) ({ float f;   memcpy(&f, &i, 4); f; })

/**
 * @brief Faster version of truncf
 * 
 * Optimized version using the MIPS trunc.w.s instruction.
 */
static inline float fm_truncf(float x) {
    /* Notice that trunc.w.s is also emitted by the compiler when casting a
     * float to int, but in this case we want a floating point result anyway,
     * so it's useless to go back and forth a GPR. */
    float yint, y;
    __asm ("trunc.w.s  %0,%1" : "=f"(yint) : "f"(x));
    __asm ("cvt.s.w  %0,%1" : "=f"(y) : "f"(yint));
    return y;
}

/**
 * @brief Faster version of floorf
 * 
 * Optimized version using the MIPS ceil.w.s instruction.
 */
static inline float fm_ceilf(float x) {
    float yint, y;
    __asm ("ceil.w.s  %0,%1" : "=f"(yint) : "f"(x));
    __asm ("cvt.s.w  %0,%1" : "=f"(y) : "f"(yint));
    return y;
}

/**
 * @brief Faster version of floorf
 * 
 * Optimized version using the MIPS trunc.w.s instruction.
 */
static inline float fm_floorf(float x) {
    float y = fm_truncf(x);
    // After truncation, correct the negative numbers
    if (x < 0) y -= 1.0f;
    return y;
}

/**
 * @brief Faster version of fmodf
 * 
 * Optimized version of fmodf, which returns accurate results in case
 * of small magnitudes (x <= 1e6). Do not use this version if you need
 * accurate module of very large numbers.
 */
static inline float fm_fmodf(float x, float y) {
    return x - fm_floorf(x * (1.0f / y)) * y;
}

/**
 * @brief Faster version of sinf.
 * 
 * This function computes a very accurate approximation of the sine of a floating
 * point number, as long as the argument as a small magnitude. Do not use this
 * function with very large (positive or negative) numbers as the accuracy
 * decreases. Normally, it is not necessary in graphics programming to compute
 * trigonometric functions on angles of unbounded magnitude.
 * 
 * The functions runs in about ~50 ticks, versus ~800 ticks of the newlib
 * version. The accuracy in the range [-π, +π] is within 5 ULP of the correct
 * result, but the argument reduction to bring the argument in that range
 * introduces errors which increase with the magnitude of the operand.
 */
float fm_sinf(float x);

/**
 * @brief Faster version of sinf, with tunable approximation level
 * 
 * This function is similar to #fm_sinf, but allows to further speedup
 * the approximation by reducing the number of calculated terms. #fm_sinf
 * in fact is pretty accurate (~ 5 ULP) but some times much less precision
 * is required. 
 * 
 * The approximation level is a number between 0 and 5, where 0 is the
 * most accurate version (identical to #fm_sinf) and 5 is the fastest one.
 * We do not give mathematical guarantees on the accuracy of the approximation,
 * and we suggest on holistic approach (try and see if it works for you).
 * 
 * This function is suggested in all cases in which you need to visually
 * reproduce a "sinewave" effect, but you do not care about the exact numbers
 * behind it. For trigonemetric formulas that includes a sine (eg: matrix
 * rotations), it is suggested to use #fm_sinf instead.
 * 
 * @param x       The angle in radians
 * @param approx  The approximation level, between 0 and 5
 */
float fm_sinf_approx(float x, int approx);

/**
 * @brief Faster version of cosf.
 * 
 * @see #fm_sinf for considerations on why and how to use this functions
 *      instead of the standard sinf.
 */
float fm_cosf(float x);

/**
 * @brief Faster version of sincosf.
 * 
 * Similar to #fm_sinf and #fm_cosf, but calculates both sinf and cosf
 * for the same angle.
 * 
 * @param x    The angle in radians
 * @param sin  The pointer to the variable that will contain the sine
 * @param cos  The pointer to the variable that will contain the cosine
 */
void fm_sincosf(float x, float *sin, float *cos);

/**
 * @brief Faster version of atan2f.
 * 
 * Given a point (x,y), return the angle in radians that the vector (x,y)
 * forms with the X axis. This is the same of arctan(y/x).
 * 
 * This function runs in about ~XX ticks, versus ~YY ticks of the newlib
 * version. The maximum measured error is ~6.14e-4, which is usually more
 * than enough in the context of angles.
 */
float fm_atan2f(float y, float x);

#ifdef LIBDRAGON_FAST_MATH
    #define truncf(x)     fm_truncf(x)
    #define floorf(x)     fm_floorf(x)
    #define ceilf(x)      fm_ceilf(x)

    // The following macros contain a special-case: when called with constant
    // arguments, they fall back to the standard math.h version. This allows
    // the compiler to compute an accurate result at compile time.
    // For instance, if we find `sinf(sqrtf(2.0f))` in the source code,
    // we expect that to be resolved at compile-time with the maximum accuracy,
    // so it's important to generate code that calls the standard C function
    // which is then intrinsified by the compiler.
    #define fmodf(x, y)     ((__builtin_constant_p(x) && __builtin_constant_p(y)) ? fmodf(x,y) : fm_fmodf(x,y))
    #define sinf(x)         (__builtin_constant_p(x) ? sinf(x) : fm_sinf(x))
    #define cosf(x)         (__builtin_constant_p(x) ? cosf(x) : fm_cosf(x))
    #define sincosf(x,s,c)  (__builtin_constant_p(x) ? sincosf(x,s,c) : fm_sincosf(x,s,c))
    #define atan2f(y, x)    ((__builtin_constant_p(x) && __builtin_constant_p(y)) ? atan2f(y, x) : fm_atan2f(y, x))
#endif

#ifdef __cplusplus
}
#endif

#endif

