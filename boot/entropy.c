/**
 * @file entropy.c
 * @author Giovanni Bajo (giovannibajo@gmail.com)
 * @brief Entropy accumulator
 * 
 * This module implements a simple entropy accumulator. During IPL3, we
 * accumulate entropy from several sources and accumulate it using entropy_add.
 * After we are done, we call entropy_get to retrieve the accumulated entropy
 * in the form of a random 32-bit integer.
 * 
 * The entropy accumulator is based on the MurmurHash3 algorithm, which is
 * a very fast hash function. We use it in a very simple way, by using the
 * 32-bit version and by using the 32-bit state as the entropy accumulator.
 * 
 * This code is not cryptographic safe but it is good enough for our purposes
 * of providing some degree of randomness to the application.
 */
#include "entropy.h"
#include "debug.h"

// Comment these to debug entropy accumulation
#undef debugf
#define debugf(...)

static uint32_t rotl(uint32_t x, int k)
{
    return (x << k) | (x >> (-k & 31));
}

void entropy_add(uint32_t k)
{
    debugf("entropy_add: ", k);
    #define h __entropy_state
    k *= 0xcc9e2d51;
    k = rotl(k, 15);
    k *= 0x1b873593;
    h ^= k;
    h = rotl(h, 13);
    h = h * 5 + 0xe6546b64;
    #undef h
}

uint32_t entropy_get(void)
{
    // NOTE: in the true MurmurHash3, we should xor the length of the
    // input string. We don't do that because we don't keep a length
    uint32_t h = __entropy_state;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    debugf("entropy_final: ", h);
    return h;
}
