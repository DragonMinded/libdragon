/*
 * Preprocessor metaprogramming utils
 * 
 * This file contains some generic macros that are useful to implement
 * preprocessor metaprogramming, that is sometimes useful in providing
 * nice APIs.
 * 
 * They are not documented via doxygen because they are not part of the
 * libdragon public API, though they might be used in header files.
 */
#ifndef __LIBDRAGON_PPUTILS_H
#define __LIBDRAGON_PPUTILS_H

/// @cond

// FOREACH helpers. These macros are internally used by __CALL_FOREACH later.
#define __FE_0(_call, ...)
#define __FE_1(_call, x)       _call(x)
#define __FE_2(_call, x, ...)  _call(x) __FE_1(_call, __VA_ARGS__)
#define __FE_3(_call, x, ...)  _call(x) __FE_2(_call, __VA_ARGS__)
#define __FE_4(_call, x, ...)  _call(x) __FE_3(_call, __VA_ARGS__)
#define __FE_5(_call, x, ...)  _call(x) __FE_4(_call, __VA_ARGS__)
#define __FE_6(_call, x, ...)  _call(x) __FE_5(_call, __VA_ARGS__)
#define __FE_7(_call, x, ...)  _call(x) __FE_6(_call, __VA_ARGS__)
#define __FE_8(_call, x, ...)  _call(x) __FE_7(_call, __VA_ARGS__)
#define __FE_9(_call, x, ...)  _call(x) __FE_8(_call, __VA_ARGS__)
#define __FE_10(_call, x, ...) _call(x) __FE_9(_call, __VA_ARGS__)
#define __FE_11(_call, x, ...) _call(x) __FE_10(_call, __VA_ARGS__)
#define __FE_12(_call, x, ...) _call(x) __FE_11(_call, __VA_ARGS__)
#define __FE_13(_call, x, ...) _call(x) __FE_12(_call, __VA_ARGS__)
#define __FE_14(_call, x, ...) _call(x) __FE_13(_call, __VA_ARGS__)
#define __FE_15(_call, x, ...) _call(x) __FE_14(_call, __VA_ARGS__)
#define __FE_16(_call, x, ...) _call(x) __FE_15(_call, __VA_ARGS__)
#define __FE_17(_call, x, ...) _call(x) __FE_16(_call, __VA_ARGS__)
#define __FE_18(_call, x, ...) _call(x) __FE_17(_call, __VA_ARGS__)
#define __FE_19(_call, x, ...) _call(x) __FE_18(_call, __VA_ARGS__)
#define __FE_20(_call, x, ...) _call(x) __FE_19(_call, __VA_ARGS__)
#define __FE_21(_call, x, ...) _call(x) __FE_20(_call, __VA_ARGS__)
#define __FE_22(_call, x, ...) _call(x) __FE_21(_call, __VA_ARGS__)
#define __FE_23(_call, x, ...) _call(x) __FE_22(_call, __VA_ARGS__)
#define __FE_24(_call, x, ...) _call(x) __FE_23(_call, __VA_ARGS__)
#define __FE_25(_call, x, ...) _call(x) __FE_24(_call, __VA_ARGS__)
#define __FE_26(_call, x, ...) _call(x) __FE_25(_call, __VA_ARGS__)
#define __FE_27(_call, x, ...) _call(x) __FE_26(_call, __VA_ARGS__)
#define __FE_28(_call, x, ...) _call(x) __FE_27(_call, __VA_ARGS__)
#define __FE_29(_call, x, ...) _call(x) __FE_28(_call, __VA_ARGS__)
#define __FE_30(_call, x, ...) _call(x) __FE_29(_call, __VA_ARGS__)
#define __FE_31(_call, x, ...) _call(x) __FE_30(_call, __VA_ARGS__)

// Get the 33rd argument to this call. This is an useful building block for later macros
#define __GET_33RD_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, N, ...)  N

// Return the number of variadic arguments
#define __COUNT_VARARGS(...)     __GET_33RD_ARG("ignored", ##__VA_ARGS__, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

// Return 1 if there is at least one variadic argument, otherwise 0
#define __HAS_VARARGS(...)       __GET_33RD_ARG("ignored", ##__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0)

// Call macro fn for each variadic argument
#define __CALL_FOREACH(fn, ...)  __GET_33RD_ARG("ignored", ##__VA_ARGS__, __FE_31, __FE_30, __FE_29, __FE_28, __FE_27, __FE_26, __FE_25, __FE_24, __FE_23, __FE_22, __FE_21, __FE_20, __FE_19, __FE_18, __FE_17, __FE_16, __FE_15, __FE_14, __FE_13, __FE_12, __FE_11, __FE_10, __FE_9, __FE_8, __FE_7, __FE_6, __FE_5, __FE_4, __FE_3, __FE_2, __FE_1, __FE_0)(fn, ##__VA_ARGS__)

// Preprocessor token paste
#define __PPCAT2(n,x) n ## x
#define __PPCAT(n,x) __PPCAT2(n,x)

// __CALL_FOREACH_BIS. Like __CALL_FOREACH, but it allows to be called without the expansion
// of a __CALL_FOREACH.
#define __FEB_0(_call, ...)
#define __FEB_1(_call, x)       _call(x)
#define __FEB_2(_call, x, ...)  _call(x) __FEB_1(_call, __VA_ARGS__)
#define __FEB_3(_call, x, ...)  _call(x) __FEB_2(_call, __VA_ARGS__)
#define __FEB_4(_call, x, ...)  _call(x) __FEB_3(_call, __VA_ARGS__)
#define __FEB_5(_call, x, ...)  _call(x) __FEB_4(_call, __VA_ARGS__)
#define __FEB_6(_call, x, ...)  _call(x) __FEB_5(_call, __VA_ARGS__)
#define __FEB_7(_call, x, ...)  _call(x) __FEB_6(_call, __VA_ARGS__)
#define __FEB_8(_call, x, ...)  _call(x) __FEB_7(_call, __VA_ARGS__)
#define __FEB_9(_call, x, ...)  _call(x) __FEB_8(_call, __VA_ARGS__)
#define __FEB_10(_call, x, ...) _call(x) __FEB_9(_call, __VA_ARGS__)
#define __FEB_11(_call, x, ...) _call(x) __FEB_10(_call, __VA_ARGS__)
#define __FEB_12(_call, x, ...) _call(x) __FEB_11(_call, __VA_ARGS__)
#define __FEB_13(_call, x, ...) _call(x) __FEB_12(_call, __VA_ARGS__)
#define __FEB_14(_call, x, ...) _call(x) __FEB_13(_call, __VA_ARGS__)
#define __FEB_15(_call, x, ...) _call(x) __FEB_14(_call, __VA_ARGS__)
#define __FEB_16(_call, x, ...) _call(x) __FEB_15(_call, __VA_ARGS__)
#define __FEB_17(_call, x, ...) _call(x) __FEB_16(_call, __VA_ARGS__)
#define __FEB_18(_call, x, ...) _call(x) __FEB_17(_call, __VA_ARGS__)
#define __FEB_19(_call, x, ...) _call(x) __FEB_18(_call, __VA_ARGS__)
#define __FEB_20(_call, x, ...) _call(x) __FEB_19(_call, __VA_ARGS__)
#define __FEB_21(_call, x, ...) _call(x) __FEB_20(_call, __VA_ARGS__)
#define __FEB_22(_call, x, ...) _call(x) __FEB_21(_call, __VA_ARGS__)
#define __FEB_23(_call, x, ...) _call(x) __FEB_22(_call, __VA_ARGS__)
#define __FEB_24(_call, x, ...) _call(x) __FEB_23(_call, __VA_ARGS__)
#define __FEB_25(_call, x, ...) _call(x) __FEB_24(_call, __VA_ARGS__)
#define __FEB_26(_call, x, ...) _call(x) __FEB_25(_call, __VA_ARGS__)
#define __FEB_27(_call, x, ...) _call(x) __FEB_26(_call, __VA_ARGS__)
#define __FEB_28(_call, x, ...) _call(x) __FEB_27(_call, __VA_ARGS__)
#define __FEB_29(_call, x, ...) _call(x) __FEB_28(_call, __VA_ARGS__)
#define __FEB_30(_call, x, ...) _call(x) __FEB_29(_call, __VA_ARGS__)
#define __FEB_31(_call, x, ...) _call(x) __FEB_30(_call, __VA_ARGS__)
#define __CALL_FOREACH_BIS(fn, ...)  __GET_33RD_ARG("ignored", ##__VA_ARGS__, __FEB_31, __FEB_30, __FEB_29, __FEB_28, __FEB_27, __FEB_26, __FEB_25, __FEB_24, __FEB_23, __FEB_22, __FEB_21, __FEB_20, __FEB_19, __FEB_18, __FEB_17, __FEB_16, __FEB_15, __FEB_14, __FEB_13, __FEB_12, __FEB_11, __FEB_10, __FEB_9, __FEB_8, __FEB_7, __FEB_6, __FEB_5, __FEB_4, __FEB_3, __FEB_2, __FEB_1, __FEB_0)(fn, ##__VA_ARGS__)

// Check that GNU extensions are active and macros work correctly. Specifically
// we require the extension that allows ##__VA_ARGS__ to elide the previous comma
// when no variadic arguments are specified.
#if __COUNT_VARARGS() != 0
#error GNU extensions are required -- please specify a -std=gnuXX/gnu++XX option to the compiler
#endif

/// @endcond

#endif
