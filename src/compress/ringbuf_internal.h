#ifndef LIBDRAGON_COMPRESS_RINGBUF_INTERNAL_H
#define LIBDRAGON_COMPRESS_RINGBUF_INTERNAL_H

#include <stdint.h>

/**
 * @brief A ring buffer used for streaming decompression.
 */
typedef struct {
	uint8_t* ringbuf;                       ///< The ring buffer itself
    unsigned int ringbuf_size;              ///< Size of the ring buffer (power of two)
	unsigned int ringbuf_pos;               ///< Current write position in the ring buffer
} decompress_ringbuf_t;


void __ringbuf_init(decompress_ringbuf_t *ringbuf, uint8_t *buf, int winsize);

static inline void __ringbuf_writebyte(decompress_ringbuf_t *ringbuf, uint8_t byte)
{
    ringbuf->ringbuf[ringbuf->ringbuf_pos++] = byte;
    ringbuf->ringbuf_pos &= ringbuf->ringbuf_size-1;
}

/**
 * @brief Write an array of bytes into the ring buffer.
 * 
 * @param ringbuf   The ring buffer to write to.
 * @param src       The source array to write from.
 * @param count     The number of bytes to write.
 */
void __ringbuf_write(decompress_ringbuf_t *ringbuf, uint8_t *src, int count);

/**
 * @brief Extract data from the ring buffer, updating it at the same time
 * 
 * This function is used to implement a typical match-copy of LZ algorithms.
 * Given the ring buffer and the position to copy from, it will copy the
 * specified number of bytes into the destination buffer, while also
 * updating the ring buffer with the copied data.
 * 
 * It correctly handles overlaps, so if copy_offset is 1 and count is 100,
 * the last character in the ring buffer will be copied 100 times to the
 * output (and to the ring buffer itself).
 * 
 * @param ringbuf               The ring buffer
 * @param copy_offset           Offset to copy from, relative to the current position.
 * @param dst                   Destination buffer
 * @param count                 Number of bytes to copy
 */
void __ringbuf_copy(decompress_ringbuf_t *ringbuf, int copy_offset, uint8_t *dst, int count);

#endif
