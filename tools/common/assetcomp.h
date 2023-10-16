#ifndef COMMON_ASSETCOMP_H
#define COMMON_ASSETCOMP_H

#define DEFAULT_COMPRESSION     1
#define MAX_COMPRESSION         2

bool asset_compress(const char *infn, const char *outfn, int compression);

#endif
