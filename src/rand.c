/**
 * @file rand.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Internal random number generation functions.
 */

#include "rand_internal.h"
#include "n64types.h"
#include <stdatomic.h>
#include <unistd.h>

// Seeded at construction time, but anyway avoid 0 which is invalid
static _Atomic uint64_t rand_state = 1;

__attribute__((noinline))
uint64_t __rand64(void)
{
    uint64_t old = atomic_load_explicit(&rand_state, memory_order_relaxed);
    while (1) {
        uint64_t h = old;
        h ^= h << 13;
        h ^= h >> 7;
        h ^= h << 17;
        if (atomic_compare_exchange_weak_explicit(
                &rand_state, &old, h,
                memory_order_relaxed, memory_order_relaxed))  
            return h;
    }
}

void __rand(void *buf, size_t n)
{
    u_uint64_t *p = (u_uint64_t *)buf;

    while (n >= sizeof(uint64_t)) {
        *p++ = __rand64();
        n -= sizeof(uint64_t);
    }

    if (n > 0) {
        uint64_t r = __rand64();
        for (size_t i = 0; i < n; i++) {
            ((uint8_t *)p)[i] = r >> 56;
            r <<= 8;
        }
    }
}

/** @brief Initialize the random number generator (constructor) */
__attribute__((constructor))
void __rand_init(void)
{
    // Make sure interrupts are initialized before we use getentropy()
    extern void __init_interrupts(void);
    __init_interrupts();

    // Initialize random state
    getentropy((uint8_t *)&rand_state, sizeof(rand_state));
}

extern inline uint8_t  __rand8(void);
extern inline uint16_t __rand16(void);
extern inline uint32_t __rand32(void);
extern inline float    __randf32(void);
