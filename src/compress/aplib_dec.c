/**
 * @file aplib_dec.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef N64
#include "n64sys.h"
#include "dma.h"
#include "dragonfs.h"
#include "debug.h"
#include "asset.h"

#include <malloc.h>
#endif
#include "../utils.h"
#include "../asset_internal.h"
#include "aplib_dec_internal.h"
#include "ringbuf_internal.h"

/** @brief Minimum match offset for 3-byte matches */
#define MINMATCH3_OFFSET 1280
/** @brief Minimum match offset for 4-byte matches */
#define MINMATCH4_OFFSET 32000

/// @cond
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif
/// @endcond

/** @brief APLib decompressor */
typedef struct {
    __lookahead_buf_t lb;                               ///< Lookahead reader state
    uint8_t cc;                                         ///< Current byte being processed
    int shift;                                          ///< Current bit being processed
    struct {
        decompress_ringbuf_t ringbuf;                   ///< Ring buffer
        bool first_literal_done;                        ///< Whether the first literal has been written
        int nlit;                                       ///< Number of expected literals
        int match_off;                                  ///< Offset of the match
        int match_len;                                  ///< Length of the match
    } partial;                                          ///< Partial decompression state
} aplib_decompressor_t;

_Static_assert(sizeof(aplib_decompressor_t) <= DECOMPRESS_APLIB_STATE_SIZE, "APLib decompressor state too small");

static inline uint8_t readbyte(aplib_decompressor_t *d)
{
    return __lookahead_buf_readbyte(&d->lb);
}

static int readbit(aplib_decompressor_t *d)
{
    if (unlikely(d->shift < 0)) {
        d->shift = 7;
        d->cc = readbyte(d);
    }
    return (d->cc >> d->shift--) & 1;
}

static inline int readgamma2(aplib_decompressor_t *d)
{
    int v = 1;
    do v = (v << 1) | readbit(d);
    while (readbit(d));
    return v;
}

static void decompress_reset(aplib_decompressor_t *d)
{
    d->shift = -1;
    __lookahead_buf_reset(&d->lb);
    d->partial.ringbuf.ringbuf_pos = 0;
    d->partial.first_literal_done = false;
    d->partial.nlit = 0;
    d->partial.match_off = 0;
    d->partial.match_len = 0;
}

static void decompress_init(aplib_decompressor_t *d, int fd, uint32_t rom_addr)
{
    memset(d, 0, sizeof(*d));
    (void)rom_addr;
    __lookahead_buf_init(&d->lb, fd);
    decompress_reset(d);
}

__attribute__((used))
static int decompress_aplib_partial(aplib_decompressor_t *d, uint8_t *out, int len)
{
    if (!len || d->lb.eof) return 0;
    uint8_t *out_orig = out;
    uint8_t *out_end = out + len;

    if (!d->partial.first_literal_done) {
        *out++ = readbyte(d);
        __ringbuf_writebyte(&d->partial.ringbuf, out[-1]);
        d->partial.nlit = 3;
        d->partial.match_len = 0;
        d->partial.match_off = -1;
        d->partial.first_literal_done = true;
    }

    int nlit = d->partial.nlit;
    int match_off = d->partial.match_off;
    int match_len = d->partial.match_len;
    if (match_len)
        goto copy_match;
    
    while (out < out_end) {
        if (!readbit(d)) {
            // 0: literal
            *out++ = readbyte(d);
            __ringbuf_writebyte(&d->partial.ringbuf, out[-1]);
            nlit = 3;
            continue;
        }
        if (!readbit(d)) {
            // 10: 8+n bits offset
            int off_hi = readgamma2(d) - nlit;
            if (off_hi >= 0) {
                match_off = (off_hi << 8) | readbyte(d);
                match_len = readgamma2(d);
                if (match_off < 128 || match_off >= MINMATCH4_OFFSET)
                    match_len += 2;
                else if (match_off >= MINMATCH3_OFFSET)
                    match_len += 1;
            } else {
                // rep-match
                match_len = readgamma2(d);
            }
        } else if (!readbit(d)) {
            // 110: 7 bits offset + 1 bit length
            uint8_t cmd = readbyte(d);
            if (cmd == 0) {
                // end of stream
                d->lb.eof = true;
                break;
            }

            match_off = cmd >> 1;
            match_len = (cmd & 1) + 2;
        } else {
            // 111: 4 bits offset
            int match_off2 = readbit(d) << 3;
            match_off2 |= readbit(d) << 2;
            match_off2 |= readbit(d) << 1;
            match_off2 |= readbit(d) << 0;
            nlit = 3;
            if (match_off2) {
                __ringbuf_copy(&d->partial.ringbuf, match_off2, out, 1);
                out++;
            } else {
                *out++ = 0;
                __ringbuf_writebyte(&d->partial.ringbuf, out[-1]);
            }
            continue;
        }

    copy_match:;
        int copy_len = MIN(out_end - out, match_len);
        __ringbuf_copy(&d->partial.ringbuf, match_off, out, copy_len);
        nlit = 2;
        out += copy_len;
        match_len -= copy_len;
    }

    d->partial.nlit = nlit;
    d->partial.match_off = match_off;
    d->partial.match_len = match_len;

    return out - out_orig;
}

void decompress_aplib_init(void *state, int fd, int winsize)
{
    aplib_decompressor_t *d = state;
    decompress_init(d, fd, 0);
    __ringbuf_init(&d->partial.ringbuf, state+sizeof(aplib_decompressor_t), winsize);
}

void decompress_aplib_reset(void *state)
{
    aplib_decompressor_t *d = state;
    decompress_reset(d);
}

ssize_t decompress_aplib_read(void *state, void *buf, size_t len)
{
    aplib_decompressor_t *d = state;
    return decompress_aplib_partial(d, buf, len);
}

#ifdef N64
int decompress_aplib_full_inplace(const uint8_t* in, size_t cmp_size, uint8_t *out, size_t size)
{
    extern int decompress_aplib_full_fast(const void*, int, void*);
    return decompress_aplib_full_fast(in, cmp_size, out);
}
#endif