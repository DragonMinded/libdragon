#ifndef SHRINKLER_COMPRESS_H
#define SHRINKLER_COMPRESS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
#endif
uint8_t *shrinkler_compress(const uint8_t *input, int dec_size, int level, int *cmp_size, int *inplace_margin);

#endif
