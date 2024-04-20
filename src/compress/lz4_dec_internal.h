#ifndef LIBDRAGON_COMPRESS_LZ4_DEC_INTERNAL_H
#define LIBDRAGON_COMPRESS_LZ4_DEC_INTERNAL_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Calculate the margin required for in-place decompression.
 * 
 * It is possible to perform in-place decompression of LZ4 data: to do so,
 * allocate a buffer large enough to hold the decompressed data, plus some
 * margin calculated through this function. Then, read the compressed
 * data at the end of the buffer. Finally, call #decompress_lz4_full_mem.
 * 
 * Example:
 * 
 * @code{.c}
 *      // Allocate a buffer large enough to hold the decompressed data,
 *      // pluse the inplace margin.
 *      int buf_size = decompressed_size + LZ4_DECOMPRESS_INPLACE_MARGIN(compressed_size);
 *      void *buf = malloc(buf_size);
 * 
 *      // Read compressed data at the end of the buffer
 *      fread(buf + buf_size - compressed_size, 1, compressed_size, fp);
 * 
 *      // Decompress
 *      decompress_lz4_full_mem(
 *          buf + buf_size - compressed_size, compressed_size,
 *          buf, decompressed_size,
 *          false);
 * @endcode
 */
#define LZ4_DECOMPRESS_INPLACE_MARGIN(compressed_size)    (((compressed_size) >> 8) + 32)

/**
 * @brief Decompress a block of LZ4 data (mem to mem).
 * 
 * This function run a LZ4 decompressor on a block of data, from memory to
 * memory. 
 * 
 * LZ4 is much faster than PI DMA. To benefit even more from this, it is possible
 * to actually run this function in parallel with the DMA transfer, "racing"
 * with it. If called with @p dma_race set to true, the function will assume
 * that the source buffer is currently being DMAed into memory, and will
 * throttle itself to never read past the current DMA position.
 * 
 * In addition to this, it is possible to in-place decompress a block of data.
 * See #LZ4_DECOMPRESS_INPLACE_MARGIN for more information.
 * 
 * @param src           Pointer to source buffer (compressed data)
 * @param src_size      Size of the compressed data in bytes
 * @param dst           Pointer to destination buffer (decompressed data)
 * @param dst_size      Size of the destination buffer in bytes
 * @return int          Number of bytes decompressed, or -1 on error.
 */
int decompress_lz4_full_inplace(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size);


#define DECOMPRESS_LZ4_STATE_SIZE  176

void decompress_lz4_init(void *state, FILE *fp, int winsize);
ssize_t decompress_lz4_read(void *state, void *buf, size_t len);
void decompress_lz4_reset(void *state);
void* decompress_lz4_full(const char *fn, FILE *fp, size_t cmp_size, size_t size);

#endif
