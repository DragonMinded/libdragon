#ifndef __LIBDRAGON_ASSET_H
#define __LIBDRAGON_ASSET_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load an asset file (possibly uncompressing it)
 * 
 * This function loads a file from a file system (eg: from ROM or SD).
 * If the file was compressed using the mkasset tool, it will be
 * automatically uncompressed.
 * 
 * @param fn        Filename to load (including filesystem prefix)
 * @param sz        Pointer to an integer where the size of the file will be stored
 * @return void*    Pointer to the loaded file (must be freed with free() when done)
 */
void *asset_load(const char *fn, int *sz);

/**
 * @brief Open an asset file for reading (with transparent decompression)
 * 
 * This function opens a file from a file system (eg: from ROM or SD).
 * If the file was compressed using the mkasset tool, it will be
 * automatically uncompressed as it is being read.
 * 
 * Note that since the file can be optionally compressed, the returned
 * FILE* cannot be rewinded. It must be read sequentially, or seeked forward.
 * Seeking backward is not supported.
 * 
 * @param fn 
 * @return FILE* 
 */
FILE *asset_fopen(const char *fn);

#ifdef __cplusplus
}
#endif

#endif
