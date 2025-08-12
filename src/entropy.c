/**
 * @file entropy.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Entropy source for non-deterministic random number generation.
 */

#include "entropy.h"
#include "entropy_internal.h"
#include "interrupt.h"
#include <string.h>

/** @brief Current entropy state */
uint64_t __entropy_state = 0;

/** @brief Entropy calculation constants (MurMurHash3-128)
 * These are not marked as static const to coerce GCC to load them for code efficiency.
 */
uint64_t __entropy_K[5] = {
    0x87c37b91114253d5ull, 0x4cf5ad432745937full,
    0xff51afd7ed558ccdull, 0xc4ceb9fe1a85ec53ull,
    0x0139408dcbbf7a44ull,
};

// Mix the entropy state to ensure it advances a bit in a non predictable way.
static void __entropy_mix(void)
{
    __entropy_state = __entropy_state<<27 | __entropy_state>>37;
    __entropy_state = __entropy_state * 5 + 0x52dce729;
}

/**
 * @brief Add some non-deterministic data to the entropy pool.
 * 
 * This is an internal function that can be used by libdragon libraries to add
 * some non-deterministic data to the entropy pool. One example of such data
 * would be the joypad inputs at any given point.
 * 
 * The entropy pool is then used t
 * 
 * @param k         Non-deterministic data (up to 64 bits)
 */
void __entropy_add(uint64_t k) {
    // This is half of MurMurHash3-128.
    k *= __entropy_K[0];
    k = k<<31 | k>>33;
    k *= __entropy_K[1];
    disable_interrupts();
    __entropy_state ^= k;
    __entropy_mix();
    enable_interrupts();
}

// Since libdragon doesn't call __entropy_add() enough for now, let's always
// pull additional entropy from the hardware by reading a few registers that
// are likely to change at the point of sampling.
static void __entropy_add_internal(void)
{
    volatile uint32_t *const AI_STATUS = (uint32_t*)0xA450000C;
    volatile uint32_t *const SP_PC = (uint32_t*)0xA4080000;
    volatile uint32_t *const DP_CLOCK = (uint32_t*)0xA4100010;
    volatile uint32_t *const PI_UNKNOWN = (uint32_t*)0xA4600034;
    volatile uint32_t *const RI_BANK0_ROW = (uint32_t*)0xA3F00200;
    volatile uint32_t *const RI_BANK1_ROW = (uint32_t*)0xA3F00600;
    static volatile uint32_t* entropic_regs[] = {
        RI_BANK0_ROW, RI_BANK1_ROW, AI_STATUS, SP_PC, DP_CLOCK, PI_UNKNOWN, 
    };

    for (int i=0; i<sizeof(entropic_regs)/sizeof(entropic_regs[0]); i+=2) {
        uint64_t k = ((uint64_t)*entropic_regs[i+0] << 32) | *entropic_regs[i+1];
        __entropy_add(k);
    }
}

// Extract data from the entropy pool.
static uint64_t __entropy_get(void) {
    disable_interrupts();
    uint64_t h = __entropy_state;
    __entropy_mix();
    enable_interrupts();
    h ^= h >> 33;
    h *= __entropy_K[2];
    h ^= h >> 33;
    h *= __entropy_K[3];
    h ^= h >> 33;
    return h;
}

static uint64_t xs_state = 88172645463325252ULL;

__attribute__((noinline))
static uint64_t xorshift64_step(void) {
    disable_interrupts();
    uint64_t x = xs_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    xs_state = x;
    enable_interrupts();
    return x * __entropy_K[4];
}

__attribute__((noinline))
static void reseed_from_entropy(void) {
    __entropy_add_internal();
    disable_interrupts();
    xs_state ^= __entropy_get();
    enable_interrupts();
}

int getentropy(void *buf, size_t len) {
    uint8_t *p = buf;
    reseed_from_entropy();

    while (len) {
        uint64_t x = xorshift64_step();
        size_t m = len < 8 ? len : 8;
        memcpy(p, &x, m);
        p += m;
        len -= m;
    }
    return 0;
}

uint32_t getentropy32(void) {
    reseed_from_entropy();
    uint64_t x = xorshift64_step();
    return (uint32_t)(x >> 32);
}
