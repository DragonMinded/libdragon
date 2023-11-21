#ifndef LIBDRAGON_COMPRESS_APLIB_DEC_INTERNAL_H
#define LIBDRAGON_COMPRESS_APLIB_DEC_INTERNAL_H

#include <stdio.h>

#define DECOMPRESS_APLIB_STATE_SIZE       348

void decompress_aplib_init(void *state, FILE *fp, int winsize);
ssize_t decompress_aplib_read(void *state, void *buf, size_t len);
void* decompress_aplib_full(const char *fn, FILE *fp, size_t cmp_size, size_t size);

#endif
