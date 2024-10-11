#ifndef COMMON_ASSETCOMP_H
#define COMMON_ASSETCOMP_H

#define DEFAULT_COMPRESSION     1
#define MAX_COMPRESSION         3

// Default window size for streaming decompression (asset_fopen())
#define DEFAULT_WINSIZE_STREAMING    (4*1024)

#ifdef __cplusplus
extern "C" {
#endif

bool asset_compress(const char *infn, const char *outfn, int compression, int winsize);
void asset_compress_mem(int compression, const uint8_t *inbuf, int size, uint8_t **outbuf, int *cmp_size, int *winsize, int *margin);

#ifdef __cplusplus
}
#endif

#endif
