#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "../common/binout.h"
#include "../common/lzh5_compress.h"
#include "../common/lzh5_compress.c"
#include "../../src/asset.c"

bool asset_compress(const char *infn, const char *outfn, int compression)
{
    // Make sure the file exists before calling asset_load,
    // which would just assert.
    FILE *in = fopen(infn, "rb");
    if (!in) {
        fprintf(stderr, "error opening input file: %s\n", infn);
        return false;
    }
    fclose(in);

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
    case 1: { // lzh5
        char *tmpfn = NULL;
        asprintf(&tmpfn, "%s.tmp", outfn);
        FILE *out = fopen(tmpfn, "wb");
        if (!out) {
            fprintf(stderr, "error opening output file: %s\n", tmpfn);
            return 1;
        }
        fwrite(data, 1, sz, out);
        fclose(out);

        in = fopen(tmpfn, "rb");
        out = fopen(outfn, "wb");
        fwrite("DCA1", 1, 4, out);
        w16(out, 1); // algo
        w16(out, 0); // flags
        int w_cmp_size = w32_placeholder(out); // cmp_size
        int w_dec_size = w32_placeholder(out); // dec_size

        unsigned int crc, dsize, csize;
        lzh5_init(LZHUFF5_METHOD_NUM);
        lzh5_encode(in, out, &crc, &csize, &dsize);

        w32_at(out, w_cmp_size, csize);
        w32_at(out, w_dec_size, dsize);

        fclose(in);
        fclose(out);
        remove(tmpfn);
        free(tmpfn);
    }   break;
    }

    return true;
}
