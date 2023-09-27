/**
 * @file asset.h
 * @brief Asset Subsystem
 * @ingroup asset
 */
#ifndef __LIBDRAGON_ASSET_H
#define __LIBDRAGON_ASSET_H

/**
 * @defgroup asset Asset Subsystem
 * @ingroup libdragon
 * @brief Interfaces for loading assets from ROM or other supports
 * 
 * The asset subsystem is in charge of loading assets. Typically, assets
 * will be loaded from ROM, but other options might be possible (like SD
 * cards).
 * 
 * Asset filenames are always prefixed with a filesystem identifier which
 * has a syntax similar to an URL. For instance, to load a file from ROM
 * through the DragonFS filesystem, use a filename like "rom:/myfile.txt".
 * 
 * While it is possible to simply open asset files using fopen, which supports
 * the filesystem prefix as well, the asset subsystem provides a few helpers
 * around asset compression.
 * 
 * Assets can be optionally compressed using the mkasset tool. Asset compression
 * is done on a per-file basis (similar to how "gzip" works), and decompression
 * is transparent to the user. The asset subsystem will automatically detect
 * a compressed file and decompress it during loading.
 * 
 * The main functions for loading assets are #asset_load and #asset_fopen.
 * #asset_load loads the entire file into memory in one go, and it is useful
 * for small files or in general files that has to fully keep in RAM as-is.
 * The asset is transparently decompressed if needed.
 * 
 * Some files might require parsing during loading, and in that case,
 * #asset_fopen is provided. It returns a FILE* so that any kind of file 
 * operation can be performed on it, with transparent decompression.
 * Since it is not possible to seek in a compressed file, the FILE* returned
 * by #asset_fopen will assert on seek, even if the file is not compressed
 * (so that the user code will be ready for adding compression at any time).
 * 
 * If you know that the file will never be compressed and you absolutely need
 * to freely seek, simply use the standard fopen() function.
 * 
 */

#include <stdio.h>

#ifdef N64
#include "debug.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// @private
extern void __asset_init_compression_lvl2(void);

/**
 * @brief Enable a non-default compression level
 * 
 * This function must be called if any asset that will be loaded use
 * a non-default compression level. The default compression level is 1,
 * for which no initialization is required.
 * 
 * Currently, only level 2 requires initialization. If you have any assets
 * compressed with level 2, you must call this function before loading them.
 * 
 * @code{.c}
 *      asset_init_compression(2); 
 * 
 *      // Load an asset that might use level 2 compression
 *      sprite_t *hero = sprite_load("rom:/hero.sprite");
 * @endcode
 * 
 * @param level     Compression level to initialize
 * 
 * @see #asset_load
 * @hideinitializer
 */
#define asset_init_compression(level) ({ \
    switch (level) { \
    case 1: break; \
    case 2: __asset_init_compression_lvl2(); break; \
    default: assertf(0, "Unsupported compression level: %d", level); \
    } \
})

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
 * @param fn        Filename to open (including filesystem prefix)
 * @param sz        If not NULL, pointer to an integer where the size of the file will be stored
 * @return FILE*    FILE pointer to use with standard C functions (fread, fclose)
 */
FILE *asset_fopen(const char *fn, int *sz);

#ifdef __cplusplus
}
#endif

#endif
