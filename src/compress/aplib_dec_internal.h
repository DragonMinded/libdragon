#ifndef LIBDRAGON_COMPRESS_APLIB_DEC_INTERNAL_H
#define LIBDRAGON_COMPRESS_APLIB_DEC_INTERNAL_H

/** @brief Set to 0 to disable assembly implementation of the full decoder */
#ifdef N64
#define DECOMPRESS_APLIB_FULL_USE_ASM             1
#else
#define DECOMPRESS_APLIB_FULL_USE_ASM             0
#endif

#include <stdio.h>

#define DECOMPRESS_APLIB_STATE_SIZE       348

void decompress_aplib_init(void *state, int fd, int winsize);
ssize_t decompress_aplib_read(void *state, void *buf, size_t len);
void decompress_aplib_reset(void *state);

#if DECOMPRESS_APLIB_FULL_USE_ASM
int decompress_aplib_full_inplace(const uint8_t* in, size_t cmp_size, uint8_t *out, size_t size);
#else
void* decompress_aplib_full(const char *fn, int fd, size_t cmp_size, size_t size);
#endif

#endif
