/**
 * @file lzh5_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_COMPRESS_LZH5_h
#define LIBDRAGON_COMPRESS_LZH5_h

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Size of the LZH5 decompression state.
 * 
 * Note that this can still be allocated on the stack, as the stack size
 * configured by libdragon is 64KB.
 */
#define DECOMPRESS_LZH5_STATE_SIZE            (6096+16)

/**
 * @brief Default window size for LZH5 decompression.
 */
#define DECOMPRESS_LZH5_DEFAULT_WINDOW_SIZE   (8192)

/**
 * @brief Initialize the LZH5 decompressor state.
 *
 * @param state Pointer to the decompressor state buffer.
 * @param fp FILE pointer to read compressed data from.
 * @param winsize Window size for the decompressor.
 */
void decompress_lzh5_init(void *state, FILE *fp, int winsize);

/**
 * @brief Read decompressed data from the LZH5 stream.
 *
 * @param state Pointer to the decompressor state buffer.
 * @param buf Buffer to store decompressed data.
 * @param len Number of bytes to read.
 * @return Number of bytes read, or -1 on error.
 */
ssize_t decompress_lzh5_read(void *state, void *buf, size_t len);

/**
 * @brief Get the current position in the decompressed stream.
 *
 * @param state Pointer to the decompressor state buffer.
 * @return Current position in the decompressed stream.
 */
int decompress_lzh5_pos(void *state);

/**
 * @brief Decompress a full LZH5 file into a buffer.
 * 
 * This function decompresses a full LZH5 file into a memory buffer.
 * The caller should provide a buffer large enough to hold the entire
 * file, or the function will fail.
 * 
 * This function is about 50% faster than using #decompress_lzh5_read,
 * as it can assume that the whole decoded file will always be available
 * during decoding.
 * 
 * @param fn        Filename of the file being decompressed, if known
 * @param fp        File pointer to the compressed file
 * @param cmp_size  Length of the compressed file
 * @param size      Length of the file after decompression
 * @return          Buffer that contains the decompressed file
 */
void* decompress_lzh5_full(const char *fn, FILE *fp, size_t cmp_size, size_t size);

#ifdef __cplusplus
}
#endif

#endif
