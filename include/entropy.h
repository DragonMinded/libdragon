/**
 * @file entropy.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Header file for entropy source functions.
 * 
 * This module implements an entropy source for non-deterministic random
 * number generation. It is implemented via two functions:
 * 
 * - `getentropy` (BSD style): fills a buffer with non-deterministic random data.
 * - `getentropy32`: returns a 32-bit non-deterministic random number.
 * 
 * Notice that this entropy source is not a cryptographic one. If you need a
 * cryptographically secure random number generator, you should fetch some
 * entropy from this module and then use a cryptographic PRNG like ChaCha20
 * or SipHash to generate the random numbers.
 * 
 * @note This is a low-level module for advance use cases. If you just
 * need some random numbers for your game, use this module just to seed the
 * standard C library `rand()` function and then use `rand()` in your game
 * engine:
 * 
 * \code{c}
 *      // Seed the C library random number generator in an unpredictable way
 *      // using the entropy source, so that the random numbers generated will
 *      // be different on each boot.
 *      srand(getentropy32());
 * 
 *      // Now you can use rand() to generate random numbers in your game.
 *      int random_stage = rand() % NUM_STAGES;
 * \endcode
 */
#ifndef LIBDRAGON_ENTROPY_H
#define LIBDRAGON_ENTROPY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generate an array of unpredictable random numbers
 * 
 * This function can be used to generate an array of random data. The function
 * is guaranteed to return good quality random numbers of basically
 * unlimited length. The function is automatically seeded by entropy collected
 * during the boot process (during IPL3) so it always returns different
 * numbers after each boot on hardware. On each emulator, though, the
 * generate numbers will be consistent.
 * 
 * The data returned by this function is not cryptographically secure, but
 * there aren't currently many known use cases in which this is a problem.
 * Instead, the numbers are non reproducible and unpredictable, meaning that
 * will vary on different runs. 
 * 
 * @param buf           Output buffer
 * @param len           Length of the output buffer
 * @return              0 on success, -1 on failure. Currently, the function
 *                      never returns -1.
 */
int getentropy(void *buf, size_t len);

/**
 * @brief Return 64-bit of entropy.
 * 
 * This is a simplified API for getentropy() to just return 64-bit of entropy
 * instead of an arbitrary buffer.
 *
 * @note This function is much, much slower than calling rand(). If you just
 *       just need random numbers for your game, use rand() instead.
 * 
 * @return Unpredictable 64-bit random number
 */
 uint64_t getentropy64(void);
 
 /**
 * @brief Return 32-bit of entropy.
 * 
 * This is a simplified API for getentropy() to just return 32-bit of entropy
 * instead of an arbitrary buffer. Useful for instance to seed `srand()`.
 * 
 * @note This function is much, much slower than calling rand(). If you just
 *       just need random numbers for your game, use rand() instead.
 *
 * @return Unpredictable 32-bit random number
 */
uint32_t getentropy32(void);

#ifdef __cplusplus
}
#endif

#endif
