/**
 * @file entropy.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Entropy source for non-deterministic random number generation.
 */

#include "entropy.h"
#include "entropy_internal.h"
#include "rand_internal.h"
#include "interrupt_internal.h"
#include <stdatomic.h>
#include <string.h>

/** @brief Current entropy state */
uint64_t __entropy_state = 0;

/** @brief Entropy calculation constants (MurMurHash3-128)
 * These are not marked as static const to coerce GCC to load them for code efficiency.
 */
uint64_t __entropy_K[4] = {
    0x87c37b91114253d5ull, 0x4cf5ad432745937full,
    0xff51afd7ed558ccdull, 0xc4ceb9fe1a85ec53ull,
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
__attribute__((noinline))
void __entropy_add(uint64_t k) {
    // This is half of MurMurHash3-128.
    k *= __entropy_K[0];
    k = k<<31 | k>>33;
    k *= __entropy_K[1];
    uint32_t sr = __disable_interrupts();
    __entropy_state ^= k;
    __entropy_mix();
    __enable_interrupts(sr);
}

/** @brief Registers used to pull entropy from */
volatile uint32_t* __entropy_regs[] = {
    (uint32_t*)0xA3F00200, // RI_BANK0_ROW
    (uint32_t*)0xA3F00600, // RI_BANK1_ROW
    (uint32_t*)0xA450000C, // AI_STATUS
    (uint32_t*)0xA4080000, // SP_PC
    (uint32_t*)0xA4100010, // DP_CLOCK
    (uint32_t*)0xA4600034, // PI_UNKNOWN
};

// Since libdragon doesn't call __entropy_add() enough for now, let's always
// pull additional entropy from the hardware by reading a few registers that
// are likely to change at the point of sampling.
static void __entropy_add_internal(void)
{
    for (int i=0; i<sizeof(__entropy_regs)/sizeof(__entropy_regs[0]); i+=2) {
        uint64_t k = ((uint64_t)*__entropy_regs[i+0] << 32) | *__entropy_regs[i+1];
        __entropy_add(k);
    }
}

// Extract data from the entropy pool.
static uint64_t __entropy_get(void) {
    uint32_t sr = __disable_interrupts();
    uint64_t h = __entropy_state;
    __entropy_mix();
    __enable_interrupts(sr);
    h ^= h >> 33;
    h *= __entropy_K[2];
    h ^= h >> 33;
    h *= __entropy_K[3];
    h ^= h >> 33;
    return h;
}

static _Atomic uint64_t xs_state = 88172645463325252ULL;

__attribute__((noinline))
static void reseed_from_entropy(void) {
    __entropy_add_internal();
    uint64_t e = __entropy_get();
    atomic_fetch_xor_explicit(&xs_state, e, memory_order_relaxed);
}

int getentropy(void *buf, size_t len) {
    reseed_from_entropy();
    __xorshift64_buffer(buf, len, &xs_state);
    return 0;
}

uint64_t getentropy64(void) {
    reseed_from_entropy();
    return __xorshift64(&xs_state);
}

uint32_t getentropy32(void) {
    return getentropy64() >> 32;
}
