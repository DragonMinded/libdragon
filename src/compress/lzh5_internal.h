#ifndef LIBDRAGON_COMPRESS_LZH5_h
#define LIBDRAGON_COMPRESS_LZH5_h

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Size of the LZ5H decompression state.
 * 
 * Note that this can still be allocated on the stack, as the stack size
 * configured by libdragon is 64KB.
 */
#define DECOMPRESS_LZ5H_STATE_SIZE    18688

void decompress_lz5h_init(void *state, FILE *fp);
size_t decompress_lz5h_read(void *state, void *buf, size_t len);
int decompress_lz5h_pos(void *state);

/**
 * @brief Decompress a full LZ5H file into a buffer.
 * 
 * This function decompresses a full LZH5 file into a memory buffer.
 * The caller should provide a buffer large enough to hold the entire
 * file, or the function will fail.
 * 
 * This function is about 50% faster than using #decompress_lz5h_read,
 * as it can assume that the whole decoded file will always be available
 * during decoding.
 * 
 * @param fp        File pointer to the compressed file
 * @param buf       Buffer to decompress into
 * @param len       Length of the buffer
 */
size_t decompress_lz5h_full(FILE *fp, void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
