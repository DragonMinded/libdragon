#include "asset.h"
#include "asset_internal.h"
#include "compress/lzh5.h"

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

static FILE *must_fopen(const char *fn)
{
    FILE *f = fopen(fn, "rb");
    if (!f) {
        // File not found. A common mistake it is to forget the filesystem
        // prefix. Try to give a hint if that's the case.
        if (!strstr(fn, ":/"))
            assertf(f, "File not found: %s\n"
                "Did you forget the filesystem prefix? (e.g. \"rom:/\")\n", fn);
        else
            assertf(f, "File not found: %s\n", fn);
    }
    return f;
}

void *asset_load(const char *fn, int *sz)
{
    uint8_t *s; int size;
    FILE *f = must_fopen(fn);
   
    // Check if file is compressed
    char magic[4];
    fread(&magic, 1, 4, f);
    if(!memcmp(magic, ASSET_MAGIC, 4)) {
        asset_header_t header;
        fread(&header, 1, sizeof(asset_header_t), f);

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
            LHANewDecoder decoder;
            lha_lh_new_init(&decoder, f);
            int n = lha_lh_new_read(&decoder, s, size);
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

static fpos_t seekfn_none(void *cookie, fpos_t pos, int whence)
{
    FILE *f = (FILE*)cookie;
    switch (whence) {
    case SEEK_SET:
        assertf(pos >= ftell(f), 
            "Cannot seek backward in file opened via asset_fopen (it might be compressed) %ld %ld", pos, ftell(f));
        break;
    case SEEK_CUR:
        assertf(pos >= 0,
            "Cannot seek backward in file opened via asset_fopen (it might be compressed) %ld", pos);
        break;
    case SEEK_END:
        assertf(0, 
            "Cannot seek from end in file opened via asset_fopen (it might be compressed)");
        break;
    }
    fseek(f, pos, whence);
    return ftell(f);
}

static int closefn_none(void *cookie)
{
    FILE *f = (FILE*)cookie;
    fclose(f);
    return 0;
}

static int readfn_none(void *cookie, char *buf, int sz)
{
    FILE *f = (FILE*)cookie;
    return fread(buf, 1, sz, f);
}

static fpos_t seekfn_lha(void *cookie, fpos_t pos, int whence)
{
    // TODO: implement forward seeking. This is currently prevented by
    // the buffering happening at the FILE* level, which causes backward
    // seeks. Eg:
    //    read 1 byte => newlib calls readfn with 1024 bytes (buffer)
    //    seek 1 byte forward => newlib calls seekfn with -1022 bytes
    assertf(0, "Cannot seek in file opened via asset_fopen (it might be compressed)");
    return 0;
}

static int closefn_lha(void *cookie)
{
    LHANewDecoder *decoder = (LHANewDecoder*)cookie;
    FILE *f = decoder->bit_stream_reader.fp;
    fclose(f);
    free(decoder);
    return 0;
}

static int readfn_lha(void *cookie, char *buf, int sz)
{
    LHANewDecoder *decoder = (LHANewDecoder*)cookie;
    return lha_lh_new_read(decoder, (uint8_t*)buf, sz);
}

FILE *asset_fopen(const char *fn)
{
    FILE *f = must_fopen(fn);

    // Check if file is compressed
    char magic[4];
    fread(&magic, 1, 4, f);
    if(!memcmp(magic, ASSET_MAGIC, 4)) {
        asset_header_t header;
        fread(&header, 1, sizeof(asset_header_t), f);

        #ifndef N64
        header.algo = __builtin_bswap16(header.algo);
        header.flags = __builtin_bswap16(header.flags);
        header.cmp_size = __builtin_bswap32(header.cmp_size);
        header.orig_size = __builtin_bswap32(header.orig_size);
        #endif

        LHANewDecoder *decoder = malloc(sizeof(LHANewDecoder));
        lha_lh_new_init(decoder, f);
        return funopen(decoder, readfn_lha, NULL, seekfn_lha, closefn_lha);
    }

    // Not compressed. Return a wrapped FILE* without the seeking capability,
    // so that it matches the behavior of the compressed file.
    fseek(f, 0, SEEK_SET);
    return funopen(f, readfn_none, NULL, seekfn_none, closefn_none);
}

#endif /* N64 */
