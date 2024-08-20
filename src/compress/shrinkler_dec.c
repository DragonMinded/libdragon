#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef N64
#include "debug.h"
#else

#endif

#if defined(__GNUC__) || defined(__clang__)
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

#if defined(__LITTLE_ENDIAN__)
#define read32be(ptr) __builtin_bswap32(*(uint32_t*)(ptr))
#else
#define read32be(ptr) (*(uint32_t*)(ptr))
#endif

#define ADJUST_SHIFT 4

#define NUM_SINGLE_CONTEXTS 1
#define NUM_CONTEXT_GROUPS 4
#define CONTEXT_GROUP_SIZE 256
#define NUM_CONTEXTS  (NUM_SINGLE_CONTEXTS + NUM_CONTEXT_GROUPS * CONTEXT_GROUP_SIZE)


#define CONTEXT_KIND 0
#define CONTEXT_REPEATED -1

#define CONTEXT_GROUP_LIT 0
#define CONTEXT_GROUP_OFFSET 2
#define CONTEXT_GROUP_LENGTH 3

/** @brief Decompressor state (for assembly) */
typedef struct {
    uint16_t contexts[NUM_CONTEXTS];    ///< Probability contexts
} shrinkler_asm_state_t;

/** @brief Decompressor state (for C) */
typedef struct {
    uint16_t contexts[NUM_CONTEXTS];    ///< Probability contexts
    unsigned intervalsize;              ///< Current interval size
    uint64_t intervalvalue;             ///< Current interval value

    uint8_t *src;                       ///< Pointer to the input data
    int bits_left;                      ///< Number of bits left in the interval
} shrinkler_ctx_t;

static void shr_decode_init(shrinkler_ctx_t *ctx, uint8_t *src) {
    for (int i=0; i<NUM_CONTEXTS; i++)
        ctx->contexts[i] = 0x8000;
    
    ctx->intervalsize = 1;
    ctx->intervalvalue = 0;
    ctx->src = src;
    ctx->bits_left = 0;

    // Adjust for 64-bit values
    for (int i=0;i<4;i++)
        ctx->intervalvalue = (ctx->intervalvalue << 8) | *ctx->src++;
    ctx->intervalvalue <<= 31;
    ctx->bits_left = 1;
    ctx->intervalsize = 0x8000;
}

static inline int shr_decode_bit(shrinkler_ctx_t *ctx, int context_index) {
    while ((ctx->intervalsize < 0x8000)) {
        if (unlikely(ctx->bits_left == 0)) {
            ctx->intervalvalue |= read32be(ctx->src);
            ctx->src += 4;
            ctx->bits_left = 32;
        }
        ctx->bits_left -= 1;
        ctx->intervalsize <<= 1;
        ctx->intervalvalue <<= 1;
    }

    unsigned prob = ctx->contexts[context_index];
    unsigned intervalvalue = ctx->intervalvalue >> 48;
    unsigned threshold = (ctx->intervalsize * prob) >> 16;

    if (intervalvalue >= threshold) {
        // Zero
        ctx->intervalvalue -= (uint64_t)threshold << 48;
        ctx->intervalsize -= threshold;
        ctx->contexts[context_index] = prob - (prob >> ADJUST_SHIFT);
        return 0;
    } else {
        // One
        ctx->intervalsize = threshold;
        ctx->contexts[context_index] = prob + (0xffff >> ADJUST_SHIFT) - (prob >> ADJUST_SHIFT);
        return 1;
    }
}

static inline int shr_decode_number(shrinkler_ctx_t *ctx, int base_context) {
    int context;
    int i;
    for (i = 0 ;; i++) {
        context = base_context + (i * 2 + 2);
        if (shr_decode_bit(ctx, context) == 0) break;
    }

    int number = 1;
    for (; i >= 0 ; i--) {
        context = base_context + (i * 2 + 1);
        int bit = shr_decode_bit(ctx, context);
        number = (number << 1) | bit;
    }

    return number;
}

static inline int lzDecode(shrinkler_ctx_t *ctx, int context) {
    return shr_decode_bit(ctx, NUM_SINGLE_CONTEXTS + context);
}

static inline int lzDecodeNumber(shrinkler_ctx_t *ctx, int context_group) {
    return shr_decode_number(ctx, NUM_SINGLE_CONTEXTS + (context_group << 8));
}

int shr_unpack(uint8_t *dst, uint8_t *src)
{
    const int parity_mask = 1;

    uint8_t *dst_start = dst;
    shrinkler_ctx_t ctx;
    shr_decode_init(&ctx, src);

    bool ref = false;
    bool prev_was_ref = false;
    int offset = 0;

    while (1) {
        if (ref) {
            bool repeated = false;
            if (!prev_was_ref) {
                repeated = lzDecode(&ctx, CONTEXT_REPEATED);
            }
            if (!repeated) {
                offset = lzDecodeNumber(&ctx, CONTEXT_GROUP_OFFSET) - 2;
                if (offset == 0)
                    break;
            }
            int length = lzDecodeNumber(&ctx, CONTEXT_GROUP_LENGTH);
            prev_was_ref = true;
            if (offset > 8) 
                while (length >= 8) {
                    memcpy(dst, dst - offset, 8);
                    dst += 8;
                    length -= 8;
                }            
            while (length--) {
                *dst = dst[-offset];
                dst++;
            }
        } else {
            int parity = (dst - dst_start) & parity_mask;
            int context = 1;
            for (int i = 7 ; i >= 0 ; i--) {
                int bit = lzDecode(&ctx, (parity << 8) | context);
                context = (context << 1) | bit;
            }
            uint8_t lit = context;
            *dst++ = lit;
            prev_was_ref = false;
        }
        int parity = (dst - dst_start) & parity_mask;
        ref = lzDecode(&ctx, CONTEXT_KIND + (parity << 8));
    }
    
    return dst - dst_start;
}

bool decompress_shrinkler_full(int fd, size_t cmp_size, size_t size, void *buf, int *buf_size)
{
    void *in = malloc(cmp_size);
    read(fd, in, cmp_size);
    if(buf == NULL || *buf_size < size) {
        *buf_size = size;
        return false;
    }
    int dec_size = shr_unpack(buf, in); (void)dec_size;
    assertf(dec_size == size, "Shrinkler size:%d exp:%d", dec_size, size);
    free(in);
    return true;
}

#ifdef N64
int decompress_shrinkler_full_inplace(const uint8_t* in, size_t cmp_size, uint8_t *out, size_t size)
{
    extern int decompress_shrinkler_full_fast(const uint8_t* in, int insize, uint8_t *out);
    return decompress_shrinkler_full_fast(in, cmp_size, out);
}
#endif
