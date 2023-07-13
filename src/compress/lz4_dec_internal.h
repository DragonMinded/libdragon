#include <stdlib.h>
#include <stdbool.h>

int lz4ultra_decompressor_expand_block(const unsigned char *pInBlock, int nBlockSize, unsigned char *pOutData, int nOutDataOffset, int nBlockMaxSize, bool dma_race);
size_t lz4ultra_decompress_inmem(const unsigned char *pFileData, unsigned char *pOutBuffer, size_t nFileSize, size_t nMaxOutBufferSize, unsigned int nFlags);
