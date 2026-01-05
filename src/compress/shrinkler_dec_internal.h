/**
 * @file shrinkler_dec_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_COMPRESS_SHRINKLER_DEC_INTERNAL_H
#define LIBDRAGON_COMPRESS_SHRINKLER_DEC_INTERNAL_H

// Keep this header self-contained (used both by libdragon and host tools)
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

/**
 * @brief Size of the Shrinkler decompressor state structure, in bytes.
 */
#define DECOMPRESS_SHRINKLER_STATE_SIZE       2304

/** @brief Initialize Shrinkler streaming decompressor */
void decompress_shrinkler_init(void *state, int fd, int winsize);
/** @brief Read decompressed data from Shrinkler streaming decompressor */
ssize_t decompress_shrinkler_read(void *state, void *buf, size_t len);
/** @brief Reset Shrinkler streaming decompressor state */
void decompress_shrinkler_reset(void *state);

#ifdef N64
int decompress_shrinkler_full_inplace(const uint8_t* in, size_t cmp_size, uint8_t *out, size_t size);
#endif /* N64 */

#endif

