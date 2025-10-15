/**
 * @file rand.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Internal random number generation functions.
 */

#include "rand_internal.h"
#include "n64types.h"
#include <stdatomic.h>
#include <unistd.h>
#include <string.h>

/** @brief Constant used in the xorshift64 algorithm */
uint64_t __xorshift64_mul_K = 0x2545F4914F6CDD1Dull;

__attribute__((noinline))
uint64_t __xorshift64(_Atomic uint64_t *state) {
    uint64_t old = atomic_load_explicit(state, memory_order_relaxed);
    while (1) {
        uint64_t x = old;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        if (atomic_compare_exchange_weak_explicit(
                state, &old, x,
                memory_order_relaxed, memory_order_relaxed))  
            return x * __xorshift64_mul_K;
    }
}

__attribute__((noinline))
void __xorshift64_buffer(void *buf, size_t len, _Atomic uint64_t *state)
{
    while (len) {
        uint64_t x = __xorshift64(state);
        size_t m = len < 8 ? len : 8;
        memcpy(buf, &x, m);
        buf += m;
        len -= m;
    }
}

// Seeded at construction time, but anyway avoid 0 which is invalid
static _Atomic uint64_t rand_state = 1;

__attribute__((noinline))
uint64_t __rand64(void)
{
    return __xorshift64(&rand_state);
}

void __rand(void *buf, size_t n)
{
    __xorshift64_buffer(buf, n, &rand_state);
}

/** @brief Initialize the random number generator (constructor) */
__attribute__((constructor))
void __rand_init(void)
{
    // Initialize random state
    getentropy((uint8_t *)&rand_state, sizeof(rand_state));
}

extern inline uint8_t  __rand8(void);
extern inline uint16_t __rand16(void);
extern inline uint32_t __rand32(void);
extern inline float    __randf32(void);
