#ifndef LIBDRAGON_COMPRESS_SHRINKLER_DEC_INTERNAL_H
#define LIBDRAGON_COMPRESS_SHRINKLER_DEC_INTERNAL_H

/** @brief Set to 0 to disable assembly implementation of the full decoder */
#ifdef N64
#define DECOMPRESS_SHRINKLER_FULL_USE_ASM             1
#else
#define DECOMPRESS_SHRINKLER_FULL_USE_ASM             0
#endif

#define DECOMPRESS_SHRINKLER_STATE_SIZE       512

#if DECOMPRESS_SHRINKLER_FULL_USE_ASM
int decompress_shrinkler_full_inplace(const uint8_t* in, size_t cmp_size, uint8_t *out, size_t size);
#else
bool decompress_shrinkler_full(int fd, size_t cmp_size, size_t size, void *buf, int *buf_size);
#endif

#endif

