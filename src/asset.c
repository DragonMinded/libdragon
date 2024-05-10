#include "asset.h"
#include "asset_internal.h"
#include "compress/aplib_dec_internal.h"
#include "compress/lz4_dec_internal.h"
#include "compress/shrinkler_dec_internal.h"
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
static asset_compression_t algos[3] = {
    {
        .state_size = DECOMPRESS_LZ4_STATE_SIZE,
        .decompress_init = decompress_lz4_init,
        .decompress_read = decompress_lz4_read,
        .decompress_reset = decompress_lz4_reset,
        .decompress_full_inplace = decompress_lz4_full_inplace,
    }
};

void __asset_init_compression_lvl2(void)
{
    algos[1] = (asset_compression_t){
        .state_size = DECOMPRESS_APLIB_STATE_SIZE,
        .decompress_init = decompress_aplib_init,
        .decompress_read = decompress_aplib_read,
        .decompress_reset = decompress_aplib_reset,
        #if DECOMPRESS_APLIB_FULL_USE_ASM
        .decompress_full_inplace = decompress_aplib_full_inplace,
        #else
        .decompress_full = decompress_aplib_full,
        #endif
    };
}

void __asset_init_compression_lvl3(void)
{
    algos[2] = (asset_compression_t){
        #if DECOMPRESS_SHRINKLER_FULL_USE_ASM
        .decompress_full_inplace = decompress_shrinkler_full_inplace,
        #else
        .decompress_full = decompress_shrinkler_full,
        #endif
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

static void* decompress_inplace(asset_compression_t *algo, const char *fn, FILE *fp, size_t cmp_size, size_t size, int margin)
{
    // Consistency check on input data
    assert(margin >= 0);

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
    if (bufsize & 15) {
        // In case we need to call invalidate (see below), we need an aligned buffer
        bufsize += 16 - (bufsize & 15);
    }

    void *s = memalign(ASSET_ALIGNMENT, bufsize);
    assertf(s, "asset_load: out of memory");
    int n;

    #ifdef N64
    if (fn && strncmp(fn, "rom:/", 5) == 0) {
        // Invalid the portion of the buffer where we are going to load
        // the compressed data. This is needed in case the buffer returned
        // by memalign happens to be in cached already.
        int align_cmp_offset = cmp_offset & ~15;
        data_cache_hit_invalidate(s+align_cmp_offset, bufsize-align_cmp_offset);

        // Loading from ROM. This is a common enough situation that we want to optimize it.
        // Start an asynchronous DMA transfer, so that we can start decompressing as the
        // data flows in.
        uint32_t addr = dfs_rom_addr(fn+5) & 0x1FFFFFFF;
        dma_read_async(s+cmp_offset, addr+sizeof(asset_header_t), cmp_size);

        // Run the decompression racing with the DMA.
        n = algo->decompress_full_inplace(s+cmp_offset, cmp_size, s, size); (void)n;
    #else
    if (false) {
    #endif
    } else {
        // Standard loading via stdio. We have to wait for the whole file to be read.
        fread(s+cmp_offset, 1, cmp_size, fp);

        // Run the decompression.
        n = algo->decompress_full_inplace(s+cmp_offset, cmp_size, s, size); (void)n;
    }
    assertf(n == size, "asset: decompression error on file %s: corrupted? (%d/%d)", fn, n, size);
    void *ptr = realloc(s, size); (void)ptr;
    assertf(s == ptr, "asset: realloc moved the buffer"); // guaranteed by newlib
    return ptr;
}

void *asset_load(const char *fn, int *sz)
{
    uint8_t *s; int size;
    FILE *f = must_fopen(fn);

    // Disable buffering. This is optimal both for the no-compression case
    // (where we just read the whole file in one go, so we want to avoid
    // extra memory copies), and for the compressed case (where each
    // compression algorithm implements its own logic of optimized buffering).
    setvbuf(f, NULL, _IONBF, 0);
   
    // Check if file is compressed
    asset_header_t header;
    fread(&header, 1, sizeof(asset_header_t), f);
    if (!memcmp(header.magic, ASSET_MAGIC, 3)) {
        if (header.version != '3') {
            assertf(0, "unsupported asset version: %c\nMake sure to rebuild libdragon tools and your assets", header.version);
            return NULL;
        }

        #ifndef N64
        header.algo = __builtin_bswap16(header.algo);
        header.flags = __builtin_bswap16(header.flags);
        header.cmp_size = __builtin_bswap32(header.cmp_size);
        header.orig_size = __builtin_bswap32(header.orig_size);
        header.inplace_margin = __builtin_bswap32(header.inplace_margin);
        #endif

        assertf(header.algo >= 1 || header.algo <= 3,
            "unsupported compression algorithm: %d", header.algo);
        assertf(algos[header.algo-1].decompress_full || algos[header.algo-1].decompress_full_inplace, 
            "asset: compression level %d not initialized. Call asset_init_compression(%d) at initialization time", header.algo, header.algo);

        size = header.orig_size;
        if ((header.flags & ASSET_FLAG_INPLACE) && algos[header.algo-1].decompress_full_inplace)
            s = decompress_inplace(&algos[header.algo-1], fn, f, header.cmp_size, size, header.inplace_margin);
        else
            s = algos[header.algo-1].decompress_full(fn, f, header.cmp_size, size);
    } else {
        // Allocate a buffer big enough to hold the file.
        // We force a 32-byte alignment for the buffer so that it's aligned to instruction cache lines.
        // This might or might not be useful, but if a binary file is laid out so that it
        // matters, at least we guarantee that. 
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        s = memalign(ASSET_ALIGNMENT, size);

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
    if (whence == SEEK_SET && pos == 0) {
        cookie->seeked = false;
        rewind(cookie->fp);
        return 0;
    }

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
    void (*reset)(void *state);
    ssize_t (*read)(void *state, void *buf, size_t len);
    uint8_t alignas(8) state[];
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
    if (whence == SEEK_SET && pos == 0 && cookie->reset) {
        cookie->seeked = false;
        cookie->pos = 0;
        fseek(cookie->fp, sizeof(asset_header_t), SEEK_SET);
        cookie->reset(cookie->state);
        return 0;
    }

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
        if (header.version != '3') {
            assertf(0, "unsupported asset version: %c\nMake sure to rebuild libdragon tools and your assets", header.version);
            return NULL;
        }

        if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) {  // for mkasset running on PC
            header.algo = __builtin_bswap16(header.algo);
            header.flags = __builtin_bswap16(header.flags);
            header.cmp_size = __builtin_bswap32(header.cmp_size);
            header.orig_size = __builtin_bswap32(header.orig_size);
            header.inplace_margin = __builtin_bswap32(header.inplace_margin);
        }

        cookie_cmp_t *cookie;

        assertf(header.algo >= 1 || header.algo <= 2,
            "unsupported compression algorithm: %d", header.algo);
        assertf(algos[header.algo-1].decompress_full || algos[header.algo-1].decompress_full_inplace, 
            "asset: compression level %d not initialized. Call asset_init_compression(%d) at initialization time", header.algo, header.algo);
        assertf(algos[header.algo-1].decompress_init, 
            "asset: compression level %d does not currently support asset_fopen()", header.algo);

        int winsize = asset_winsize_from_flags(header.flags);
        cookie = malloc(sizeof(cookie_cmp_t) + algos[header.algo-1].state_size + winsize);
        cookie->read = algos[header.algo-1].decompress_read;
        cookie->reset = algos[header.algo-1].decompress_reset;
        algos[header.algo-1].decompress_init(cookie->state, f, winsize);

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
