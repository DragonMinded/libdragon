#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "binout.h"
#include "assetcomp.h"
#include "aplib_compress.h"
#include "shrinkler_compress.h"
#undef SWAP
#undef MIN_MATCH_SIZE
#undef MIN
#undef MAX
#include "../../src/asset.c"
#include "../../src/compress/aplib_dec.c"
#include "../../src/compress/shrinkler_dec.c"
#include "../../src/compress/lz4_dec.c"
#include "../../src/compress/ringbuf.c"
#undef MIN
#undef MAX
#undef LZ4_DECOMPRESS_INPLACE_MARGIN

#include "lz4_compress.h"

static uint8_t* slurp(const char *fn, int *size)
{
    FILE *f = fopen(fn, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    int sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = (uint8_t*)malloc(sz);
    fread(buf, 1, sz, f);
    fclose(f);
    if (size) *size = sz;
    return buf;
}

void asset_compress_mem_raw(int compression, const uint8_t *data, int sz, uint8_t **output, int *cmp_size, int *winsize, int *margin)
{
    switch (compression) {
    case 1: { // lz4hc
        // Default for LZ4HC is 8 KiB, which makes sense given the little
        // data cache of VR4300 to improve decompression speed.
        if (*winsize == 0) {
            *winsize = 8*1024;
            while (sz < *winsize && *winsize > 2*1024)
                *winsize /= 2;
        }

        // The actual max distance of the LZ4 format is 64KiB-1, make sure we
        // don't go over that.
        if (*winsize > 64*1024) *winsize = 64*1024;
        lz4_distance_max = *winsize;
        if (lz4_distance_max > 65535) lz4_distance_max = 65535;

        int cmp_max_size = LZ4_COMPRESSBOUND(sz);
        *output = malloc(cmp_max_size);

        // Compress the file. Use compression level LZ4HC_CLEVEL_MAX and
        // "favor decompression speed", as we prefer to leave a bit of
        // compression ratio on the table in exchange for faster decompression.
        LZ4_streamHC_t* state = LZ4_createStreamHC();
        LZ4_setCompressionLevel(state, LZ4HC_CLEVEL_MAX);
        LZ4_favorDecompressionSpeed(state, 1);
        *cmp_size = LZ4_compress_HC_continue(state, (char*)data, (char*)*output, sz, cmp_max_size);
        LZ4_freeStreamHC(state);
        assert(*cmp_size <= cmp_max_size);
        *margin = LZ4_DECOMPRESS_INPLACE_MARGIN(*cmp_size);
    }   break;
    case 2: { // aplib
        if (*winsize == 0) {
            *winsize = 256*1024;
            while (sz < *winsize && *winsize > 2*1024)
                *winsize /= 2;
        }
    
        apultra_stats stats;
        int max_cmp_size = apultra_get_max_compressed_size(sz);
        *output = calloc(1, max_cmp_size);  // note: apultra.c clears the buffer, not sure why
        *cmp_size = apultra_compress(data, *output, sz, max_cmp_size, 
            0,          // flags
            *winsize,    // window size
            0,          // dictionary size
            NULL,       // progress callback
            &stats);

        *margin = stats.safe_dist + *cmp_size - sz;
    }   break;
    case 3: { // shrinkler
        *winsize = 256*1024; // FIXME
        int inplace_margin;
        *output = shrinkler_compress(data, sz, 3, cmp_size, &inplace_margin);
        // Shrinkler seems to return negative margin values because we asked to
        // verify using 4 byte reads. Just clamp to zero.
        *margin = inplace_margin > 0 ? inplace_margin : 0;
    }   break;
    default:
        assert(0);
    }  
}

/**
 * @brief Compress or recompress a file in the libdragon asset format.
 * 
 * @param infn          Input file to (re-)compress
 * @param outfn         Output file
 * @param compression   Requested compression level (0 = none, 1 = lz4hc, 2 = lzh5)
 * @param winsize       If zero, the compressor will choose the best window size
 *                      for optimal compression ratio/dec-speed. If not zero, the specified
 *                      window size will be used for compression. This can be useful
 *                      to decrease the amount of RAM used by the decompressor.
 * @return true         File was compressed correctly
 * @return false        Error compressing the file
 */
bool asset_compress(const char *infn, const char *outfn, int compression, int winsize)
{
    int sz;
    void *data = slurp(infn, &sz);
    if (!data) {
        fprintf(stderr, "error loading input file: %s\n", infn);
        return false;
    }

    FILE *out = fopen(outfn, "wb");
    if (!out) {
        fprintf(stderr, "error opening output file: %s\n", outfn);
        return false;
    }

    int cmp_size = asset_compress_mem(data, sz, out, compression, winsize);
    free(data);
    fclose(out);
    if (cmp_size < 0) unlink(outfn);
    return (cmp_size >= 0);
}

int asset_compress_mem(void *data, int sz, FILE *out, int compression, int winsize)
{
    if (winsize && asset_winsize_to_flags(winsize) < 0) {
        fprintf(stderr, "unsupported window size: %d\n", winsize);
        fprintf(stderr, "supported window sizes: 2, 4, 8, 16, 32, 64, 128, 256\n");
        return -1;
    }

    // The caller specified a certain window size. We can still silently decrease it
    // if the file is smaller, as there is no functional difference and we can save
    // some RAM at decompression time.
    if (winsize) {
        while (sz < winsize && winsize > 2*1024)
            winsize /= 2;
    }

    // FIXME: use asset_compress_mem_raw() instead of duplicating the code here
    switch (compression) {
    case 0: { // none
        fwrite(data, 1, sz, out);
        return sz;
    }   break;
    case 3: { // shrinkler
        winsize = 256*1024; // FIXME
        int cmp_size; int inplace_margin;
        uint8_t *output = shrinkler_compress(data, sz, 3, &cmp_size, &inplace_margin);
        // Shrinkler seems to return negative margin values because we asked to
        // verify using 4 byte reads. Just clamp to zero.
        inplace_margin = inplace_margin > 0 ? inplace_margin : 0;

        fwrite("DCA3", 1, 4, out);
        w16(out, 3); // algo
        w16(out, asset_winsize_to_flags(winsize) | ASSET_FLAG_INPLACE); // flags
        w32(out, cmp_size); // cmp_size
        w32(out, sz); // dec_size
        w32(out, inplace_margin); // inplace margin
        fwrite(output, 1, cmp_size, out);
        free(output);
        return cmp_size + 20;
    }   break;
    case 2: { // aplib
        if (winsize == 0) {
            winsize = 256*1024;
            while (sz < winsize && winsize > 2*1024)
                winsize /= 2;
        }
    
        apultra_stats stats;
        int max_cmp_size = apultra_get_max_compressed_size(sz);
        void *output = calloc(1, max_cmp_size);  // note: apultra.c clears the buffer, not sure why
        int cmp_size = apultra_compress(data, output, sz, max_cmp_size, 
            0,          // flags
            winsize,    // window size
            0,          // dictionary size
            NULL,       // progress callback
            &stats);

        int inplace_margin = stats.safe_dist + cmp_size - sz;
        fwrite("DCA3", 1, 4, out);
        w16(out, 2); // algo
        w16(out, asset_winsize_to_flags(winsize) | ASSET_FLAG_INPLACE); // flags
        w32(out, cmp_size); // cmp_size
        w32(out, sz); // dec_size
        w32(out, inplace_margin); // inplace margin
        fwrite(output, 1, cmp_size, out);
        free(output);
        return cmp_size + 20;
    }   break;
    case 1: { // lz4hc
        // Default for LZ4HC is 8 KiB, which makes sense given the little
        // data cache of VR4300 to improve decompression speed.
        if (winsize == 0) {
            winsize = 8*1024;
            while (sz < winsize && winsize > 2*1024)
                winsize /= 2;
        }

        // The actual max distance of the LZ4 format is 64KiB-1, make sure we
        // don't go over that.
        if (winsize > 64*1024) winsize = 64*1024;
        lz4_distance_max = winsize;
        if (lz4_distance_max > 65535) lz4_distance_max = 65535;

        int cmp_max_size = LZ4_COMPRESSBOUND(sz);
        void *output = malloc(cmp_max_size);

        // Compress the file. Use compression level LZ4HC_CLEVEL_MAX and
        // "favor decompression speed", as we prefer to leave a bit of
        // compression ratio on the table in exchange for faster decompression.
        LZ4_streamHC_t* state = LZ4_createStreamHC();
        LZ4_setCompressionLevel(state, LZ4HC_CLEVEL_MAX);
        LZ4_favorDecompressionSpeed(state, 1);
        int cmp_size = LZ4_compress_HC_continue(state, (char*)data, output, sz, cmp_max_size);
        LZ4_freeStreamHC(state);
        assert(cmp_size <= cmp_max_size);

        fwrite("DCA3", 1, 4, out);
        w16(out, 1); // algo
        w16(out, asset_winsize_to_flags(winsize) | ASSET_FLAG_INPLACE); // flags
        w32(out, cmp_size); // cmp_size
        w32(out, sz); // dec_size
        w32(out, LZ4_DECOMPRESS_INPLACE_MARGIN(cmp_size)); // inplace margin
        fwrite(output, 1, cmp_size, out);
        free(output);
        return cmp_size + 20;
    }   break;
    default:
        assert(0);
        return -1;
    }
}
