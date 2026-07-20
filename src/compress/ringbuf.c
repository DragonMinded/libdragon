/**
 * @file ringbuf.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#include "ringbuf_internal.h"
#include "../utils.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

#ifdef N64
#include "n64sys.h"
#include "dma.h"
#include "dragonfs.h"
#endif

void __ringbuf_init(decompress_ringbuf_t *ringbuf, uint8_t *buf, int size)
{
    assert(size > 0);
    assert((size & (size-1)) == 0); // check if power of two
    ringbuf->ringbuf = buf;
    ringbuf->ringbuf_size = size ;
    ringbuf->ringbuf_pos = 0;
}

void __ringbuf_write(decompress_ringbuf_t *ringbuf, uint8_t *src, int count)
{
    while (count > 0) {
        int n = MIN(count, ringbuf->ringbuf_size - ringbuf->ringbuf_pos);
        memcpy(ringbuf->ringbuf + ringbuf->ringbuf_pos, src, n);
        ringbuf->ringbuf_pos += n;
        ringbuf->ringbuf_pos &= ringbuf->ringbuf_size - 1;
        src += n;
        count -= n;
    }
}

void __ringbuf_copy(decompress_ringbuf_t *ringbuf, int copy_offset, uint8_t *dst, int count)
{
    int ringbuf_copy_pos = (ringbuf->ringbuf_pos - copy_offset) & (ringbuf->ringbuf_size - 1);
    int dst_pos = 0;
    while (count > 0) {
        int wn = count;
        wn = wn < ringbuf->ringbuf_size - ringbuf_copy_pos     ? wn : ringbuf->ringbuf_size - ringbuf_copy_pos;
        wn = wn < ringbuf->ringbuf_size - ringbuf->ringbuf_pos ? wn : ringbuf->ringbuf_size - ringbuf->ringbuf_pos;
        count -= wn;

        // Check if there's an overlap in the ring buffer between read and write pos, in which
        // case we need to copy byte by byte.
        if (ringbuf->ringbuf_pos < ringbuf_copy_pos || 
            ringbuf->ringbuf_pos > ringbuf_copy_pos+7) {
            while (wn >= 8) {
                // Copy 8 bytes at at time, using a unaligned memory access (LDL/LDR/SDL/SDR)
                typedef uint64_t u_uint64_t __attribute__((aligned(1)));
                uint64_t value = *(u_uint64_t*)&ringbuf->ringbuf[ringbuf_copy_pos];
                if(dst) *(u_uint64_t*)&dst[dst_pos] = value;
                *(u_uint64_t*)&ringbuf->ringbuf[ringbuf->ringbuf_pos] = value;

                ringbuf_copy_pos += 8;
                ringbuf->ringbuf_pos += 8;
                dst_pos += 8;
                wn -= 8;
            }
        }

        // Finish copying the remaining bytes
        while (wn > 0) {
            uint8_t value = ringbuf->ringbuf[ringbuf_copy_pos];
            if(dst) dst[dst_pos] = value;
            ringbuf->ringbuf[ringbuf->ringbuf_pos] = value;

            ringbuf_copy_pos += 1;
            ringbuf->ringbuf_pos += 1;
            dst_pos += 1;
            wn -= 1;
        }

        ringbuf_copy_pos &= ringbuf->ringbuf_size - 1;
        ringbuf->ringbuf_pos &= ringbuf->ringbuf_size - 1;
    }
}

// ----------------------------------------------------------------------------
// Lookahead (double-buffer) reader
// ----------------------------------------------------------------------------

__attribute__((noinline))
void __lookahead_buf_refill(__lookahead_buf_t *lb)
{
    int buf_size;

    lb->cur ^= 1;

    #ifdef N64
    if (lb->rom_base) {
        dma_wait_finished(lb->ticket[lb->cur]);
        int next = lb->cur ^ 1;
        data_cache_hit_invalidate(lb->buf[next], sizeof(lb->buf[0]));
        lb->ticket[next] = dma_read_raw_async(lb->buf[next], lb->rom_addr, sizeof(lb->buf[0]));
        lb->rom_addr += sizeof(lb->buf[0]);
        buf_size = sizeof(lb->buf[0]);
    } else {
        buf_size = read(lb->fd, lb->buf[lb->cur], sizeof(lb->buf[0]));
    }
    #else
    buf_size = read(lb->fd, lb->buf[lb->cur], sizeof(lb->buf[0]));
    #endif

    lb->ptr = lb->buf[lb->cur];
    lb->end = lb->buf[lb->cur] + buf_size;
    if (!buf_size) lb->eof = true;
}

void __lookahead_buf_init(__lookahead_buf_t *lb, int fd)
{
    memset(lb, 0, sizeof(*lb));
    lb->fd = fd;

    #ifdef N64
    uint32_t rom_base = 0;
    if (ioctl(fd, IODFS_GET_ROM_BASE, &rom_base) >= 0) {
        lb->rom_base = rom_base;
    }
    #endif

    __lookahead_buf_reset(lb);
}

void __lookahead_buf_reset(__lookahead_buf_t *lb)
{
    lb->eof = false;
    lb->cur = 0;
    lb->ptr = lb->end = lb->buf[0];
    lb->rom_addr = lb->rom_base ? (lb->rom_base + (uint32_t)lseek(lb->fd, 0, SEEK_CUR)) : 0;

    #ifdef N64
    if (lb->rom_base) {
        // Prefetch the first buffer (the logic expects the first refill() to flip
        // cur and start prefetching the other buffer).
        int next = lb->cur ^ 1;
        data_cache_hit_invalidate(lb->buf[next], sizeof(lb->buf[0]));
        lb->ticket[next] = dma_read_raw_async(lb->buf[next], lb->rom_addr, sizeof(lb->buf[0]));
        lb->rom_addr += sizeof(lb->buf[0]);
    }
    #endif
}
