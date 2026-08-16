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
 * * #LIBDRAGON_PREVIEW_HEADER locks a whole header. This is only meant for the
 *   few headers that the user is expected to include explicitly (eg: GL/gl.h).
 * * #LIBDRAGON_PREVIEW_API and #LIBDRAGON_PREVIEW_SYM lock a single entry
 *   point. This is the common case: most headers are pulled in by libdragon.h,
 *   so they must stay includable even in stable projects.
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

/**
 * @def LIBDRAGON_HAVE_PREVIEW
 * @brief 0 or 1 depending on whether preview APIs are enabled
 */

/**
 * @def LIBDRAGON_PREVIEW_HEADER
 * @brief Error out if preview APIs are disabled
 *
 * Put this at the top of a header to make its inclusion an error. Only use it
 * for headers that are not reachable from libdragon.h, otherwise every stable
 * project would fail to build. Mark single entry points with
 * #LIBDRAGON_PREVIEW_API or #LIBDRAGON_PREVIEW_SYM instead.
 */

/**
 * @def LIBDRAGON_PREVIEW_API
 * @brief Mark a function as preview
 *
 * Calling a function with this attribute is an error if preview APIs are
 * disabled. The @c deprecated attribute is there as a backstop: the @c error
 * attribute is only reported for calls that survive optimization, so on its own
 * it would miss eg. taking the address of the function, or calling it from dead
 * code.
 */

/**
 * @def LIBDRAGON_PREVIEW_SYM
 * @brief Mark a non-function symbol as preview
 *
 * #LIBDRAGON_PREVIEW_API only works on functions, so use this for types,
 * enumerators, global variables and macros. Put it *after* the declaration, as
 * it makes any later mention of the name an error. For a macro, @c \#undef it
 * first, otherwise the name would be expanded before being poisoned.
 */

/// @cond
#define __LIBDRAGON_PREVIEW_PRAGMA_RAW(x) _Pragma(#x)
// Extra indirection level, so that macros in the argument are expanded before
// being stringified into the pragma.
#define __LIBDRAGON_PREVIEW_PRAGMA(x) __LIBDRAGON_PREVIEW_PRAGMA_RAW(x)

// Diagnostic message shown when a preview API is used by mistake
#define __LIBDRAGON_PREVIEW_MSG "libdragon preview API: set LIBDRAGON_PREVIEW=1 in your Makefile (before including n64.mk) to use it"
/// @endcond

// __LIBDRAGON_INTERNAL_BUILD is only defined while building libdragon itself,
// it allows use of preview APIs so that libdragon can be built at all.
#if defined(LIBDRAGON_PREVIEW) || defined(__LIBDRAGON_INTERNAL_BUILD)
#define LIBDRAGON_HAVE_PREVIEW 1
#define LIBDRAGON_PREVIEW_HEADER
#define LIBDRAGON_PREVIEW_API
#define LIBDRAGON_PREVIEW_SYM(name)
#else
#define LIBDRAGON_HAVE_PREVIEW 0
#define LIBDRAGON_PREVIEW_HEADER __LIBDRAGON_PREVIEW_PRAGMA(GCC error __LIBDRAGON_PREVIEW_MSG)
#define LIBDRAGON_PREVIEW_API __attribute__((error(__LIBDRAGON_PREVIEW_MSG), deprecated(__LIBDRAGON_PREVIEW_MSG)))
#define LIBDRAGON_PREVIEW_SYM(name) __LIBDRAGON_PREVIEW_PRAGMA_RAW(GCC poison name)
#endif

#endif
