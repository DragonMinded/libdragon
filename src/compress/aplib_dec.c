#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef N64
#include "n64sys.h"
#include "dma.h"
#include "dragonfs.h"
#include "debug.h"
#include <malloc.h>
#endif
#include "../utils.h"
#include "../asset_internal.h"
#include "aplib_dec_internal.h"
#include "ringbuf_internal.h"

#define MINMATCH3_OFFSET 1280
#define MINMATCH4_OFFSET 32000

#if defined(__GNUC__) || defined(__clang__)
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

/** @brief APLib decompressor */
typedef struct {
    uint8_t buf[2][128] __attribute__((aligned(16)));   ///< Buffer of loaded data
    int cur_buf;                                        ///< Current buffer being used
    uint8_t *buf_ptr;                                   ///< Pointer to data being processed
    uint8_t *buf_end;                                   ///< Pointer to end of current loaded data
    uint8_t cc;                                         ///< Current byte being processed
    int shift;                                          ///< Current bit being processed
    int fd;                                             ///< File being read from
    uint32_t rom_addr;                                  ///< ROM address being read from (optional)
    bool eof;                                           ///< Whether the end of the stream has been reached
    struct {
        decompress_ringbuf_t ringbuf;                   ///< Ring buffer
        bool first_literal_done;                        ///< Whether the first literal has been written
        int nlit;                                       ///< Number of expected literals
        int match_off;                                  ///< Offset of the match
        int match_len;                                  ///< Length of the match
    } partial;                                          ///< Partial decompression state
} aplib_decompressor_t;

_Static_assert(sizeof(aplib_decompressor_t) <= DECOMPRESS_APLIB_STATE_SIZE, "APLib decompressor state too small");

__attribute__((noinline))
static void refill(aplib_decompressor_t *d)
{
    int buf_size;

    d->cur_buf ^= 1;
    #ifdef N64
    if (d->rom_addr) {
        data_cache_hit_invalidate(d->buf[d->cur_buf^1], sizeof(d->buf[0]));
        dma_read_raw_async(d->buf[d->cur_buf^1], d->rom_addr, sizeof(d->buf[0]));
        d->rom_addr += sizeof(d->buf[0]);
        buf_size = sizeof(d->buf[0]);
    } else {
        buf_size = read(d->fd, d->buf[d->cur_buf], sizeof(d->buf[0]));
    }
    #else
    buf_size = read(d->fd, d->buf[d->cur_buf], sizeof(d->buf[0]));
    #endif

    d->buf_ptr = d->buf[d->cur_buf];
    d->buf_end = d->buf[d->cur_buf] + buf_size;
    if (!buf_size) d->eof = true;
}

static uint8_t readbyte(aplib_decompressor_t *d)
{
    if (unlikely(d->buf_ptr >= d->buf_end)) refill(d);
    return *d->buf_ptr++;
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
    d->eof = false;
    d->partial.ringbuf.ringbuf_pos = 0;
    d->partial.first_literal_done = false;
    d->partial.nlit = 0;
    d->partial.match_off = 0;
    d->partial.match_len = 0;
    d->buf_ptr = 0;
    d->buf_end = 0;
    
    #ifdef N64
	if (d->rom_addr) {
		data_cache_hit_invalidate(d->buf[d->cur_buf^1], sizeof(d->buf[0]));
		dma_read_raw_async(d->buf[d->cur_buf^1], d->rom_addr, sizeof(d->buf[0]));
		d->rom_addr += sizeof(d->buf[0]);
	}
    #endif
}

static void decompress_init(aplib_decompressor_t *d, int fd, uint32_t rom_addr)
{
    memset(d, 0, sizeof(*d));
    d->fd = fd;
    d->rom_addr = rom_addr;
    decompress_reset(d);
}

static int decompress_full(aplib_decompressor_t *d, uint8_t *out)
{
    uint8_t *out_orig = out;
    int nlit = 3;
    int match_off = -1;
    int match_len = -1;

    *out++ = readbyte(d);
    while (!d->eof) {
        if (!readbit(d)) {
            // 0: literal
            *out++ = readbyte(d);
            nlit = 3;
            // fprintf(stderr, "%x: lit %x\n", out-out_orig-1, out[-1]);
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
                // fprintf(stderr, "%x: offset8 %x %x\n", out-out_orig,match_off, match_len);
            } else {
                // rep-match
                match_len = readgamma2(d);
                // fprintf(stderr, "%x: offset8 rep %x\n", out-out_orig, match_len);
            }
        } else if (!readbit(d)) {
            // 110: 7 bits offset + 1 bit length
            uint8_t cmd = readbyte(d);
            // fprintf(stderr, "%x: offset7 %02x\n", out-out_orig, cmd);
            if (cmd == 0) {
                // end of stream
                // debugf("EOD\n");
                // fprintf(stderr, "EOD\n");
                d->eof = true;
                break;
            }

            match_off = cmd >> 1;
            *out = out[-match_off], out++;
            *out = out[-match_off], out++;
            if (cmd & 1)
            *out = out[-match_off], out++;
            nlit = 2;
            continue;
        } else {
            // 111: 4 bits offset
            int match_off2 = readbit(d) << 3;
            match_off2 |= readbit(d) << 2;
            match_off2 |= readbit(d) << 1;
            match_off2 |= readbit(d) << 0;
            // fprintf(stderr, "%x: offset4 %x\n", out-out_orig, match_off2);
            nlit = 3;
            if (match_off2) {
                *out = out[-match_off2];
                out++;
            } else {
                *out++ = 0;
            }
            continue;
        }

        if (match_off >= match_len) {
            do {
                memcpy(out, out - match_off, 8);
                out += 8;
                match_len -= 8;
            } while (match_len > 0);
            out += match_len;
        } else {
            while (match_len-- > 0) {
                *out = out[-match_off];
                out++;
            }
        }
        nlit = 2;
    }

    // fprintf(stderr, "return %x\n", out - out_orig);
    return out - out_orig;
}

__attribute__((used))
static int decompress_aplib_partial(aplib_decompressor_t *d, uint8_t *out, int len)
{
    if (!len || d->eof) return 0;
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
                d->eof = true;
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

void* decompress_aplib_full(const char *fn, int fd, size_t cmp_size, size_t size)
{
    uint32_t rom_addr = 0;
    #ifdef N64
	if (strncmp(fn, "rom:/", 5) == 0) {
		rom_addr = (dfs_rom_addr(fn+5) & 0x1fffffff) + lseek(fd, 0, SEEK_CUR);
	}
    #endif
    void *buf = memalign(ASSET_ALIGNMENT, size + 8);
    aplib_decompressor_t d;
    decompress_init(&d, fd, rom_addr);
    int sz = decompress_full(&d, buf); (void)sz;
    buf = realloc(buf, size);
    return buf;
}

#ifdef N64
int decompress_aplib_full_inplace(const uint8_t* in, size_t cmp_size, uint8_t *out, size_t size)
{
    extern int decompress_aplib_full_fast(const void*, int, void*);
    return decompress_aplib_full_fast(in, cmp_size, out);
}
#endif