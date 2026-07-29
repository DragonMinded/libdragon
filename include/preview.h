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
 */

#ifndef __LIBDRAGON_PREVIEW_H
#define __LIBDRAGON_PREVIEW_H

// LIBDRAGON_BUILD_TIME is only defined while building libdragon,
// it allows use of preview APIs so that libdragon can be built at all.
#if defined(LIBDRAGON_PREVIEW) || defined(LIBDRAGON_BUILD_TIME)
#define HAVE_PREVIEW 1
#define ASSERT_PREVIEW
#define PREVIEW_API
#else
/// 0 or 1 depending on whether preview APIs are enabled
#define HAVE_PREVIEW 0
/// Error if preview APIs are disabled
#define ASSERT_PREVIEW _Pragma("GCC error \"This API is part of preview\"")
/// Error if preview APIs are disabled and a function with this attribute is used
#define PREVIEW_API __attribute__((error("This API is part of preview")))
#endif

#endif
