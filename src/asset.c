#include "asset.h"
#include "asset_internal.h"
#include "compress/aplib_dec_internal.h"
#include "compress/lz4_dec_internal.h"
#include "compress/shrinkler_dec_internal.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdalign.h>
#include <sys/stat.h>

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

int must_open(const char *fn)
{
    int fd = open(fn, O_RDONLY);
    if (fd < 0) {
        // File not found.
        int errnum = errno;
        if (errnum == EINVAL) {
            if (!strstr(fn, ":/")) {
                // A common mistake is to forget the filesystem prefix.
                // Try to give a hint if that's the case.
                assertf(fd >= 0, "File not found: %s\n"
                    "Did you forget the filesystem prefix? (e.g. \"rom:/\")", fn);
                return -1;
            } else if (strstr(fn, "rom:/")) {
                // Another common mistake is to forget to initialize the rom filesystem.
                // Suggest that if the filesystem prefix is "rom:/".
                assertf(fd >= 0, "File not found: %s\n"
                    "Did you forget to call dfs_init(), or did it return an error?", fn);
                return -1;
            }
        }
        assertf(fd >= 0, "error opening file %s: %s\n", fn, strerror(errnum));
    }
    return fd;
}

FILE *must_fopen(const char *fn)
{
    return fdopen(must_open(fn), "rb");
}

static bool decompress_inplace(asset_compression_t *algo, int fd, size_t cmp_size, size_t size, int margin, void *buf, int *buf_size)
{
    // Consistency check on input data
    assert(margin >= 0);
    int cmp_offset;
    int bufsize = asset_buf_size(size, cmp_size, margin, &cmp_offset);
    if(buf == NULL || *buf_size < bufsize) {
        *buf_size = bufsize;
        return false;
    } else {
        #ifdef N64
        assertf(((uintptr_t)(buf) & (ASSET_ALIGNMENT_MIN-1)) == 0, "Asset buffer incorrectly aligned.");
        #endif
    }
    void *s = buf;
    int n;

    #ifdef N64
    uint32_t rom_addr = 0;
    if (ioctl(fd, IODFS_GET_ROM_BASE, &rom_addr) >= 0) {
        // Invalid the portion of the buffer where we are going to load
        // the compressed data. This is needed in case the buffer returned
        // by memalign happens to be in cached already.
        int align_cmp_offset = cmp_offset & ~15;
        data_cache_hit_invalidate(s+align_cmp_offset, bufsize-align_cmp_offset);

        // Loading from ROM. This is a common enough situation that we want to optimize it.
        // Start an asynchronous DMA transfer, so that we can start decompressing as the
        // data flows in.
        uint32_t addr = rom_addr+lseek(fd, 0, SEEK_CUR);
        dma_read_async(s+cmp_offset, addr, cmp_size);

        // Run the decompression racing with the DMA.
        n = algo->decompress_full_inplace(s+cmp_offset, cmp_size, s, size); (void)n;
    #else
    if (false) {
    #endif
    } else {
        // Standard loading via stdio. We have to wait for the whole file to be read.
        read(fd, s+cmp_offset, cmp_size);

        // Run the decompression.
        n = algo->decompress_full_inplace(s+cmp_offset, cmp_size, s, size); (void)n;
    }
    assertf(n == size, "asset: decompression error: corrupted? (%d/%d)", n, size);
    return true;
}

static int asset_read_header(int fd, asset_header_t *header, int *sz)
{
    read(fd, header, sizeof(asset_header_t));
    
    if (!memcmp(header->magic, ASSET_MAGIC, 3)) {
        if (header->version != '3') {
            assertf(0, "unsupported asset version: %c\nMake sure to rebuild libdragon tools and your assets", header->version);
        }

        #ifndef N64
        header->algo = __builtin_bswap16(header->algo);
        header->flags = __builtin_bswap16(header->flags);
        header->cmp_size = __builtin_bswap32(header->cmp_size);
        header->orig_size = __builtin_bswap32(header->orig_size);
        header->inplace_margin = __builtin_bswap32(header->inplace_margin);
        #endif
        
        int compressed_size = header->cmp_size+sizeof(asset_header_t);
        assertf(compressed_size == *sz, "Wrong compressed size (%d/%d)", *sz, compressed_size);

        assertf(header->algo >= 1 || header->algo <= 3,
            "unsupported compression algorithm: %d", header->algo);
        assertf(algos[header->algo-1].decompress_full || algos[header->algo-1].decompress_full_inplace, 
            "asset: compression level %d not initialized. Call asset_init_compression(%d) at initialization time", header->algo, header->algo);
        return asset_buf_size(header->orig_size, header->cmp_size, header->inplace_margin, NULL);
    } else {
        assertf(*sz >= 0, "Invalid uncompressed size");
        return *sz;
    }
}

static bool asset_read(int fd, asset_header_t *header, int *sz, void *buf, int *buf_size)
{
    if(!memcmp(header->magic, ASSET_MAGIC, 3)) {
        bool ret;

        if ((header->flags & ASSET_FLAG_INPLACE) && algos[header->algo-1].decompress_full_inplace)
            ret = decompress_inplace(&algos[header->algo-1], fd, header->cmp_size, header->orig_size, header->inplace_margin, buf, buf_size);
        else
            ret = algos[header->algo-1].decompress_full(fd, header->cmp_size, header->orig_size, buf, buf_size);
        if(ret) {
            *sz = header->orig_size;
        }
        return ret;
    } else {
        if(buf == NULL || *buf_size < *sz) {
            *buf_size = *sz;
            return false;
        } else {
            #ifdef N64
            assertf(((uintptr_t)(buf) & (ASSET_ALIGNMENT_MIN-1)) == 0, "Asset buffer incorrectly aligned.");
            #endif
        }
        lseek(fd, -((off_t)sizeof(asset_header_t)), SEEK_CUR);
        read(fd, buf, *sz);
        return true;
    }
}

bool asset_loadf_into(FILE *f, int *sz, void *buf, int *buf_size)
{
    int fd;
    
    fd = fileno(f);
    fflush(f);
    assertf(ftell(f) == lseek(fd, 0, SEEK_CUR), "Flushing has data remaining in buffer");
    asset_header_t header;
    asset_read_header(fd, &header, sz);
    return asset_read(fd, &header, sz, buf, buf_size);
}

void *asset_loadf(FILE *f, int *sz)
{
    void *buf = NULL; int buf_size = 0;
    int fd;
    fd = fileno(f);
    fflush(f);
    assertf(ftell(f) == lseek(fd, 0, SEEK_CUR), "Flushing has data remaining in buffer");
    asset_header_t header;
    buf_size = asset_read_header(fd, &header, sz);
    buf = memalign(ASSET_ALIGNMENT, buf_size);
    asset_read(fd, &header, sz, buf, &buf_size);
    return buf;
}

void *asset_load(const char *fn, int *sz)
{
    void *buf = NULL; int buf_size = 0;
    int size;
    int fd = must_open(fn);
    struct stat stat;
    fstat(fd, &stat);
    size = stat.st_size;
    asset_header_t header;
    buf_size = asset_read_header(fd, &header, &size);
    buf = memalign(ASSET_ALIGNMENT, buf_size);
    asset_read(fd, &header, &size, buf, &buf_size);
    if (sz) *sz = size;
    close(fd);
    return buf;
}

#ifdef N64

typedef struct  {
    int fd;
    bool seeked;
} cookie_none_t;

static fpos_t seekfn_none(void *c, fpos_t pos, int whence)
{
    cookie_none_t *cookie = c;

    // SEEK_CUR with pos=0 is used as ftell()
    if (whence == SEEK_CUR && pos == 0)
        return lseek(cookie->fd, 0, SEEK_CUR);
    if (whence == SEEK_SET && pos == 0) {
        cookie->seeked = false;
        lseek(cookie->fd, 0, SEEK_SET);
        return 0;
    }

    cookie->seeked = true;
    return -1;
}

static int readfn_none(void *c, char *buf, int sz)
{
    cookie_none_t *cookie = c;
    assertf(!cookie->seeked, "Cannot seek in file opened via asset_fopen (it might be compressed)");
    return read(cookie->fd, buf, sz);
}

static int closefn_none(void *c)
{
    cookie_none_t *cookie = c;
    close(cookie->fd); cookie->fd = -1;
    free(cookie);
    return 0;
}

typedef struct  {
    int fd;
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
        lseek(cookie->fd, sizeof(asset_header_t), SEEK_SET);
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
    close(cookie->fd); cookie->fd = -1;
    free(cookie);
    return 0;
}

FILE *asset_fopen(const char *fn, int *sz)
{
    // Open the file. We use buffering on the outer file created by funopen,
    // so we don't actually need buffering on the underlying one.
    int fd = must_open(fn);

    // Check if file is compressed
    asset_header_t header;
    read(fd, &header, sizeof(asset_header_t));
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
        algos[header.algo-1].decompress_init(cookie->state, fd, winsize);

        cookie->fd = fd;
        cookie->pos = 0;
        cookie->seeked = false;
        if (sz) *sz = header.orig_size;
        return funopen(cookie, readfn_cmp, NULL, seekfn_cmp, closefn_cmp);
    }

    // Not compressed. Return a wrapped FILE* without the seeking capability,
    // so that it matches the behavior of the compressed file.
    if (sz) *sz = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    cookie_none_t *cookie = malloc(sizeof(cookie_none_t));
    cookie->fd = fd;
    cookie->seeked = false;
    return funopen(cookie, readfn_none, NULL, seekfn_none, closefn_none);
}

#endif /* N64 */
