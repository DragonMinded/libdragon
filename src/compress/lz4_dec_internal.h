#ifndef LIBDRAGON_COMPRESS_LZ4_DEC_INTERNAL_H
#define LIBDRAGON_COMPRESS_LZ4_DEC_INTERNAL_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define DECOMPRESS_LZ4_STATE_SIZE  (16552)

int lz4ultra_decompressor_expand_block(const unsigned char *pInBlock, int nBlockSize, unsigned char *pOutData, int nOutDataOffset, int nBlockMaxSize, bool dma_race);
size_t lz4ultra_decompress_inmem(const unsigned char *pFileData, unsigned char *pOutBuffer, size_t nFileSize, size_t nMaxOutBufferSize, unsigned int nFlags);

void decompress_lz4_init(void *state, FILE *fp);
ssize_t decompress_lz4_read(void *state, void *buf, size_t len);

#endif
