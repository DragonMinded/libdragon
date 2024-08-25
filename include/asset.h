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

/** @brief Minimum required alignment for assets */
#define ASSET_ALIGNMENT_MIN 16  ///< Aligned to data cacheline


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
 * @brief Load an asset file (possibly uncompressing it)
 * 
 * This function loads an asset embedded within a larger file. It requires in
 * input an open file pointer, seeked to the beginning of the asset, and the
 * size of the asset itself. If the asset is compressed, it is transparently
 * decompressed.
 *
 * After this function returns, for technical reasons, the position of the
 * provided file pointer becomes undefined. If you need to use it again, make
 * sure to seek it.
 *
 * A memory buffer to hold the uncompressed asset is automatically allocated
 * and returned. It must be freed using free() when the buffer is not required
 * anymore. The memory is guaranteed to be aligned by at least #ASSET_ALIGNMENT_MIN.
 * 
 * @param f         pre-seeked file pointer, pointing to a valid asset header (or
 *                  actual data if uncompressed)
 * @param sz        size of input data (compressed or not). It will be filled
 *                  the uncompressed asset size, which is equal to the input value if the
 *                  asset is not compressed.
 * @return void*    Allocated buffer filled with the uncompressed asset content
 */
void* asset_loadf(FILE *f, int *sz);


/**
  * @brief Load an asset file (possibly uncompressing it)
  *
  * This is the lowest-level asset loading function, that is
  * needed only for advanced use cases. In general, prefer using
  * any of other variants if possible, as the other APIs are
  * harder to misuse.
  * 
  * This function loads an asset potentially embedded within a
  * larger, opened file. It requires an open file pointer, seeked
  * to the beginning of the asset, and the size of the asset itself.
  * If the asset is compressed, it is transparently decompressed.
  *
  * After this function returns, for technical reasons, the position
  * of the provided file pointer becomes undefined. If you need to
  * use it again, make sure to seek it.
  * 
  * The memory buffer to hold the uncompressed asset must be provided as
  * input, together with its size. If the provided buffer is too small (or
  * it is NULL), the function does not load the asset and returns false,
  * change buf_size to contain the minimum required size for the buffer.
  * Notice that the minimum buffer size might be slightly larger than
  * the uncompressed asset size, because some extra space might be required
  * to perform in-place decompression. The minimum buffer size can either
  * be calculated a build time (the assetcomp library exposes a function to
  * do so), or queried at runtime by simply calling this function with a NULL
  * input buffer.
  *
  * @param f         pre-seeked file pointer, pointing to a valid asset header
  *                  (or actual data if uncompressed)
  * @param sz        [in/out]: size of input data (compressed or not). It will
  *                  be filled the uncompressed asset size, which is equal to
  *                  the input value if the asset is not compressed.
  * @param buf       Pointer to the buffer where data must be loaded into. 
  *                  If the buffer pointer is NULL, or it is too small,
  *                  asset_loadf_into will fail.
  * @param buf_size  [in/out]: Size of the provided input buffer. Changed to 
  *                  minimum required size, if it was too small.
  *
  * @return true The function has succeeded and the asset was loaded
  * @return false The function has failed because the provided buffer was too small.
  *               In this case, *buf_size is changed to contain the minimum size
  *               that is required to load this asset.
  */
bool asset_loadf_into(FILE *f, int *sz, void *buf, int *buf_size);

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
