#ifndef COMMON_ASSETCOMP_H
#define COMMON_ASSETCOMP_H

#define DEFAULT_COMPRESSION     1
#define MAX_COMPRESSION         2

// Default window size for streaming decompression (asset_fopen())
#define DEFAULT_WINSIZE_STREAMING    (4*1024)

bool asset_compress(const char *infn, const char *outfn, int compression, int winsize);

#endif
