/*
    assetcomp: compression library for the N64
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
#ifndef COMMON_ASSETCOMP_H
#define COMMON_ASSETCOMP_H

#include <stdio.h>
#include <stdint.h>

#define DEFAULT_COMPRESSION     1
#define MAX_COMPRESSION         3

// Default window size for streaming decompression (asset_fopen())
#define DEFAULT_WINSIZE_STREAMING    (4*1024)

#ifdef __cplusplus
extern "C" {
#endif

bool asset_compress(const char *infn, const char *outfn, int compression, int winsize);
int asset_compress_mem(void *data, int sz, FILE *out, int compression, int winsize, int *margin);
void asset_compress_mem_raw(int compression, const uint8_t *inbuf, int size, uint8_t **outbuf, int *cmp_size, int *winsize, int *margin);

#ifdef __cplusplus
}
#endif

#endif
