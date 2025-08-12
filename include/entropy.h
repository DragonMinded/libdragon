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
 */
#ifndef LIBDRAGON_ENTROPY_H
#define LIBDRAGON_ENTROPY_H

#include <stdint.h>
#include <stddef.h>

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
 * The code is not cryptographically safe especially by
 * modern standards, but it should be good enough for expected usages on
 * Nintendo 64.
 * 
 * @param buf           Output buffer
 * @param len           Length of the output buffer
 * @return int          0 on success, -1 on failure. Currently, the function
 *                      never returns -1.
 */
int getentropy(void *buf, size_t len);

/**
 * @brief Return 32-bit of entropy.
 * 
 * This is a simplified API for getentropy() to just return 32-bit of entropy
 * instead of an arbitrary buffer. Useful for instance to seed srand().
 * 
 * @return uint32_t         32-bit of entropy
 */
uint32_t getentropy32(void);

#endif