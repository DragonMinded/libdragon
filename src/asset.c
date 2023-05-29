#include "asset.h"
#include "asset_internal.h"
#include "compress/lzh5_internal.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdalign.h>

#ifdef N64
#include <malloc.h>
#include "debug.h"
#include "n64sys.h"
#else
#include <stdlib.h>
#include <assert.h>
#define memalign(a, b) malloc(b)
#define assertf(x, ...) assert(x)
#endif

FILE *must_fopen(const char *fn)
{
    FILE *f = fopen(fn, "rb");
    if (!f) {
        // File not found. A common mistake it is to forget the filesystem
        // prefix. Try to give a hint if that's the case.
        int errnum = errno;
        if (errnum == EINVAL && !strstr(fn, ":/"))
            assertf(f, "File not found: %s\n"
                "Did you forget the filesystem prefix? (e.g. \"rom:/\")\n", fn);
        else
        	assertf(f, "error opening file %s: m%s\n", fn, strerror(errnum));
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
        case 1: {
            size = header.orig_size;
            s = memalign(16, size);
            int n = decompress_lz5h_full(f, s, size); (void)n;
            assertf(n == size, "DCA: decompression error on file %s: corrupted? (%d/%d)", fn, n, size);
        }   break;
        default:
            assertf(0, "DCA: unsupported compression algorithm: %d", header.algo);
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

FILE *asset_fopen(const char *fn)
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
        return funopen(cookie, readfn_lha, NULL, seekfn_lha, closefn_lha);
    }

    // Not compressed. Return a wrapped FILE* without the seeking capability,
    // so that it matches the behavior of the compressed file.
    fseek(f, 0, SEEK_SET);
    cookie_none_t *cookie = malloc(sizeof(cookie_none_t));
    cookie->fp = f;
    cookie->seeked = false;
    return funopen(cookie, readfn_none, NULL, seekfn_none, closefn_none);
}

#endif /* N64 */
