/*
 * At runtime, ym64.c supports streaming directly from a YM5/YM6
 * "interleaved" file, that is a file where frames are laid out linearly in the
 * file. We also support streaming from LHA-compressed files (using algorithm
 * -lh5-, which is the standard ones for YM files).
 *
 * The goal of this pre-processing is:
 *
 *   * Convert from older YM versions (eg: YM3!, YM3b).
 *   * Convert non-interleaved to interleaved.
 *   * Re-compress with LHA -lh5-.
 *
 */

#ifndef assertf
#define assertf(x, ...) assert(x)
#endif
#define memalign(a, b) malloc(b)

#include "../../src/compress/lzh5_internal.h"  // LZH5 decompression
#include "../../src/compress/lzh5.c"
#include "../../src/compress/ringbuf.c"
#include "../common/lzh5_compress.h"           // LZH5 compression
#include "../common/lzh5_compress.c"
#include <stdalign.h>


bool flag_ym_compress = false;

typedef struct __attribute__((packed)) {
    uint8_t size;
    uint8_t checksum;
    char methodid[5];
    uint32_t csize;
    uint32_t dsize;
    uint32_t timestamp;
    uint8_t attr;
    uint8_t level;
    uint8_t filename_len;
} lhaheader;

_Static_assert(sizeof(lhaheader) == 22, "invalid lhaheader size");

typedef struct __attribute__((packed)) {
    uint32_t nvbl;
    uint32_t attrs;
    uint16_t ndigidrums;
    uint32_t extfreq;
    uint16_t playfreq;
    uint32_t loop;
    uint16_t sizeext;
} ym5header;

_Static_assert(sizeof(ym5header) == 22, "invalid ym5header size");

static FILE *ym_f;
static bool ym_compressed;
static uint8_t alignas(8) ym_decoder[DECOMPRESS_LZH5_STATE_SIZE];

static void ymread(void *buf, int sz) {
    if (ym_compressed) {
        decompress_lzh5_read(ym_decoder, buf, sz);
        return;
    }
    fread(buf, 1, sz, ym_f);
}

static void ymwrite(const void *buf, int sz) {
    fwrite(buf, 1, sz, ym_f);
}

// Compress a file with LHA (algorithm -lh5-). This is done using
// a stripped down version of https://github.com/jca02266/lha
// stored as single file in lzh5_compress.c. The library works only
// through FILE*, so the compression will happen from disk to disk.
static void lha_compress(const char *outfn, const char *infn) {
    
    FILE *in = fopen(infn, "rb");
    if (!in) fatal("cannot open file: %s\n", infn);

    FILE *out = fopen(outfn, "wb");
    if (!out) fatal("cannot create file: %s\n", infn);

    // Prepare a basic LHA header. Leave most fields empty,
    // we will fill them later. The YM module itself is a file
    // within the archive whose name doesn't matter, so we
    // will use a fixed name of "audioconv64.bin".
    lhaheader head; uint16_t crc16 = 0;
    const char *lha_fn = "audioconv64.bin";
    memset(&head, 0, sizeof(head));

    head.size = (sizeof(head)-2) + strlen(lha_fn) + 2;
    memcpy(head.methodid, "-lh5-", 5);
    head.filename_len = strlen(lha_fn);

    // Write (incomplete) header, filename and (to be calculated) crc16.
    fwrite(&head, 1, sizeof(head), out);
    fwrite(lha_fn, 1, strlen(lha_fn), out);
    fwrite(&crc16, 1, 2, out);

    // Do the actual compression.
    unsigned int crc, dsize, csize;
    lzh5_init(LZHUFF5_METHOD_NUM);
    lzh5_encode(in, out, &crc, &csize, &dsize);

    // Complete the header with the information we got.
    head.csize = HOST_TO_LE32(csize);
    head.dsize = HOST_TO_LE32(dsize);
    crc16 = HOST_TO_LE16(crc);

    // Now that the header is finished, calculate the header checksum.
    uint8_t csum = 0;
    uint8_t *h = (uint8_t*)&head;
    for (int i=0;i<sizeof(head)-2;i++)
        csum += h[i+2];
    for (int i=0;i<strlen(lha_fn);i++)
        csum += (uint8_t)lha_fn[i];
    csum += crc16 & 0xFF;
    csum += crc16 >> 8;
    head.checksum = csum;

    // Write again the header and the crc16 of the file.
    fseek(out, 0, SEEK_SET);
    fwrite(&head, 1, sizeof(head), out);

    fseek(out, head.size-2+2, SEEK_SET);
    fwrite(&crc16, 1, 2, out);

    fclose(in); fclose(out);
}

int ym_convert(const char *infn, const char *outfn) {
    ym_f = fopen(infn, "rb");
    if (!ym_f) fatal("cannot open: %s\n", infn);

    // Get the file size
    fseek(ym_f, 0, SEEK_END);
    unsigned int fsize = ftell(ym_f);
    fseek(ym_f, 0, SEEK_SET);

    // Read the header. "ymread" is our helper function that can either
    // read a raw file or decompress LHA on-the-fly, depending on ym_compressed.
    // Start assuming the file is not compressed.
    char head[16]; head[12] = '\0';
    ym_compressed = false;
    ymread(head, 12);

    // Look for the LHA header. If found, this is a compressed file and we
    // need to decompress it while reading it.
    if (head[2] == '-' && head[3] == 'l' && head[6] == '-') {
        if (head[4] != 'h' || head[5] != '5') fatal("unsupported LHA algorithm: -l%c%c-", head[4], head[5]);

        // Get the decompressed file size from the LHA header.
        fseek(ym_f, 11, SEEK_SET);
        fread(&fsize, 4, 1, ym_f);
        fsize = LE32_TO_HOST(fsize);

        // Initialize LHA decompression, and read back the now uncompressed header.
        // Decompression is performed via a minimal version of
        // https://github.com/fragglet/lhasa, stored in lzh5.h.
        fseek(ym_f, head[0]+2, SEEK_SET);
        ym_compressed = true;
        decompress_lzh5_init(ym_decoder, ym_f, DECOMPRESS_LZH5_DEFAULT_WINDOW_SIZE);
        ymread(head, 12);
    }

    // If this is a YM3! or YM3b file, we need to convert it.
    // YM3 is an interleaved format. We need to convert it to non-interleaved
    // otherwise it cannot be streamed and thus would require lots of RAM.
    if (strncmp(head, "YM3!", 4) == 0 || strncmp(head, "YM3b", 4) == 0) {
        int csize = fsize-4; uint32_t loop;
        if (head[3] == 'b') csize -= 4; // loop frame

        // A valid YM3! file contains data for 14 registers, so the actual size
        // must be a multiple of 14 bytes.
        if (csize%14 != 0) fatal("YM3 has an invalid content size: %d\n", fsize);
        int nframes = csize / 14;

        // Read the whole file. If YM3b, read also the looping frame number.
        uint8_t *data = malloc(csize);
        ymread(data, csize);
        if (head[3] == 'b') ymread(&loop, 4);
        fclose(ym_f);

        // De-interleave the data. Notice that YM3! stores data for 14
        // registers, while YM5! has room for 16 registers (to handle digidrums).
        uint8_t *outdata = malloc(csize/14*16);
        memset(outdata, 0, csize/14*16);
        for (int i=0;i<csize;i++) {
            int r = i / nframes;
            int f = i % nframes;
            outdata[f*16+r] = data[i];
        }

        // Write a YM5 format (uncompressed) into temporary file.
        const char *tmpfilename = ".song.tmp";
        ym_f = fopen(tmpfilename, "wb");
        if (!ym_f) fatal("cannot create: %s", tmpfilename);

        ymwrite("YM5!LeOnArD!", 12);

        ym5header head;
        memset(&head, 0, sizeof(head));
        head.nvbl = HOST_TO_BE32(nframes);
        head.extfreq = HOST_TO_BE32(1000000);
        head.playfreq = HOST_TO_BE16(50);
        head.loop = loop;
        ymwrite(&head, sizeof(head));

        const char *song_name = "";
        const char *song_author = "";
        const char *song_comment = "";
        ymwrite(song_name, strlen(song_name)+1);
        ymwrite(song_author, strlen(song_author)+1);
        ymwrite(song_comment, strlen(song_comment)+1);

        ymwrite(outdata, csize/14*16);
        ymwrite("End!", 4);
        fclose(ym_f);

        // Do LHA compression to convert the temporary file into the final file.
        lha_compress(outfn, tmpfilename);

        free(data); free(outdata);
        remove(tmpfilename);

    // If this is a YM5! or YM6! file, we might need to convert it if it's not
    // interleaved, and compress it if it's not compressed.
    } else if (strncmp(head, "YM5!", 4) == 0 || strncmp(head, "YM6!", 4) == 0) {
        char buf[9];
        memcpy(buf, head+4, 8); buf[8] = 0;
        if (memcmp(buf, "LeOnArD!", 8) != 0) fatal("invalid header signature: %s", buf);

        // Read the header and check if the file is already interleaved.
        ym5header ymhead;
        ymread(&ymhead, sizeof(ymhead));
        if (ymhead.ndigidrums) fatal("digidrums not supported");

        // Read metadata (zero-terminated strings, so read one byte at a time).
        char song_name[512]; char song_author[512]; char song_comment[512]; int i;
        i = 0; do ymread(&song_name[i], 1); while (song_name[i++] != 0);
        i = 0; do ymread(&song_author[i], 1); while (song_author[i++] != 0);
        i = 0; do ymread(&song_comment[i], 1); while (song_comment[i++] != 0);

        // Read file contents
        int numframes = BE32_TO_HOST(ymhead.nvbl);
        int datasize = numframes*16;
        uint8_t *data = malloc(datasize);
        ymread(data, datasize);

        // Check terminator
        ymread(buf, 4);
        if (memcmp(buf, "End!", 4) != 0) fatal("missing terminator in YM5 file: %s", infn);
        fclose(ym_f); ym_f = NULL;

        // De-interleave (if required)
        uint8_t *outdata = malloc(datasize);
        if (BE32_TO_HOST(ymhead.attrs) & 1) {
            for (int i=0;i<datasize;i++) {
                int f = i % numframes;
                int r = i / numframes;
                outdata[f*16+r] = data[i];
            }           
        } else {
            memcpy(outdata, data, datasize);
        }

        // Turn off interleaving bit in header attributes
        ymhead.attrs = HOST_TO_BE32((BE32_TO_HOST(ymhead.attrs) & ~1));

        // Write back the YM5 file into a temporary file.
        const char *tmpfilename = ".song.tmp";
        ym_f = fopen(flag_ym_compress ? tmpfilename : outfn, "wb");
        if (!ym_f) fatal("cannot create: %s", flag_ym_compress ? tmpfilename : outfn);
        ymwrite("YM5!LeOnArD!", 12);
        ymwrite(&ymhead, sizeof(ymhead));
        ymwrite(song_name, strlen(song_name)+1);
        ymwrite(song_author, strlen(song_author)+1);
        ymwrite(song_comment, strlen(song_comment)+1);
        ymwrite(outdata, datasize);
        ymwrite("End!", 4);
        fclose(ym_f);

        // Do LHA compression
        if (flag_ym_compress) {
            lha_compress(outfn, tmpfilename);
            remove(tmpfilename);
        }

        free(data); free(outdata);
    } else {
        fatal("unsupported YM format: %c%c%c%c", head[0], head[1], head[2], head[3]);
    }

    return 0;
}

