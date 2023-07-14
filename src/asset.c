#include "asset.h"
#include "asset_internal.h"
#include "compress/lzh5_internal.h"
#include "compress/lz4_dec_internal.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdalign.h>

#ifdef N64
#include <malloc.h>
#include "debug.h"
#include "n64sys.h"
#include "dma.h"
#include "dragonfs.h"
#else
#include <stdlib.h>
#include <assert.h>
#define memalign(a, b) malloc(b)
#define assertf(x, ...) assert(x)
#endif

#define LZ4_DECOMPRESS_INPLACE_MARGIN(compressedSize)          (((compressedSize) >> 8) + 32)

FILE *must_fopen(const char *fn)
{
    FILE *f = fopen(fn, "rb");
    if (!f) {
        // File not found.
        int errnum = errno;
        if (errnum == EINVAL) {
            if (!strstr(fn, ":/")) {
                // A common mistake is to forget the filesystem prefix.
                // Try to give a hint if that's the case.
                assertf(f, "File not found: %s\n"
                    "Did you forget the filesystem prefix? (e.g. \"rom:/\")\n", fn);
                return NULL;
            } else if (strstr(fn, "rom:/")) {
                // Another common mistake is to forget to initialize the rom filesystem.
                // Suggest that if the filesystem prefix is "rom:/".
                assertf(f, "File not found: %s\n"
                    "Did you forget to call dfs_init(), or did it return an error?\n", fn);
                return NULL;
            }
        }
        assertf(f, "error opening file %s: %s\n", fn, strerror(errnum));
    }
    return f;
}

void *asset_load(const char *fn, int *sz)
{
    uint8_t *s; int size;
    FILE *f = must_fopen(fn);
   
    // Check if file is compressed
    asset_header_t header;
    fread(&header, 1, sizeof(asset_header_t), f);
    if (!memcmp(header.magic, ASSET_MAGIC, 4)) {
        #ifndef N64
        header.algo = __builtin_bswap16(header.algo);
        header.flags = __builtin_bswap16(header.flags);
        header.cmp_size = __builtin_bswap32(header.cmp_size);
        header.orig_size = __builtin_bswap32(header.orig_size);
        #endif

        switch (header.algo) {
        case 2: {
            size = header.orig_size;
            s = memalign(16, size);
            int n = decompress_lz5h_full(f, s, size); (void)n;
            assertf(n == size, "asset: decompression error on file %s: corrupted? (%d/%d)", fn, n, size);
        }   break;
        case 1: {
            size = header.orig_size;
            int bufsize = size + LZ4_DECOMPRESS_INPLACE_MARGIN(header.cmp_size);
            int cmp_offset = bufsize - header.cmp_size;
            if (cmp_offset & 1) {
                cmp_offset++;
                bufsize++;
            }
            if (bufsize & 15) {
                // In case we need to call invalidate (see below), we need an aligned buffer
                bufsize += 16 - (bufsize & 15);
            }

            s = memalign(16, bufsize);
            int n;

            #ifdef N64
            if (strncmp(fn, "rom:/", 5) == 0) {
                // Invalid the portion of the buffer where we are going to load
                // the compressed data. This is needed in case the buffer returned
                // by memalign happens to be in cached already.
                int align_cmp_offset = cmp_offset & ~15;
                data_cache_hit_invalidate(s+align_cmp_offset, bufsize-align_cmp_offset);

                // Loading from ROM. This is a common enough situation that we want to optimize it.
                // Start an asynchronous DMA transfer, so that we can start decompressing as the
                // data flows in.
                uint32_t addr = dfs_rom_addr(fn+5) & 0x1FFFFFFF;
                dma_read_async(s+cmp_offset, addr+16, header.cmp_size);

                // Run the decompression racing with the DMA.
                n = lz4ultra_decompressor_expand_block(s+cmp_offset, header.cmp_size, s, 0, size, true); (void)n;
            #else
            if (false) {
            #endif
            } else {
                // Standard loading via stdio. We have to wait for the whole file to be read.
                fread(s+cmp_offset, 1, header.cmp_size, f);

                // Run the decompression.
                n = lz4ultra_decompressor_expand_block(s+cmp_offset, header.cmp_size, s, 0, size, false); (void)n;
            }
            assertf(n == size, "asset: decompression error on file %s: corrupted? (%d/%d)", fn, n, size);
            void *ptr = realloc(s, size); (void)ptr;
            assertf(s == ptr, "asset: realloc moved the buffer"); // guaranteed by newlib
        }   break;
        default:
            assertf(0, "asset: unsupported compression algorithm: %d", header.algo);
            return NULL;
        }        
    } else {
        // Allocate a buffer big enough to hold the file.
        // We force a 16-byte alignment for the buffer so that it's cacheline aligned.
        // This might or might not be useful, but if a binary file is laid out so that it
        // matters, at least we guarantee that. 
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        s = memalign(16, size);

        fseek(f, 0, SEEK_SET);
        fread(s, 1, size, f);
    }

    fclose(f);
    if (sz) *sz = size;
    return s;
}

#ifdef N64

typedef struct  {
    FILE *fp;
    bool seeked;
} cookie_none_t;

static fpos_t seekfn_none(void *c, fpos_t pos, int whence)
{
    cookie_none_t *cookie = c;

    // SEEK_CUR with pos=0 is used as ftell()
    if (whence == SEEK_CUR && pos == 0)
        return ftell(cookie->fp);

    cookie->seeked = true;
    return -1;
}

static int readfn_none(void *c, char *buf, int sz)
{
    cookie_none_t *cookie = c;
    assertf(!cookie->seeked, "Cannot seek in file opened via asset_fopen (it might be compressed)");
    return fread(buf, 1, sz, cookie->fp);
}

static int closefn_none(void *c)
{
    cookie_none_t *cookie = c;
    fclose(cookie->fp); cookie->fp = NULL;
    free(cookie);
    return 0;
}

typedef struct  {
    FILE *fp;
    int pos;
    bool seeked;
    uint8_t state[DECOMPRESS_LZ5H_STATE_SIZE] alignas(8);
} cookie_lha_t;

static int readfn_lha(void *c, char *buf, int sz)
{
    cookie_lha_t *cookie = (cookie_lha_t*)c;
    assertf(!cookie->seeked, "Cannot seek in file opened via asset_fopen (it might be compressed)");
    int n = decompress_lz5h_read(cookie->state, (uint8_t*)buf, sz);
    cookie->pos += n;
    return n;
}

static fpos_t seekfn_lha(void *c, fpos_t pos, int whence)
{
    cookie_lha_t *cookie = (cookie_lha_t*)c;

    // SEEK_CUR with pos=0 is used as ftell()
    if (whence == SEEK_CUR && pos == 0)
        return cookie->pos;

    // We should really have an assert here but unfortunately newlib's fclose
    // also issue a fseek (backward...) as part of a fflush. So we delay the actual
    // assert until the next read (if any), which is better than nothing.
    cookie->seeked = true;
    return -1;
}

static int closefn_lha(void *c)
{
    cookie_lha_t *cookie = (cookie_lha_t*)c;
    fclose(cookie->fp); cookie->fp = NULL;
    free(cookie);
    return 0;
}

FILE *asset_fopen(const char *fn, int *sz)
{
    FILE *f = must_fopen(fn);

    // We use buffering on the outer file created by funopen, so we don't
    // actually need buffering on the underlying one.
    setbuf(f, NULL);

    // Check if file is compressed
    asset_header_t header;
    fread(&header, 1, sizeof(asset_header_t), f);
    if (!memcmp(header.magic, ASSET_MAGIC, 4)) {
        if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) {  // for mkasset running on PC
            header.algo = __builtin_bswap16(header.algo);
            header.flags = __builtin_bswap16(header.flags);
            header.cmp_size = __builtin_bswap32(header.cmp_size);
            header.orig_size = __builtin_bswap32(header.orig_size);
        }

        cookie_lha_t *cookie = malloc(sizeof(cookie_lha_t));
        cookie->fp = f;
        cookie->pos = 0;
        cookie->seeked = false;
        decompress_lz5h_init(cookie->state, f);
        if (sz) *sz = header.orig_size;
        return funopen(cookie, readfn_lha, NULL, seekfn_lha, closefn_lha);
    }

    // Not compressed. Return a wrapped FILE* without the seeking capability,
    // so that it matches the behavior of the compressed file.
    if (sz) {
        fseek(f, 0, SEEK_END);
        *sz = ftell(f);
    }
    fseek(f, 0, SEEK_SET);
    cookie_none_t *cookie = malloc(sizeof(cookie_none_t));
    cookie->fp = f;
    cookie->seeked = false;
    return funopen(cookie, readfn_none, NULL, seekfn_none, closefn_none);
}

#endif /* N64 */
