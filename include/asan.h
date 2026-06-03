/**
 * @file asan.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Emulator-assisted heap memory sanitizer (XASAN)
 * @ingroup asan
 * @defgroup asan asan
 *
 * XASAN is an emulator-assisted memory access sanitizer for the VR4300.
 * Enable it at link time with `N64_ASAN=1` in n64.mk.
 *
 * When disabled (default), the functions resolve to weak no-op stubs with
 * no runtime overhead beyond a call when actually used.
 *
 * @note Global object registration (`.xasan_globals`) is not implemented yet.
 */

#ifndef LIBDRAGON_ASAN_H
#define LIBDRAGON_ASAN_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mark a memory region as user-poisoned (inaccessible)
 *
 * @param ptr   Base address (rounded down to 8 bytes)
 * @param size  Size in bytes (rounded up to 8 bytes)
 */
void asan_poison(void *ptr, size_t size);

/**
 * @brief Mark a memory region as accessible
 *
 * @param ptr   Base address (rounded down to 8 bytes)
 * @param size  Size in bytes (rounded up to 8 bytes)
 */
void asan_unpoison(void *ptr, size_t size);

/**
 * @brief Return whether XASAN is linked in and active
 *
 * When the app is linked with `N64_ASAN=1` and the emulator supports XASAN,
 * this returns true after startup initialization.
 */
bool asan_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBDRAGON_ASAN_H */
