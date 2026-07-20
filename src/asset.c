/**
 * @file asset.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#include "asset.h"
#include "asset_internal.h"
#include "debug.h"
#include "compress/aplib_dec_internal.h"
#include "compress/lz4_dec_internal.h"
#include "compress/shrinkler_dec_internal.h"
#include "utils.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdalign.h>
#include <sys/stat.h>

/// @cond
#ifndef O_BINARY
#define O_BINARY 0
#endif
/// @endcond

#ifdef N64
#include <malloc.h>
#include "debug.h"
#include "n64sys.h"
#include "dma.h"
#include "dragonfs.h"
#else
#include <stdlib.h>
#include <assert.h>
/// @cond
#define memalign(a, b) malloc(b)
/// @endcond
#endif

/** 
 * @brief Compression algorithms
 * 
 * Only level 1 (LZ4) is always initialized. The other algorithm (LZH5)
 * must be initialized manually via #asset_init_compression.
 */
__attribute__((used))
static asset_compression_full_t algos_full[3] = {
{
        #ifdef N64
        .decompress_full = decompress_lz4_full_inplace,
        #endif
    },
};

static asset_compression_stream_t algos_stream[3] = {
{
        .state_size = DECOMPRESS_LZ4_STATE_SIZE,
        .decompress_init = decompress_lz4_init,
        .decompress_read = decompress_lz4_read,
        .decompress_reset = decompress_lz4_reset,
    },
};

/** @brief Initialize compression level 2 (APLIB) */
void __asset_init_compression_lvl2(void)
{
    #ifdef N64
    algos_full[1] = (asset_compression_full_t){
        .decompress_full = decompress_aplib_full_inplace,
    };
    #endif
    algos_stream[1] = (asset_compression_stream_t){
        .state_size = DECOMPRESS_APLIB_STATE_SIZE,
        .decompress_init = decompress_aplib_init,
        .decompress_read = decompress_aplib_read,
        .decompress_reset = decompress_aplib_reset,
    };
}

/** @brief Initialize compression level 3 (SHRINKLER) */
void __asset_init_compression_lvl3(void)
{
    #ifdef N64
    algos_full[2] = (asset_compression_full_t){
        .decompress_full = decompress_shrinkler_full_inplace,
    };
    #endif
    algos_stream[2] = (asset_compression_stream_t){
        .state_size = DECOMPRESS_SHRINKLER_STATE_SIZE,
        .decompress_init = decompress_shrinkler_init,
        .decompress_read = decompress_shrinkler_read,
        .decompress_reset = decompress_shrinkler_reset,
    };
}

int must_open(const char *fn)
{
    int fd = open(fn, O_RDONLY|O_BINARY);
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

__attribute__((used))
static bool decompress_full_stream(asset_compression_stream_t *algo, int fd, uint16_t flags, size_t size, void *buf, int *buf_size)
{
    if (buf == NULL || *buf_size < (int)size) {
        *buf_size = (int)size;
        return false;
    }
    #ifdef N64
    assertf(((uintptr_t)(buf) & (ASSET_ALIGNMENT_MIN-1)) == 0, "Asset buffer incorrectly aligned.");
    #endif

    assertf(algo->decompress_init && algo->decompress_read,
        "asset: compression level does not support streaming decompression");

    int winsize = asset_winsize_from_flags(flags);
    uint8_t *state = malloc(algo->state_size + winsize);
    assertf(state, "Out of memory");
    algo->decompress_init(state, fd, winsize);
    int n = algo->decompress_read(state, buf, size);
    assertf(n == size, "asset: decompression error: corrupted? (%d/%d)", n, (int)size);
    free(state);
    return true;
}

__attribute__((used))
static bool decompress_full(asset_compression_full_t *algo, int fd, size_t cmp_size, size_t size, int margin, void *buf, int *buf_size)
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
        assertf(addr % 2 == 0, "asset_load requires ROM data to be 2-byte aligned");
        uint64_t ticket = dma_read_async(s+cmp_offset, addr, cmp_size);
        dma_wait_started(ticket);

        // Run the decompression racing with the DMA.
        n = algo->decompress_full(s+cmp_offset, cmp_size, s, size); (void)n;
    #else
    if (false) {
    #endif
    } else {
        // Standard loading via stdio. We have to wait for the whole file to be read.
        read(fd, s+cmp_offset, cmp_size);

        // Run the decompression.
        n = algo->decompress_full(s+cmp_offset, cmp_size, s, size); (void)n;
    }
    assertf(n == size, "asset: decompression error: corrupted? (%d/%d)", n, (int)size);
    return true;
}

static int asset_read_header(int fd, asset_parsed_header_t *header, int *sz)
{
    int rdhead = read(fd, &header->base, sizeof(asset_header_t));
    
    if (memcmp(header->base.magic, ASSET_MAGIC, 3) == 0) {
        if (header->base.version != '5') {
            assertf(0, "unsupported asset version: %c\nMake sure to rebuild libdragon tools and your assets", header->base.version);
        }

        const uint8_t *ptr = header->base.varints;
        header->cmp_size = __read_varint_u64(&ptr);
        header->orig_size = __read_varint_u64(&ptr);
        header->inplace_margin = __read_varint_u64(&ptr);
        int header_size = (void*)ptr - (void*)header;
        if (header_size & 1) header_size++;
        assertf(header_size < sizeof(asset_header_t), "header size too large");

        // Seek back to the actual end of the header
        int cur = lseek(fd, header_size - rdhead, SEEK_CUR); (void)cur;
        assertf((cur & 1) == 0, "asset_read_header: header not 2-byte aligned (pos=%d)", cur);
        
        int compressed_size = header->cmp_size + header_size;
        assertf(compressed_size == *sz, "Wrong compressed size (%d/%d)", *sz, compressed_size);

        int algo = ASSET_FLAG_ALGO(header->base.flags);
        assertf(algo >= 1 && algo <= 3,
            "unsupported compression algorithm: %d", algo);
        #ifdef N64
        assertf(algos_full[algo-1].decompress_full,
            "asset: compression level %d not initialized. Call asset_init_compression(%d) at initialization time", algo, algo);
        #else
        assertf(algos_stream[algo-1].decompress_init && algos_stream[algo-1].decompress_read,
            "asset: compression level %d not initialized. Call asset_init_compression(%d) at initialization time", algo, algo);
        #endif
        return asset_buf_size(header->orig_size, header->cmp_size, header->inplace_margin, NULL);
    } else {
        // Seek back before the header.
        lseek(fd, -rdhead, SEEK_CUR);
        assertf(*sz >= 0, "Invalid uncompressed size");
        return *sz;
    }
}

static bool asset_read(int fd, asset_parsed_header_t *header, int *sz, void *buf, int *buf_size)
{
    if(memcmp(header->base.magic, ASSET_MAGIC, 3) == 0) {
        bool ret;
        int algo = ASSET_FLAG_ALGO(header->base.flags);

        #ifdef N64
        ret = decompress_full(&algos_full[algo-1], fd, header->cmp_size, header->orig_size, header->inplace_margin, buf, buf_size);
        #else
        // On PC, full decompression is implemented by consuming the streaming decoder.
        ret = decompress_full_stream(&algos_stream[algo-1], fd, header->base.flags, header->orig_size, buf, buf_size);
        #endif
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
        read(fd, buf, *sz);
        return true;
    }
}

bool asset_loadfd_into(int fd, int *sz, void *buf, int *buf_size)
{
    asset_parsed_header_t header;
    asset_read_header(fd, &header, sz);
    return asset_read(fd, &header, sz, buf, buf_size);
}

bool asset_loadf_into(FILE *f, int *sz, void *buf, int *buf_size)
{
    int fd = fileno(f);
    fflush(f);
    assertf(ftell(f) == lseek(fd, 0, SEEK_CUR), "Flushing has data remaining in buffer");
    return asset_loadfd_into(fd, sz, buf, buf_size);
}

void *asset_loadfd(int fd, int *sz)
{
    void *buf = NULL; int buf_size = 0;
    asset_parsed_header_t header;
    buf_size = asset_read_header(fd, &header, sz);
    buf = memalign(ASSET_ALIGNMENT, buf_size);
    assertf(buf, "Out of memory");
    asset_read(fd, &header, sz, buf, &buf_size);
    return buf;
}

void *asset_loadf(FILE *f, int *sz)
{
    int fd = fileno(f);
    fflush(f);
    assertf(ftell(f) == lseek(fd, 0, SEEK_CUR), "Flushing has data remaining in buffer");
    return asset_loadfd(fd, sz);
}

void *asset_load(const char *fn, int *sz)
{
    void *buf = NULL; int buf_size = 0;
    int size;
    int fd = must_open(fn);
    struct stat stat;
    fstat(fd, &stat);
    size = stat.st_size;
    asset_parsed_header_t header;
    buf_size = asset_read_header(fd, &header, &size);
    buf = memalign(ASSET_ALIGNMENT, buf_size);
    assertf(buf, "Out of memory");
    asset_read(fd, &header, &size, buf, &buf_size);
    if (sz) *sz = size;
    close(fd);
    return buf;
}

#ifdef N64

/** @brief Uncompressed file cookie for funopen() */
typedef struct  {
    int fd;             ///< Open file descriptor
    bool seeked;        ///< True if the file has been seeked once
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

/** @brief Compression cookie for funopen() */
typedef struct  {
    int fd;                         ///< Open File descriptor
    int pos;                        ///< Current position in the file
    bool seeked;                    ///< True if the file has been seeked once
    int header_size;                ///< Size of the header
    void (*reset)(void *state);     ///< Reset function for the decompression state
    ssize_t (*read)(void *state, void *buf, size_t len); ///< Read function for the decompression state
    uint8_t alignas(16) state[];    ///< Decompression state (16-byte aligned)
} cookie_cmp_t;

_Static_assert(offsetof(cookie_cmp_t, state) % 16 == 0, "cookie_cmp_t.state must be 16-byte aligned");

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
        lseek(cookie->fd, cookie->header_size, SEEK_SET);
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
    return asset_fdopen(fd, sz);
}

FILE *asset_fdopen(int fd, int *sz)
{
    // Check if file is compressed
    asset_parsed_header_t header;
    int rdhead = read(fd, &header.base, sizeof(asset_header_t));
    if (memcmp(header.base.magic, ASSET_MAGIC, 3) == 0) {
        if (header.base.version != '5') {
            assertf(0, "unsupported asset version: %c\nMake sure to rebuild libdragon tools and your assets", header.base.version);
            return NULL;
        }

        const uint8_t *ptr = header.base.varints;
        header.cmp_size = __read_varint_u64(&ptr);
        header.orig_size = __read_varint_u64(&ptr);
        header.inplace_margin = __read_varint_u64(&ptr);
        int header_size = (void*)ptr - (void*)&header;
        if (header_size & 1) header_size++;
        
        // Seek back to the actual end of the header
        lseek(fd, header_size - rdhead, SEEK_CUR);

        cookie_cmp_t *cookie;

        int algo = ASSET_FLAG_ALGO(header.base.flags);
        assertf(algo >= 1 && algo <= 3,
            "unsupported compression algorithm: %d", algo);
        assertf(algos_stream[algo-1].decompress_init,
            "asset: compression level %d not initialized. Call asset_init_compression(%d) at initialization time", algo, algo);

        int winsize = asset_winsize_from_flags(header.base.flags);
        cookie = memalign(16, sizeof(cookie_cmp_t) + algos_stream[algo-1].state_size + winsize);
        assertf(cookie, "Out of memory");
        cookie->read = algos_stream[algo-1].decompress_read;
        cookie->reset = algos_stream[algo-1].decompress_reset;
        algos_stream[algo-1].decompress_init(cookie->state, fd, winsize);
        cookie->fd = fd;
        cookie->pos = 0;
        cookie->seeked = false;
        cookie->header_size = header_size;
        if (sz) *sz = header.orig_size;
        return funopen(cookie, readfn_cmp, NULL, seekfn_cmp, closefn_cmp);
    }

    // Not compressed. Return a wrapped FILE* without the seeking capability,
    // so that it matches the behavior of the compressed file.
    int pos = lseek(fd, 0, SEEK_CUR);
    if (sz) *sz = lseek(fd, 0, SEEK_END);
    lseek(fd, pos - sizeof(asset_header_t), SEEK_SET);
    cookie_none_t *cookie = malloc(sizeof(cookie_none_t));
    assertf(cookie, "Out of memory");
    cookie->fd = fd;
    cookie->seeked = false;
    return funopen(cookie, readfn_none, NULL, seekfn_none, closefn_none);
}

#endif /* N64 */
