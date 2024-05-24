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
 * ## Asset compression
 * 
 * To compress your own data files, you can use the mkasset tool.
 * 
 * There are currently two compression levels:
 * 
 * * Level 1: this is based on LZ4 by Yann Collet. It is extremely fast and
 *   produce reasonable compression ratios. It is so fast at decompression
 *   that our implementation is typically faster at loading and decompressing
 *   a compressed asset, rather than loading it uncompressed. Libdragon tools
 *   will compress at level 1 by default.
 * * Level 2: this is based on LZH5 by Haruhiko Okumura, part of the LHA archiver.
 *   It is slower than LZ4, but it produces better compression ratios. It has
 *   been measured to beat gzip/zlib for small files like those typically used
 *   on N64. Level 2 should be selected if there is a necessity to squeeze data
 *   at the maximum ratio, at the expense of loading speed.
 * 
 * To minimize text siz and RAM usage, only the decompression code for level 1
 * is compiled by default. If you need to use level 2, you must call
 * #asset_init_compression(2).
 */

#include <stdio.h>

#ifdef N64
#include "debug.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// @cond
extern void __asset_init_compression_lvl2(void);
extern void __asset_init_compression_lvl3(void);
/// @endcond

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
    case 3: __asset_init_compression_lvl3(); break; \
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
 * @param fn        Filename to load (including filesystem prefix, eg: "rom:/foo.dat")
 * @param sz        If not NULL, this will be filed with the uncompressed size of the loaded file
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
 * Note that since the file might be compressed, the returned
 * FILE* cannot be arbitrarily seeked backward, as that would be impossible
 * to do efficiently on a compressed file. Seeking forward is supported and is
 * simulated by reading (decompressing) and discarding data. You can rewind
 * the file to the start though, (by using either fseek or rewind).
 * 
 * This behavior of the returned file is enforced also for non compressed
 * assets, so that the code is ready to switch to compressed assets if
 * required. If you need random access to an uncompressed file, simply use
 * the standard fopen() function.
 * 
 * @param fn        Filename to load (including filesystem prefix, eg: "rom:/foo.dat")
 * @param sz        If not NULL, this will be filed with the uncompressed size of the loaded file
 * @return FILE*    FILE pointer to use with standard C functions (fread, fclose)
 */
FILE *asset_fopen(const char *fn, int *sz);

#ifdef __cplusplus
}
#endif

#endif
