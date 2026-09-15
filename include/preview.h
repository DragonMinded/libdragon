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
 * the project opts in by setting `LIBDRAGON_PREVIEW` in its Makefile, before
 * including n64.mk:
 *
 * * `0` — using a preview API is a hard error (default)
 * * `1` — preview APIs are usable but each use produces a warning
 * * `2` — preview APIs are fully unlocked (no diagnostics)
 *
 * There are three ways of marking an API as preview:
 *
 * * #LIBDRAGON_PREVIEW_HEADER locks a whole header. This is only meant for the
 *   few headers that the user is expected to include explicitly (eg: GL/gl.h).
 * * #LIBDRAGON_PREVIEW_API locks a function.
 * * #LIBDRAGON_PREVIEW_SYM locks non-function
 *   declarations. Most headers are pulled in by libdragon.h, so they must stay
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
 *       unstable as well. New fields on a stable struct must stay in the
 *       layout for everyone and be locked with #LIBDRAGON_PREVIEW_SYM: hiding
 *       them behind "#if" would change @c sizeof between the library and a
 *       stable project.
 */

#ifndef __LIBDRAGON_PREVIEW_H
#define __LIBDRAGON_PREVIEW_H

/**
 * @def LIBDRAGON_HAVE_PREVIEW
 * @brief 0 or 1 depending on whether preview APIs are enabled
 *
 * True for both `LIBDRAGON_PREVIEW=1` (warning mode) and
 * `LIBDRAGON_PREVIEW=2` (fully unlocked).
 */

/**
 * @def LIBDRAGON_PREVIEW_HEADER
 * @brief Gate a whole header when preview APIs are disabled
 *
 * Put this at the top of a header to make its inclusion an error (or a warning
 * in warning mode). Only use it for headers that are not reachable from
 * libdragon.h, otherwise every stable project would fail to build. Mark single
 * entry points with #LIBDRAGON_PREVIEW_API or #LIBDRAGON_PREVIEW_SYM instead.
 */

/**
 * @def LIBDRAGON_PREVIEW_API
 * @brief Mark a function as preview
 *
 * Calling a function with this attribute is an error if preview APIs are
 * locked, a warning in warning mode, and a no-op when preview is fully
 * unlocked. The @c deprecated attribute is there as a backstop: the @c error
 * attribute is only reported for calls that survive optimization, so on its own
 * it would miss eg. taking the address of the function, or calling it from dead
 * code.
 */

/**
 * @def LIBDRAGON_PREVIEW_SYM
 * @brief Mark a field, enumerator or global as preview
 *
 * Use this like a GCC attribute on the declaration. The declaration stays in
 * the header (so struct layouts do not change between stable and preview), but
 * using it is an error when preview APIs are locked, or a warning in warning
 * mode.
 *
 * @code{.c}
 * struct bar {
 *     int a;
 *     LIBDRAGON_PREVIEW_SYM int b;   // always present in the layout
 * };
 * enum { OLD = 0, NEW LIBDRAGON_PREVIEW_SYM = 1 };
 * @endcode
 *
 * Do **not** put this on a typedef that is still mentioned later in the same
 * header (eg: in function signatures): @c unavailable would then make the
 * header itself fail to compile under stable.
 *
 * @note On GCC, designated initializers (@c .field = ...) of an unavailable
 *       field are currently not diagnosed (Clang does diagnose them).
 * @note In warning mode this expands to @c deprecated rather than
 *       @c unavailable, because the latter is always a hard error.
 */

/// @cond
#define __LIBDRAGON_PREVIEW_PRAGMA_RAW(x) _Pragma(#x)
// Extra indirection level, so that macros in the argument are expanded before
// being stringified into the pragma.
#define __LIBDRAGON_PREVIEW_PRAGMA(x) __LIBDRAGON_PREVIEW_PRAGMA_RAW(x)

// Diagnostic message shown when a preview API is used by mistake
#define __LIBDRAGON_PREVIEW_MSG "libdragon preview API: set LIBDRAGON_PREVIEW=1 (or 2) in your Makefile (before including n64.mk) to use it"
#define __LIBDRAGON_PREVIEW_MSG_WARN "libdragon preview API: set LIBDRAGON_PREVIEW=2 in your Makefile (before including n64.mk) to silence this"
/// @endcond

// __LIBDRAGON_INTERNAL_BUILD is only defined while building libdragon itself,
// it allows use of preview APIs so that libdragon can be built at all.
#if defined(__LIBDRAGON_INTERNAL_BUILD)
#define LIBDRAGON_HAVE_PREVIEW 1
#define LIBDRAGON_PREVIEW_HEADER
#define LIBDRAGON_PREVIEW_API
#define LIBDRAGON_PREVIEW_SYM
#elif defined(LIBDRAGON_PREVIEW) && (LIBDRAGON_PREVIEW) == 1
#define LIBDRAGON_HAVE_PREVIEW 1
// Whole-header gate: declare a deprecated unused object so including the
// header warns, without tripping -Werror (n64.mk exempts deprecated-declarations).
// Installed headers live on the compiler's system include path, where GCC
// normally suppresses diagnostics — temporarily enable -Wsystem-headers so the
// warning still fires. __LINE__ keeps the names unique across gated headers.
#define __LIBDRAGON_PREVIEW_HEADER_PASTE2(a, b) a##b
#define __LIBDRAGON_PREVIEW_HEADER_PASTE(a, b) __LIBDRAGON_PREVIEW_HEADER_PASTE2(a, b)
#define __LIBDRAGON_PREVIEW_HEADER_IMPL(uniq) \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic warning \"-Wsystem-headers\"") \
    typedef int __attribute__((deprecated(__LIBDRAGON_PREVIEW_MSG_WARN))) \
        __LIBDRAGON_PREVIEW_HEADER_PASTE(__libdragon_ph_t, uniq); \
    static __LIBDRAGON_PREVIEW_HEADER_PASTE(__libdragon_ph_t, uniq) \
        __LIBDRAGON_PREVIEW_HEADER_PASTE(__libdragon_ph_v, uniq) __attribute__((unused)); \
    _Pragma("GCC diagnostic pop")
#define LIBDRAGON_PREVIEW_HEADER __LIBDRAGON_PREVIEW_HEADER_IMPL(__LINE__)
#define LIBDRAGON_PREVIEW_API __attribute__((deprecated(__LIBDRAGON_PREVIEW_MSG_WARN)))
#define LIBDRAGON_PREVIEW_SYM __attribute__((deprecated(__LIBDRAGON_PREVIEW_MSG_WARN)))
#elif defined(LIBDRAGON_PREVIEW) && (LIBDRAGON_PREVIEW) != 0
#define LIBDRAGON_HAVE_PREVIEW 1
#define LIBDRAGON_PREVIEW_HEADER
#define LIBDRAGON_PREVIEW_API
#define LIBDRAGON_PREVIEW_SYM
#else
#define LIBDRAGON_HAVE_PREVIEW 0
#define LIBDRAGON_PREVIEW_HEADER __LIBDRAGON_PREVIEW_PRAGMA(GCC error __LIBDRAGON_PREVIEW_MSG)
#define LIBDRAGON_PREVIEW_API __attribute__((error(__LIBDRAGON_PREVIEW_MSG), deprecated(__LIBDRAGON_PREVIEW_MSG)))
#define LIBDRAGON_PREVIEW_SYM __attribute__((unavailable(__LIBDRAGON_PREVIEW_MSG)))
#endif

#endif
