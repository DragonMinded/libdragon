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

/** @brief Set to 0 to disable assembly implementation of the full decoder */
#ifdef N64
#define DECOMPRESS_SHRINKLER_FULL_USE_ASM             1
#else
#define DECOMPRESS_SHRINKLER_FULL_USE_ASM             0
#endif

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

#if DECOMPRESS_SHRINKLER_FULL_USE_ASM
int decompress_shrinkler_full_inplace(const uint8_t* in, size_t cmp_size, uint8_t *out, size_t size);
#else
/**
 * @brief Decompress a full Shrinkler-compressed file into a buffer.
 *
 * @param fd File descriptor to read compressed data from.
 * @param cmp_size Size of the compressed data.
 * @param size Size of the decompressed data.
 * @param buf Buffer to store decompressed data.
 * @param buf_size Pointer to the size of the buffer; updated with required size.
 * @return true on success, false on failure.
 */
bool decompress_shrinkler_full(int fd, size_t cmp_size, size_t size, void *buf, int *buf_size);
#endif

#endif

