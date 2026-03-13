/**
 * @file shrinkler_dec.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdalign.h>

#include "shrinkler_dec_internal.h"
#include "ringbuf_internal.h"
#include "../utils.h"
#include "debug.h"

/// @cond
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

#ifdef N64
#define read32be(ptr) (*(uint32_t*)(ptr))
#else
#define read32be(ptr) __builtin_bswap32(*(uint32_t*)(ptr))
#endif
/// @endcond

#define ADJUST_SHIFT 4             ///< Shift amount for context probability adjustment

#define NUM_SINGLE_CONTEXTS 1      ///< Number of single contexts
#define NUM_CONTEXT_GROUPS 4       ///< Number of context groups  
#define CONTEXT_GROUP_SIZE 256     ///< Size of each context group
#define NUM_CONTEXTS  (NUM_SINGLE_CONTEXTS + NUM_CONTEXT_GROUPS * CONTEXT_GROUP_SIZE)  ///< Total number of contexts

#define CONTEXT_KIND 0             ///< Context kind index
#define CONTEXT_REPEATED -1        ///< Context repeated index

#define CONTEXT_GROUP_LIT 0        ///< Context group for literals
#define CONTEXT_GROUP_OFFSET 2     ///< Context group for offsets
#define CONTEXT_GROUP_LENGTH 3     ///< Context group for lengths

#ifdef N64
int decompress_shrinkler_full_inplace(const uint8_t* in, size_t cmp_size, uint8_t *out, size_t size)
{
    extern int decompress_shrinkler_full_fast(const uint8_t* in, int insize, uint8_t *out);
    return decompress_shrinkler_full_fast(in, cmp_size, out);
}
#endif

// ----------------------------------------------------------------------------
// Streaming (asset_fopen) implementation
// ----------------------------------------------------------------------------

/** @brief Shrinkler streaming decompressor state */
typedef struct {
    uint8_t buf[128] __attribute__((aligned(8)));   ///< File buffer
    int fd;                          ///< File descriptor to read from
    int buf_idx;                     ///< Current index in the file buffer
    int buf_size;                    ///< Size of the file buffer
    bool file_eof;                   ///< True if we reached EOF on the compressed stream

    // Arithmetic decoder state
    uint16_t contexts[NUM_CONTEXTS]; ///< Probability contexts
    uint32_t intervalsize;           ///< Current interval size
    uint64_t intervalvalue;          ///< Current interval value
    int bits_left;                   ///< Bits left until next 32-bit refill
    bool decoder_inited;             ///< True after reading the initial 32 bits

    // LZ/high-level decoder state
    bool stream_eof;                 ///< True if end-of-stream token has been decoded
    bool ref;                        ///< Next symbol is a match if true, literal if false
    bool prev_was_ref;               ///< Previous symbol was a match
    int match_off;                   ///< Current match offset (for repeated matches)
    int match_len;                   ///< Remaining match length to copy
    uint32_t out_pos;                ///< Bytes produced so far (for parity)

    decompress_ringbuf_t ringbuf;    ///< Ring buffer (window)
} shrinkler_dec_state_t;

_Static_assert(sizeof(shrinkler_dec_state_t) <= DECOMPRESS_SHRINKLER_STATE_SIZE,
    "Shrinkler streaming decompressor state too small");

static void shr_refill(shrinkler_dec_state_t *s)
{
    s->buf_size = read(s->fd, s->buf, sizeof(s->buf));
    s->buf_idx = 0;
    s->file_eof = (s->buf_size == 0);
}

static inline uint8_t shr_readbyte(shrinkler_dec_state_t *s)
{
    if (unlikely(s->buf_idx >= s->buf_size)) {
        shr_refill(s);
        /*
         * Shrinkler streams are typically padded and the arithmetic decoder
         * is allowed to read past the end of the compressed input.
         * When reading from a file descriptor, we emulate this by returning
         * zero bytes once EOF is reached, instead of terminating early.
         */
        if (unlikely(s->file_eof)) return 0;
    }
    return s->buf[s->buf_idx++];
}

static inline uint32_t shr_read_u32be(shrinkler_dec_state_t *s)
{
    uint32_t v = 0;
    v = (v << 8) | shr_readbyte(s);
    v = (v << 8) | shr_readbyte(s);
    v = (v << 8) | shr_readbyte(s);
    v = (v << 8) | shr_readbyte(s);
    return v;
}

static void shr_stream_decode_init(shrinkler_dec_state_t *s)
{
    for (int i = 0; i < NUM_CONTEXTS; i++)
        s->contexts[i] = 0x8000;

    s->intervalsize = 1;
    s->intervalvalue = 0;
    s->bits_left = 0;

    // Load initial 32 bits and position them like the reference decoder
    uint32_t w = shr_read_u32be(s);
    s->intervalvalue = ((uint64_t)w) << 31;
    s->bits_left = 1;
    s->intervalsize = 0x8000;
    s->decoder_inited = true;
}

static inline int shr_stream_decode_bit(shrinkler_dec_state_t *s, int context_index)
{
    while (s->intervalsize < 0x8000) {
        if (unlikely(s->bits_left == 0)) {
            s->intervalvalue |= (uint64_t)shr_read_u32be(s);
            s->bits_left = 32;
        }
        s->bits_left -= 1;
        s->intervalsize <<= 1;
        s->intervalvalue <<= 1;
    }

    unsigned prob = s->contexts[context_index];
    unsigned intervalvalue = (unsigned)(s->intervalvalue >> 48);
    unsigned threshold = (s->intervalsize * prob) >> 16;

    if (intervalvalue >= threshold) {
        // Zero
        s->intervalvalue -= (uint64_t)threshold << 48;
        s->intervalsize -= threshold;
        s->contexts[context_index] = prob - (prob >> ADJUST_SHIFT);
        return 0;
    } else {
        // One
        s->intervalsize = threshold;
        s->contexts[context_index] = prob + (0xffff >> ADJUST_SHIFT) - (prob >> ADJUST_SHIFT);
        return 1;
    }
}

static inline int shr_stream_decode_number(shrinkler_dec_state_t *s, int base_context)
{
    int context;
    int i;
    for (i = 0 ;; i++) {
        context = base_context + (i * 2 + 2);
        if (shr_stream_decode_bit(s, context) == 0) break;
    }

    int number = 1;
    for (; i >= 0 ; i--) {
        context = base_context + (i * 2 + 1);
        int bit = shr_stream_decode_bit(s, context);
        number = (number << 1) | bit;
    }
    return number;
}

static inline int shr_lzDecode(shrinkler_dec_state_t *s, int context)
{
    return shr_stream_decode_bit(s, NUM_SINGLE_CONTEXTS + context);
}

static inline int shr_lzDecodeNumber(shrinkler_dec_state_t *s, int context_group)
{
    return shr_stream_decode_number(s, NUM_SINGLE_CONTEXTS + (context_group << 8));
}

void decompress_shrinkler_init(void *state, int fd, int winsize)
{
    shrinkler_dec_state_t *s = (shrinkler_dec_state_t*)state;
    memset(s, 0, sizeof(*s));
    s->fd = fd;
    __ringbuf_init(&s->ringbuf, (uint8_t*)state + sizeof(shrinkler_dec_state_t), winsize);
    decompress_shrinkler_reset(state);
}

void decompress_shrinkler_reset(void *state)
{
    shrinkler_dec_state_t *s = (shrinkler_dec_state_t*)state;
    s->buf_idx = 0;
    s->buf_size = 0;
    s->file_eof = false;

    s->decoder_inited = false;
    s->stream_eof = false;
    s->ref = false;
    s->prev_was_ref = false;
    s->match_off = 0;
    s->match_len = 0;
    s->out_pos = 0;
    s->ringbuf.ringbuf_pos = 0;
}

ssize_t decompress_shrinkler_read(void *state, void *buf, size_t len)
{
    shrinkler_dec_state_t *s = (shrinkler_dec_state_t*)state;
    uint8_t *out = (uint8_t*)buf;
    uint8_t *out_orig = out;

    if (len == 0 || s->stream_eof) return 0;

    if (unlikely(!s->decoder_inited)) {
        // Lazy-init: read the initial 32-bit seed only when the stream is first consumed
        if (unlikely(s->buf_idx >= s->buf_size))
            shr_refill(s);
        if (unlikely(s->file_eof)) { s->stream_eof = true; return 0; }
        shr_stream_decode_init(s);
    }

    while (!s->stream_eof && len > 0) {
        // If we're in the middle of copying a match, continue copying bytes first.
        if (s->match_len > 0) {
            int n = MIN((size_t)s->match_len, len);
            __ringbuf_copy(&s->ringbuf, s->match_off, out, n);
            out += n;
            len -= n;
            s->match_len -= n;
            s->out_pos += n;
            if (s->match_len > 0)
                break; // output buffer full

            // Match finished: decode the next kind bit
            int parity = s->out_pos & 1;
            s->ref = shr_lzDecode(s, CONTEXT_KIND + (parity << 8));
            continue;
        }

        if (!s->ref) {
            // Literal
            int parity = s->out_pos & 1;
            int context = 1;
            for (int i = 7; i >= 0; i--) {
                int bit = shr_lzDecode(s, (parity << 8) | context);
                context = (context << 1) | bit;
            }
            uint8_t lit = (uint8_t)context;
            *out++ = lit;
            __ringbuf_writebyte(&s->ringbuf, lit);
            len -= 1;
            s->out_pos += 1;
            s->prev_was_ref = false;

            parity = s->out_pos & 1;
            s->ref = shr_lzDecode(s, CONTEXT_KIND + (parity << 8));
        } else {
            // Match
            bool repeated = false;
            if (!s->prev_was_ref)
                repeated = shr_lzDecode(s, CONTEXT_REPEATED);
            if (!repeated) {
                s->match_off = shr_lzDecodeNumber(s, CONTEXT_GROUP_OFFSET) - 2;
                if (s->match_off == 0) {
                    s->stream_eof = true;
                    break;
                }
            }
            s->match_len = shr_lzDecodeNumber(s, CONTEXT_GROUP_LENGTH);
            s->prev_was_ref = true;

            // Copy as much as we can in this call (loop continues to match_len branch)
        }
    }

    return out - out_orig;
}
