#include "ringbuf_internal.h"
#include "../utils.h"
#include <assert.h>

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
