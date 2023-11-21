#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "../common/binout.h"
#include "../common/aplib_compress.c"
#undef SWAP
#undef MIN_MATCH_SIZE
#undef MIN
#undef MAX
#include "../../src/asset.c"
#include "../../src/compress/aplib_dec.c"
#include "../../src/compress/lz4_dec.c"
#include "../../src/compress/ringbuf.c"
#undef MIN
#undef MAX
#undef LZ4_DECOMPRESS_INPLACE_MARGIN

static int lz4_distance_max = 16384;

#ifndef LZ4_SRC_INCLUDED
#define LZ4_DISTANCE_MAX lz4_distance_max
#include "../common/lz4.c"
#endif
#include "../common/lz4hc.c"
#undef MIN
#undef MAX

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
    asset_init_compression(2);

    // Make sure the file exists before calling asset_load,
    // which would just assert.
    int insize = 0;
    FILE *in = fopen(infn, "rb");
    if (!in) {
        fprintf(stderr, "error opening input file: %s\n", infn);
        return false;
    }
    fseek(in, 0, SEEK_END);
    insize = ftell(in);
    fclose(in);

    if (winsize && asset_winsize_to_flags(winsize) < 0) {
        fprintf(stderr, "unsupported window size: %d\n", winsize);
        fprintf(stderr, "supported window sizes: 2, 4, 8, 16, 32, 64, 128, 256\n");
        return false;
    }

    // The caller specified a certain window size. We can still silently decrease it
    // if the file is smaller, as there is no functional difference and we can save
    // some RAM at decompression time.
    if (winsize) {
        while (insize < winsize && winsize > 2*1024)
            winsize /= 2;
    }

    // Load the file. This will transparently decompresses it if it was compressed,
    // so that this function can be used to also recompress an already compressed
    // file.
    int sz;
    uint8_t *data = asset_load(infn, &sz);

    switch (compression) {
    case 0: { // none
        FILE *out = fopen(outfn, "wb");
        if (!out) {
            fprintf(stderr, "error opening output file: %s\n", outfn);
            return 1;
        }
        fwrite(data, 1, sz, out);
        fclose(out);
    }   break;
    case 2: { // aplib
        if (winsize == 0) {
            winsize = 256*1024;
            while (insize < winsize && winsize > 2*1024)
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

        FILE *out = fopen(outfn, "wb");
        fwrite("DCA2", 1, 4, out);
        w16(out, 3); // algo
        w16(out, asset_winsize_to_flags(winsize)); // flags
        w32(out, cmp_size); // cmp_size
        w32(out, sz); // dec_size
        fwrite(output, 1, cmp_size, out);
        fclose(out);
        free(output);
    }   break;
    case 1: { // lz4hc
        // Default for LZ4HC is 8 KiB, which makes sense given the little
        // data cache of VR4300 to improve decompression speed.
        if (winsize == 0) {
            winsize = 8*1024;
            while (insize < winsize && winsize > 2*1024)
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

        FILE *out = fopen(outfn, "wb");
        fwrite("DCA2", 1, 4, out);
        w16(out, 1); // algo
        w16(out, asset_winsize_to_flags(winsize)); // flags
        w32(out, cmp_size); // cmp_size
        w32(out, sz); // dec_size
        fwrite(output, 1, cmp_size, out);
        fclose(out);
        free(output);
    }   break;
    default:
        assert(0);
    }

    return true;
}
