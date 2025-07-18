/**
 * @file shrinkler_dec_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_COMPRESS_SHRINKLER_DEC_INTERNAL_H
#define LIBDRAGON_COMPRESS_SHRINKLER_DEC_INTERNAL_H

/** @brief Set to 0 to disable assembly implementation of the full decoder */
#ifdef N64
#define DECOMPRESS_SHRINKLER_FULL_USE_ASM             1
#else
#define DECOMPRESS_SHRINKLER_FULL_USE_ASM             0
#endif

/**
 * @brief Size of the Shrinkler decompressor state structure, in bytes.
 */
#define DECOMPRESS_SHRINKLER_STATE_SIZE       512

#if DECOMPRESS_SHRINKLER_FULL_USE_ASM
int decompress_shrinkler_full_inplace(const uint8_t* in, size_t cmp_size, uint8_t *out, size_t size);
#else
/**
 * @brief Decompress a full Shrinkler-compressed file into a buffer.
 *
 * @param fn Filename of the file to decompress
 * @param fd File descriptor to read compressed data from.
 * @param cmp_size Size of the compressed data.
 * @param size Size of the decompressed data.
 * @return Newly-allocated buffer with decompressed contents
 */
void* decompress_shrinkler_full(const char *fn, int fd, size_t cmp_size, size_t size);
#endif

#endif

