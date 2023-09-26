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

/** 
 * @brief Compression algorithms
 * 
 * Only level 1 (LZ4) is always initialized. The other algorithm (LZH5)
 * must be initialized manually via #asset_init_compression.
 */
static asset_compression_t algos[2] = {
    {
        .state_size = DECOMPRESS_LZ4_STATE_SIZE,
        .decompress_init = decompress_lz4_init,
        .decompress_read = decompress_lz4_read,
        .decompress_full = decompress_lz4_full,
    }
};

void __asset_init_compression_lvl2(void)
{
    algos[1] = (asset_compression_t){
        .state_size = DECOMPRESS_LZH5_STATE_SIZE,
        .decompress_init = decompress_lzh5_init,
        .decompress_read = decompress_lzh5_read,
        .decompress_full = decompress_lzh5_full,
    };
}

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
    if (!memcmp(header.magic, ASSET_MAGIC, 3)) {
        if (header.version != '2') {
            assertf(0, "unsupported asset version: %c\nMake sure to rebuild libdragon tools and your assets", header.version);
            return NULL;
        }

        #ifndef N64
        header.algo = __builtin_bswap16(header.algo);
        header.flags = __builtin_bswap16(header.flags);
        header.cmp_size = __builtin_bswap32(header.cmp_size);
        header.orig_size = __builtin_bswap32(header.orig_size);
        #endif

        assertf(header.algo >= 1 || header.algo <= 2,
            "unsupported compression algorithm: %d", header.algo);
        assertf(algos[header.algo-1].decompress_full, 
            "asset: compression level %d not initialized. Call asset_init_compression(%d) at initialization time", header.algo, header.algo);

        size = header.orig_size;
        s = algos[header.algo-1].decompress_full(fn, f, header.cmp_size, size);
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
    ssize_t (*read)(void *state, void *buf, size_t len);
    uint8_t state[] alignas(8);
} cookie_cmp_t;

static int readfn_cmp(void *c, char *buf, int sz)
{
    cookie_cmp_t *cookie = (cookie_cmp_t*)c;
    assertf(!cookie->seeked, "Cannot seek in file opened via asset_fopen (it might be compressed)");
    int n = cookie->read(cookie->state, (uint8_t*)buf, sz);
    cookie->pos += n;
    return n;
}

static fpos_t seekfn_cmp(void *c, fpos_t pos, int whence)
{
    cookie_cmp_t *cookie = (cookie_cmp_t*)c;

    // SEEK_CUR with pos=0 is used as ftell()
    if (whence == SEEK_CUR && pos == 0)
        return cookie->pos;

    // We should really have an assert here but unfortunately newlib's fclose
    // also issue a fseek (backward...) as part of a fflush. So we delay the actual
    // assert until the next read (if any), which is better than nothing.
    cookie->seeked = true;
    return -1;
}

static int closefn_cmp(void *c)
{
    cookie_cmp_t *cookie = (cookie_cmp_t*)c;
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
    if (!memcmp(header.magic, ASSET_MAGIC, 3)) {
        if (header.version != '2') {
            assertf(0, "unsupported asset version: %c\nMake sure to rebuild libdragon tools and your assets", header.version);
            return NULL;
        }

        if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) {  // for mkasset running on PC
            header.algo = __builtin_bswap16(header.algo);
            header.flags = __builtin_bswap16(header.flags);
            header.cmp_size = __builtin_bswap32(header.cmp_size);
            header.orig_size = __builtin_bswap32(header.orig_size);
        }

        cookie_cmp_t *cookie;

        assertf(header.algo >= 1 || header.algo <= 2,
            "unsupported compression algorithm: %d", header.algo);
        assertf(algos[header.algo-1].decompress_init, 
            "asset: compression level %d not initialized. Call asset_init_compression(%d) at initialization time", header.algo, header.algo);

        cookie = malloc(sizeof(cookie_cmp_t) + algos[header.algo-1].state_size);
        cookie->read = algos[header.algo-1].decompress_read;
        algos[header.algo-1].decompress_init(cookie->state, f);

        cookie->fp = f;
        cookie->pos = 0;
        cookie->seeked = false;
        if (sz) *sz = header.orig_size;
        return funopen(cookie, readfn_cmp, NULL, seekfn_cmp, closefn_cmp);
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
