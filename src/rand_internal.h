/**
 * @file rand_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Internal random number generation functions.
 * 
 * This module provides a simple and fast PRNG, only for internal libdragon use.
 * 
 * Whenever libdragon needs some random number, it should use this module instead
 * of the standard library functions. This is important because C rand() is
 * deterministic and if determinism is needed, users will expect all calls to
 * rand() to happen under their control. If libdragon itself started calling
 * rand() without the user knowing, it would break determinism.
 */
#ifndef LIBDRAGON_RAND_INTERNAL_H
#define LIBDRAGON_RAND_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

/** @brief Main PRNG (xorshift64*) */
uint64_t __xorshift64(_Atomic uint64_t *state);

/** @brief Fill a buffer with xorshift64 */
void __xorshift64_buffer(void *buf, size_t n, _Atomic uint64_t *state);

/** 
 * @brief Generate a random 64-bit unsigned integer.
 */
uint64_t __rand64(void);

/**
 * @brief Fill a buffer with random data.
 * 
 * @param buf Pointer to the buffer to fill.
 * @param len Length of the buffer in bytes.
 */
void __rand(void *buf, size_t len);

/** @brief Generate a random 8-bit unsigned integer. */
inline uint8_t  __rand8(void)  { return __rand64() & 0xFF; }
/** @brief Generate a random 16-bit unsigned integer. */
inline uint16_t __rand16(void) { return __rand64() & 0xFFFF; }
/** @brief Generate a random 32-bit unsigned integer. */
inline uint32_t __rand32(void) { return __rand64() & 0xFFFFFFFF; }
/** @brief Generate a random 32-bit floating point number in range [0, 1). */
inline float    __randf32(void) { return (float)__rand32() * (1.0f / 4294967296.0f); }

/** @brief Generate a random number in the range [0, n-1]. */
#define __randn(n)  (\
    __builtin_constant_p(n) ? \
        __rand32() % (n) : \
        ((uint64_t)__rand32() * (n)) >> 32)

#endif // LIBDRAGON_RAND_INTERNAL_H
