/**
 * @file aplib_dec_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_COMPRESS_APLIB_DEC_INTERNAL_H
#define LIBDRAGON_COMPRESS_APLIB_DEC_INTERNAL_H

#include <stdio.h>

/** @brief Size of APLIB decompressor state */
#define DECOMPRESS_APLIB_STATE_SIZE       348

/** @brief Initialize APLIB decompressor */
void decompress_aplib_init(void *state, int fd, int winsize);
/** @brief Read decompressed data from APLIB decompressor */
ssize_t decompress_aplib_read(void *state, void *buf, size_t len);
/** @brief Reset APLIB decompressor state */
void decompress_aplib_reset(void *state);

#ifdef N64
/** @brief Decompress APLIB data in-place using fast assembly implementation */
int decompress_aplib_full_inplace(const uint8_t* in, size_t cmp_size, uint8_t *out, size_t size);
#endif /* N64 */

#endif
