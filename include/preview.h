/**
 * @file preview.h
 * @brief Preview APIs
 * @ingroup preview
 */

/**
 * @defgroup preview Preview APIs
 * @ingroup libdragon
 * @brief Preview APIs may change without notice
 *
 * libdragon's APIs are either stable or preview.
 * Stable APIs are guaranteed to remain working without breakage.
 * Preview APIs may change without notice, potentially causing project breakage.
 *
 * Preview APIs are locked by default: using one is a compile time error, unless
 * the project opts in by setting `LIBDRAGON_PREVIEW=1` in its Makefile, before
 * including n64.mk.
 *
 * There are two ways of marking an API as preview:
 *
 * * #ASSERT_PREVIEW locks a whole header. This is only meant for the few
 *   headers that the user is expected to include explicitly (eg: GL/gl.h).
 * * #PREVIEW_API and #PREVIEW_SYM lock a single entry point. This is the common
 *   case: most headers are pulled in by libdragon.h, so they must stay
 *   includable even in stable projects.
 *
 * @note libdragon itself is always built with preview enabled, so a single
 *       libdragon.a serves both stable and preview projects. This means that a
 *       preview API must never change the layout of a type, the body of an
 *       inline function or the value of a macro that stable code can also see,
 *       otherwise the library and the project would silently disagree on the
 *       ABI. Stable and preview builds must only differ in *which* declarations
 *       are usable, never in *how* they are defined. For the same reason, a
 *       preview type must never appear in a stable structure nor in the
 *       signature of a stable function, as that would make the stable API
 *       unstable as well.
 */

#ifndef __LIBDRAGON_PREVIEW_H
#define __LIBDRAGON_PREVIEW_H

/// @cond
#define __PREVIEW_PRAGMA_RAW(x) _Pragma(#x)
// Extra indirection level, so that macros in the argument are expanded before
// being stringified into the pragma.
#define __PREVIEW_PRAGMA(x) __PREVIEW_PRAGMA_RAW(x)
/// @endcond

/** @brief Diagnostic message shown when a preview API is used by mistake */
#define __PREVIEW_MSG "libdragon preview API: set LIBDRAGON_PREVIEW=1 in your Makefile (before including n64.mk) to use it"

// LIBDRAGON_BUILD_TIME is only defined while building libdragon,
// it allows use of preview APIs so that libdragon can be built at all.
#if defined(LIBDRAGON_PREVIEW) || defined(LIBDRAGON_BUILD_TIME)
#define HAVE_PREVIEW 1
#define ASSERT_PREVIEW
#define PREVIEW_API
#define PREVIEW_SYM(name)
#else
/// 0 or 1 depending on whether preview APIs are enabled
#define HAVE_PREVIEW 0

/**
 * @brief Error out if preview APIs are disabled
 *
 * Put this at the top of a header to make its inclusion an error. Only use it
 * for headers that are not reachable from libdragon.h, otherwise every stable
 * project would fail to build. Mark single entry points with #PREVIEW_API or
 * #PREVIEW_SYM instead.
 */
#define ASSERT_PREVIEW __PREVIEW_PRAGMA(GCC error __PREVIEW_MSG)

/**
 * @brief Mark a function as preview
 *
 * Calling a function with this attribute is an error if preview APIs are
 * disabled. The @c deprecated attribute is there as a backstop: the @c error
 * attribute is only reported for calls that survive optimization, so it misses
 * eg. taking the address of the function, or calling it from dead code.
 */
#define PREVIEW_API __attribute__((error(__PREVIEW_MSG), deprecated(__PREVIEW_MSG)))

/**
 * @brief Mark a non-function symbol as preview
 *
 * #PREVIEW_API only works on functions, so use this for types, enumerators,
 * global variables and macros. Put it *after* the declaration, as it makes any
 * later mention of the name an error. For a macro, @c #undef it first,
 * otherwise the name would be expanded before being poisoned.
 */
#define PREVIEW_SYM(name) __PREVIEW_PRAGMA_RAW(GCC poison name)
#endif

#endif
