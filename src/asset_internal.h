#ifndef __LIBDRAGON_ASSET_INTERNAL_H
#define __LIBDRAGON_ASSET_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

#include <stdio.h>

#define ASSET_MAGIC                 "DCA"   ///< Magic compressed asset header
#define ASSET_FLAG_WINSIZE_MASK     0x0007  ///< Mask to isolate the window size in the flags
#define ASSET_FLAG_WINSIZE_16K      0x0000  ///< 16 KiB window size
#define ASSET_FLAG_WINSIZE_8K       0x0001  ///< 8 KiB window size
#define ASSET_FLAG_WINSIZE_4K       0x0002  ///< 4 KiB window size
#define ASSET_FLAG_WINSIZE_2K       0x0003  ///< 2 KiB window size
#define ASSET_FLAG_WINSIZE_32K      0x0004  ///< 32 KiB window size
#define ASSET_FLAG_WINSIZE_64K      0x0005  ///< 64 KiB window size
#define ASSET_FLAG_WINSIZE_128K     0x0006  ///< 128 KiB window size
#define ASSET_FLAG_WINSIZE_256K     0x0007  ///< 256 KiB window size
#define ASSET_FLAG_INPLACE          0x0100  ///< Decompress in-place
#define ASSET_ALIGNMENT             32      ///< Aligned to instruction cacheline

__attribute__((used))
static inline int asset_buf_size(int size, int cmp_size, int margin, int *cmp_offset_dst)
{
    // add 8 because the assembly decompressors do writes up to 8 bytes out-of-bounds,
    // that could overwrite the input data.
    margin += 8;
    int bufsize = size + margin;
    int cmp_offset = bufsize - cmp_size;
    // Align the source buffer to 4 bytes, so that we can use 32-bit loads (required by shrinkler).
    // Notice that we need at least 2-byte alignment anyway, for DMA.
    while (cmp_offset & 3) {
        cmp_offset++;
        bufsize++;
    }
    if(cmp_offset_dst) {
        *cmp_offset_dst = cmp_offset;
    }
    if (bufsize & 15) {
        // In case we need to call invalidate (see below), we need an aligned buffer
        bufsize += 16 - (bufsize & 15);
    }
    return bufsize;
}

__attribute__((used))
static inline int asset_winsize_from_flags(uint16_t flags) {
    flags &= ASSET_FLAG_WINSIZE_MASK;
    if (flags & 4) 
        return (2*1024) << flags;
    else
        return (16*1024) >> flags;
}

__attribute__((used))
static int asset_winsize_to_flags(int winsize) {
    if (winsize == 16*1024)  return ASSET_FLAG_WINSIZE_16K;
    if (winsize == 8*1024)   return ASSET_FLAG_WINSIZE_8K;
    if (winsize == 4*1024)   return ASSET_FLAG_WINSIZE_4K;
    if (winsize == 2*1024)   return ASSET_FLAG_WINSIZE_2K;
    if (winsize == 32*1024)  return ASSET_FLAG_WINSIZE_32K;
    if (winsize == 64*1024)  return ASSET_FLAG_WINSIZE_64K;
    if (winsize == 128*1024) return ASSET_FLAG_WINSIZE_128K;
    if (winsize == 256*1024) return ASSET_FLAG_WINSIZE_256K;
    return -1;
}

/** @brief Header of a compressed asset */
typedef struct {
    char magic[3];          ///< Magic header
    uint8_t version;        ///< Version of the asset header
    uint16_t algo;          ///< Compression algorithm
    uint16_t flags;         ///< Flags
    uint32_t cmp_size;      ///< Compressed size in bytes
    uint32_t orig_size;     ///< Original size in bytes
    uint32_t inplace_margin; ///< Margin for in-place decompression
} asset_header_t;

_Static_assert(sizeof(asset_header_t) == 20, "invalid sizeof(asset_header_t)");

/** @brief A decompression algorithm used by the asset library */
typedef struct {
    int state_size;     ///< Basic size of the decompression state (without ringbuffer)

    /** @brief Initialize the decompression state */
    void (*decompress_init)(void *state, int fd, int winsize);

    /** @brief Partially read a decompressed file from a state */
    ssize_t (*decompress_read)(void *state, void *buf, size_t len);

    /** @brief Reset decompression state after rewind */
    void (*decompress_reset)(void *state);

    /** @brief Decompress a full file in one go */
    bool (*decompress_full)(int fd, size_t cmp_size, size_t len, void *buf, int *buf_size);

    /** @brief Decompress a full file in-place */
    int (*decompress_full_inplace)(const uint8_t *in, size_t cmp_size, uint8_t *out, size_t len);
} asset_compression_t;


FILE *must_fopen(const char *fn);

#endif
