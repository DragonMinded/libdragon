/*
    n64symdump: dump the full contents of a symbol table for an N64 ROM
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <assert.h>
#include "n64sym_huffman.h"
#include "../common/polyfill.h"
#include "../../src/compress/shrinkler_dec_internal.h"

#define STBDS_NO_SHORT_NAMES
#define STB_DS_IMPLEMENTATION
#include "../common/stb_ds.h"

#define MAX_BUFFER_SIZE 512

// Known/supported format versions. These must be kept in sync with the tools
// that generate the data: SYMT_VERSION in n64sym.c, and the rompak TOC magic
// ("TOC0", where the trailing digit is the format version) in n64tool.c.
#define SYMT_KNOWN_VERSION   5
#define TOC_KNOWN_MAGIC      "TOC0"

typedef struct {
    char head[4];
    uint32_t version;
    uint32_t num_symbols;
    uint32_t num_chunks;
    uint32_t chunk_idx_off;
    uint32_t file_tab_off;
    uint32_t func_tab_off;
    uint32_t huff_tab_off;
    uint32_t file_blob_off;
    uint32_t func_blob_off;
    uint32_t stream_off;
    uint32_t max_chunk_margin;
    uint32_t num_files;
    uint32_t num_funcs;
    uint32_t num_file_blocks;
    uint32_t num_func_blocks;
    uint32_t huff_tab_size;
    uint32_t file_blob_size;
    uint32_t func_blob_size;
    uint32_t stream_size;
} symt_header_t;

typedef struct {
    uint32_t start_addr;
    uint32_t stream_off;
} chunk_index_entry_t;

typedef struct {
    uint32_t start_idx;
    uint32_t blob_off;
} string_index_entry_t;

static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void read_exact(FILE *f, void *buf, size_t len, const char *what) {
    if (fread(buf, 1, len, f) != len) {
        fprintf(stderr, "Error: cannot read %s\n", what);
        exit(1);
    }
}

// Base offset of the SYMT file within the input file. This is 0 when parsing a
// standalone .sym file, or the ROM offset of the .sym file when parsing a ROM.
static long g_symt_base = 0;

// Seek to an offset relative to the beginning of the SYMT file (all offsets in
// the SYMT header are relative to the SYMT start, not to the input file start).
static void seek_symt(FILE *f, long off, const char *what) {
    if (fseek(f, g_symt_base + off, SEEK_SET) != 0) {
        fprintf(stderr, "Error: cannot seek to %s (offset %ld)\n", what, g_symt_base + off);
        exit(1);
    }
}

static symt_header_t read_header(FILE *f) {
    enum { SYMT_HEADER_BYTES = 4 + 4 * 19 };
    uint8_t raw[SYMT_HEADER_BYTES];
    seek_symt(f, 0, "SYMT header");
    read_exact(f, raw, sizeof(raw), "header");
    symt_header_t h = {0};
    memcpy(h.head, raw + 0, 4);
    h.version       = be32(raw + 4);
    h.num_symbols   = be32(raw + 8);
    h.num_chunks    = be32(raw + 12);
    h.chunk_idx_off = be32(raw + 16);
    h.file_tab_off  = be32(raw + 20);
    h.func_tab_off  = be32(raw + 24);
    h.huff_tab_off  = be32(raw + 28);
    h.file_blob_off = be32(raw + 32);
    h.func_blob_off = be32(raw + 36);
    h.stream_off    = be32(raw + 40);
    h.max_chunk_margin = be32(raw + 44);
    h.num_files     = be32(raw + 48);
    h.num_funcs     = be32(raw + 52);
    h.num_file_blocks = be32(raw + 56);
    h.num_func_blocks = be32(raw + 60);
    h.huff_tab_size = be32(raw + 64);
    h.file_blob_size= be32(raw + 68);
    h.func_blob_size= be32(raw + 72);
    h.stream_size   = be32(raw + 76);
    return h;
}

static void print_header(symt_header_t *h) {
    printf("=== SYMT Header ===\n");
    printf("magic        : %.4s\n", h->head);
    printf("version      : %u\n", h->version);
    printf("num_symbols  : %u\n", h->num_symbols);
    printf("num_chunks   : %u\n", h->num_chunks);
    printf("chunk_idx_off: %u\n", h->chunk_idx_off);
    printf("file_tab_off : %u\n", h->file_tab_off);
    printf("func_tab_off : %u\n", h->func_tab_off);
    printf("huff_tab_off : %u\n", h->huff_tab_off);
    printf("file_blob_off: %u (size %u)\n", h->file_blob_off, h->file_blob_size);
    printf("func_blob_off: %u (size %u)\n", h->func_blob_off, h->func_blob_size);
    printf("stream_off   : %u (size %u)\n", h->stream_off, h->stream_size);
    printf("max_chunk_margin: %u\n", h->max_chunk_margin);
    printf("num_files    : %u (%u blocks)\n", h->num_files, h->num_file_blocks);
    printf("num_funcs    : %u (%u blocks)\n", h->num_funcs, h->num_func_blocks);
    printf("huff_tab_size: %u bytes\n", h->huff_tab_size);
    printf("\n");
}

static chunk_index_entry_t *read_chunk_index(FILE *f, symt_header_t *h) {
    chunk_index_entry_t *idx = NULL;
    if (h->num_chunks == 0) return idx;
    seek_symt(f, h->chunk_idx_off, "chunk index");
    for (uint32_t i = 0; i < h->num_chunks; i++) {
        uint8_t raw[8];
        read_exact(f, raw, 8, "chunk index");
        chunk_index_entry_t e = {
            .start_addr = be32(raw + 0),
            .stream_off = be32(raw + 4),
        };
        stbds_arrput(idx, e);
    }
    return idx;
}

static string_index_entry_t *read_string_index(FILE *f, uint32_t off, uint32_t count, const char *what) {
    string_index_entry_t *idx = NULL;
    if (count == 0) return idx;
    seek_symt(f, off, "string index");
    for (uint32_t i = 0; i < count; i++) {
        uint8_t raw[8];
        read_exact(f, raw, 8, what);
        string_index_entry_t e = {
            .start_idx = be32(raw + 0),
            .blob_off  = be32(raw + 4),
        };
        stbds_arrput(idx, e);
    }
    return idx;
}

static void print_chunk_index(chunk_index_entry_t *idx) {
    printf("=== Chunk Index (%d entries) ===\n", (int)stbds_arrlen(idx));
    for (int i = 0; i < stbds_arrlen(idx); i++) {
        printf("Chunk %3d: start_addr=0x%08x, stream_off=%u\n",
            i, idx[i].start_addr, idx[i].stream_off);
    }
    printf("\n");
}

static void print_string_index(const char *title, string_index_entry_t *idx, uint32_t blob_size) {
    printf("=== %s Index (%d blocks) ===\n", title, (int)stbds_arrlen(idx));
    for (int i = 0; i < stbds_arrlen(idx); i++) {
        uint32_t start = idx[i].blob_off;
        uint32_t end = (i + 1 < stbds_arrlen(idx)) ? idx[i + 1].blob_off : blob_size;
        printf("Block %3d: start_idx=%u, blob_off=%u, size=%u\n",
            i, idx[i].start_idx, idx[i].blob_off, end - start);
    }
    printf("\n");
}

static void print_huff_table(huff_decoder_t *dec) {
    printf("=== Huffman Table ===\n");
    printf("max_len: %d\n", dec->max_len);
    printf("LUT (len > 0):\n");
    for (int i = 0; i < 64; i++) {
        if (dec->lut[i].len)
            printf("  idx=%2d code_len=%d symbol=%u (0x%02x)\n", i, dec->lut[i].len, dec->lut[i].symbol, dec->lut[i].symbol);
    }
    if (dec->len_count > 0) {
        printf("FirstCode / FirstSymbol (len >=7):\n");
        for (int i = 0; i < dec->len_count; i++) {
            int len = 7 + i;
            printf("  len=%2d first_code=%u first_symbol=%u\n",
                len, dec->first_code[len], dec->first_symbol[len]);
        }
        printf("  terminal symbols_len=%d\n", dec->symbols_len);
        printf("Symbols (len>6):");
        for (int i = 0; i < dec->symbols_len; i++) {
            if (i % 16 == 0) printf("\n  ");
            printf("%02x ", dec->symbols[i]);
        }
        if (dec->symbols_len == 0) printf(" none");
        printf("\n");
    }
    printf("\n");
}

static uint32_t read_varint(const uint8_t **p, const uint8_t *end) {
    const uint8_t *ptr = *p;
    uint32_t val = 0;
    int shift = 0;
    while (ptr < end) {
        uint8_t b = *ptr++;
        val |= (uint32_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    *p = ptr;
    return val;
}

static int32_t read_signed_varint(const uint8_t **p, const uint8_t *end) {
    uint32_t val = read_varint(p, end);
    return (val >> 1) ^ -(val & 1);
}

static void decode_string_block(const uint8_t *data, size_t len, int count, huff_decoder_t *dec, char ***out) {
    BitReader br;
    br_init(&br, data, len);
    char cur[MAX_BUFFER_SIZE];
    cur[0] = 0;
    int cur_len = 0;
    int prev_prefix = 0;
    for (int i = 0; i < count; i++) {
        int32_t prefix_delta = read_exp_golomb_signed(&br);
        int prefix_len = prev_prefix + prefix_delta;
        if (prefix_len < 0) prefix_len = 0;
        if (prefix_len > cur_len) prefix_len = cur_len;
        if (prefix_len >= (int)sizeof(cur) - 1) prefix_len = sizeof(cur) - 2;
        cur_len = prefix_len;
        int sym;
        while ((sym = huff_decode_symbol(dec, &br)) > 0) {
            if (cur_len < (int)sizeof(cur) - 1) {
                cur[cur_len++] = (char)sym;
            }
        }
        cur[cur_len] = 0;
        stbds_arrput(*out, strdup(cur));
        prev_prefix = prefix_len;
    }
}

static char **decode_strings(FILE *f, int count, string_index_entry_t *idx, uint32_t blob_off, uint32_t blob_size, huff_decoder_t *dec, const char *title) {
    char **out = NULL;
    printf("=== %s Strings ===\n", title);
    for (int i = 0; i < stbds_arrlen(idx); i++) {
        uint32_t start = idx[i].blob_off;
        uint32_t end = (i + 1 < stbds_arrlen(idx)) ? idx[i + 1].blob_off : blob_size;
        uint32_t sz = end - start;
        uint8_t *buf = malloc(sz);
        seek_symt(f, blob_off + start, "string block");
        read_exact(f, buf, sz, "string block");

        int num_strings;
        if (i + 1 < stbds_arrlen(idx))
            num_strings = idx[i + 1].start_idx - idx[i].start_idx;
        else
            num_strings = count - idx[i].start_idx;
        decode_string_block(buf, sz, num_strings, dec, &out);
        free(buf);
    }
    for (int i = 0; i < stbds_arrlen(out); i++) {
        printf("  [%4d] %s\n", i, out[i]);
    }
    printf("\n");
    return out;
}

static const char *safe_lookup(char **arr, int idx) {
    if (idx < 0 || idx >= stbds_arrlen(arr)) return "???";
    return arr[idx];
}

static int decompress_chunk(const uint8_t *cmp_buf, uint32_t cmp_size, uint8_t *out_buf, uint32_t out_cap)
{
    FILE *tmp = tmpfile();
    assert(tmp && "tmpfile() failed");
    fwrite(cmp_buf, 1, cmp_size, tmp);
    fflush(tmp);
    fseek(tmp, 0, SEEK_SET);

    int winsize = 2 * 1024; // Chunks are <= 512 bytes plain, so compressor uses 2 KiB window
    uint8_t *state = calloc(1, DECOMPRESS_SHRINKLER_STATE_SIZE + winsize);
    decompress_shrinkler_init(state, fileno(tmp), winsize);

    int dec_size = 0;
    while (dec_size < out_cap) {
        ssize_t n = decompress_shrinkler_read(state, out_buf + dec_size, out_cap - dec_size);
        if (n < 0) {
            dec_size = -1;
            break;
        }
        if (n == 0) break;
        dec_size += n;
    }

    if (dec_size >= 0) {
        uint8_t tmp_byte;
        ssize_t n = decompress_shrinkler_read(state, &tmp_byte, 1);
        assert(n <= 0 && "Corrupted .sym file: compressed chunk expands beyond expected size");
    }

    free(state);
    fclose(tmp);
    return dec_size;
}

static void dump_symbols(FILE *f, symt_header_t *h, chunk_index_entry_t *chunks, char **files, char **funcs) {
    printf("=== Symbols ===\n");
    uint32_t total = 0;
    for (int c = 0; c < stbds_arrlen(chunks); c++) {
        uint32_t chunk_start = chunks[c].stream_off;
        uint32_t chunk_end = (c + 1 < stbds_arrlen(chunks)) ? chunks[c + 1].stream_off : h->stream_size;
        uint32_t chunk_len = chunk_end - chunk_start;
        uint8_t *cmp_buf = malloc(chunk_len);
        uint8_t *buf = malloc(MAX_BUFFER_SIZE);

        seek_symt(f, h->stream_off + chunk_start, "symbol chunk");
        read_exact(f, cmp_buf, chunk_len, "symbol chunk");
        int dec_size = decompress_chunk(cmp_buf, chunk_len, buf, MAX_BUFFER_SIZE);
        if (dec_size <= 0) {
            fprintf(stderr, "Error: decompression failed for chunk %d\n", c);
            exit(1);
        }

        const uint8_t *ptr = buf;
        const uint8_t *end = buf + dec_size;
        uint32_t chunk_func_off = read_varint(&ptr, end);

        uint32_t cur_addr = chunks[c].start_addr;
        int cur_file[2] = {0, 0}, cur_func[2] = {0, 0}, cur_line[2] = {0, 0};
        uint32_t last_func_addr = chunk_func_off ? (chunks[c].start_addr - chunk_func_off) : 0;

        while (ptr < end) {
            uint8_t op = *ptr++;
            int delta_file = (op & 0x80) ? read_signed_varint(&ptr, end) : 0;
            int delta_func = (op & 0x40) ? read_signed_varint(&ptr, end) : 0;
            int delta_line = (op & 0x20) ? read_signed_varint(&ptr, end) : 0;

            uint32_t delta_addr = 0;
            if ((op & 0x07) == 7) {
                delta_addr = (read_varint(&ptr, end) + 7) * 4;
            } else {
                delta_addr = (op & 0x07) * 4;
            }

            bool is_func = op & 0x10;
            bool is_inline = op & 0x08;
            int sid = is_inline ? 1 : 0;
            cur_file[sid] += delta_file;
            cur_func[sid] += delta_func;
            cur_line[sid] += delta_line;
            uint32_t sym_addr = cur_addr + delta_addr;
            if (is_func) last_func_addr = sym_addr;

            const char *file_str = safe_lookup(files, cur_file[sid]);
            const char *func_str = safe_lookup(funcs, cur_func[sid]);
            uint32_t func_off = last_func_addr ? sym_addr - last_func_addr : 0;

            printf("  [%6u] addr=0x%08x file=%d:%s func=%d:%s line=%d func_off=0x%x%s%s\n",
                total, sym_addr, cur_file[sid], file_str, cur_func[sid], func_str, cur_line[sid], func_off,
                is_func ? " [FUNC]" : "", is_inline ? " [INLINE]" : "");

            total++;
            cur_addr = sym_addr;
        }

        free(cmp_buf);
        free(buf);
    }
    printf("Total symbols decoded: %u (header says %u)\n\n", total, h->num_symbols);
}

// Scan a ROM file (.z64) for the rompak TOC, locate the symbol table file
// (.sym) inside it, and return its offset within the ROM file. Aborts with an
// error message if the TOC or the .sym file cannot be found, or if the TOC is
// in an unknown format.
//
// The layout mirrors rompak.c (runtime) and n64tool.c (build tool):
//  - The TOC is searched at 16-byte aligned addresses starting at ROM offset
//    0x1000 (PI address 0x10001000), which is right after IPL3.
//  - TOC entry offsets are relative to the start of ROM (PI 0x10000000), which
//    corresponds to byte offset 0 in the ROM file. So the entry offset is
//    directly the file offset of the .sym file.
//
// The rompak format has evolved over time while keeping the same "TOC0" magic,
// so the format version is not encoded in the magic and must be detected from
// the header/entry layout. Three variants are known and all are supported here
// (we only need the entry offset and name to locate the .sym file):
//
//   Header (16 bytes, big-endian):
//     Legacy:  magic[4], toc_size(u32),  entry_size(u32), num_entries(u32)
//     Current: magic[4], cookie(u32), toc_size(u32), entry_size(u16), num_entries(u16)
//
//   Entry (entry_size bytes, big-endian):
//     v1:      offset(u32), name[]
//     v2/curr: offset(u32), size(u32), name[]
static long rompak_find_sym(FILE *f) {
    // Only big-endian ROMs (.z64 byte order) are supported. Byte-swapped
    // variants (.v64 = halfword-swapped, .n64 = word-swapped) are rejected, as
    // the whole ROM (including the rompak TOC and the SYMT file) would need to
    // be unswapped first. A big-endian N64 ROM always starts with 0x80371240.
    uint8_t magic0[4];
    fseek(f, 0, SEEK_SET);
    read_exact(f, magic0, sizeof(magic0), "ROM header");
    if (be32(magic0) != 0x80371240) {
        fprintf(stderr, "Error: unsupported ROM byte order (header 0x%08x, expected 0x80371240). "
            "Only big-endian ROMs are supported; please convert byte-swapped (.v64/.n64) ROMs first.\n",
            be32(magic0));
        exit(1);
    }

    long toc_off = -1;
    for (int i = 0; i < 1024; i++) {
        long pos = 0x1000 + (long)i * 16;
        uint8_t magic[4];
        if (fseek(f, pos, SEEK_SET) != 0) break;
        if (fread(magic, 1, 4, f) != 4) break;
        // Match the 3-byte "TOC" prefix so that we can detect (and report)
        // TOCs written with a different, unknown magic version.
        if (magic[0] == 'T' && magic[1] == 'O' && magic[2] == 'C') {
            if (memcmp(magic, TOC_KNOWN_MAGIC, 4) != 0) {
                fprintf(stderr, "Error: unsupported rompak TOC format (magic '%.4s', expected '%s'). "
                    "This ROM was built with an incompatible version of the libdragon tools.\n",
                    (char*)magic, TOC_KNOWN_MAGIC);
                exit(1);
            }
            toc_off = pos;
            break;
        }
    }
    if (toc_off < 0) {
        fprintf(stderr, "Error: no rompak TOC found in ROM (is this a libdragon ROM?)\n");
        exit(1);
    }

    uint8_t hdr[16];
    fseek(f, toc_off, SEEK_SET);
    read_exact(f, hdr, sizeof(hdr), "rompak TOC header");

    // The header is 16 bytes in all known formats, and entries always start
    // right after it. Detect whether a "cookie" field is present by trying the
    // current layout first and falling back to the legacy layout. A field pair
    // is considered valid when entry_size and num_entries are within sane
    // bounds (same limits enforced at runtime by rompak.c).
    #define TOC_FIELDS_VALID(es, ne) ((es) >= 8 && (es) < 1024 && (ne) > 0 && (ne) < 1024)

    // Current layout: magic, cookie, toc_size, entry_size(u16), num_entries(u16)
    uint32_t cur_entry_size = ((uint32_t)hdr[12] << 8) | hdr[13];
    uint32_t cur_num_entries = ((uint32_t)hdr[14] << 8) | hdr[15];
    // Legacy layout: magic, toc_size, entry_size(u32), num_entries(u32)
    uint32_t leg_entry_size = be32(hdr + 8);
    uint32_t leg_num_entries = be32(hdr + 12);

    uint32_t entry_size, num_entries;
    if (TOC_FIELDS_VALID(cur_entry_size, cur_num_entries)) {
        entry_size = cur_entry_size;
        num_entries = cur_num_entries;
    } else if (TOC_FIELDS_VALID(leg_entry_size, leg_num_entries)) {
        entry_size = leg_entry_size;
        num_entries = leg_num_entries;
    } else {
        fprintf(stderr, "Error: unrecognized rompak TOC layout (this ROM was built with an "
            "incompatible version of the libdragon tools)\n");
        exit(1);
    }

    long sym_off = -1;
    uint8_t *entry = malloc(entry_size + 1);
    entry[entry_size] = 0; // ensure the name is always null-terminated
    for (uint32_t i = 0; i < num_entries; i++) {
        fseek(f, toc_off + sizeof(hdr) + (long)i * entry_size, SEEK_SET);
        read_exact(f, entry, entry_size, "rompak TOC entry");
        uint32_t offset = be32(entry + 0);

        // The name follows the offset field, optionally preceded by a size
        // field (added in later format versions). Detect the size field by
        // checking whether the byte right after the offset looks like the
        // start of a filename: a size field is a big-endian u32 whose most
        // significant byte is (near) zero for any realistic file, hence not a
        // printable filename character.
        int name_col = isprint(entry[4]) ? 4 : 8;
        const char *name = (const char *)(entry + name_col);
        size_t name_len = strnlen(name, entry_size - name_col);
        if (name_len >= 4 && strcasecmp(name + name_len - 4, ".sym") == 0) {
            printf("Found symbol table '%s' in rompak at ROM offset 0x%x\n\n", name, offset);
            sym_off = offset;
            break;
        }
    }
    free(entry);

    if (sym_off < 0) {
        fprintf(stderr, "Error: no symbol table (.sym) found in the ROM rompak\n");
        exit(1);
    }
    return sym_off;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <file.sym | rom.z64/.n64/.v64>\n", prog);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        usage(argv[0]);
        return 1;
    }

    const char *path = argv[1];
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    // If the input is a ROM file, locate the symbol table inside it by scanning
    // the rompak TOC. Otherwise, treat the whole file as a .sym file. Common N64
    // ROM extensions are accepted (.z64/.n64/.v64), but only big-endian ROMs are
    // actually supported (see the byte-order check in rompak_find_sym).
    const char *ext = strrchr(path, '.');
    if (ext && (strcasecmp(ext, ".z64") == 0 ||
                strcasecmp(ext, ".n64") == 0 ||
                strcasecmp(ext, ".v64") == 0)) {
        g_symt_base = rompak_find_sym(f);
    }

    symt_header_t h = read_header(f);
    if (memcmp(h.head, "SYMT", 4) != 0) {
        fprintf(stderr, "Error: invalid symbol table magic (expected 'SYMT', got '%.4s')\n", h.head);
        return 1;
    }
    if (h.version != SYMT_KNOWN_VERSION) {
        fprintf(stderr, "Error: unsupported symbol table version %u (expected %u). "
            "This symbol table was built with an incompatible version of n64sym.\n",
            h.version, SYMT_KNOWN_VERSION);
        return 1;
    }

    print_header(&h);
    chunk_index_entry_t *chunks = read_chunk_index(f, &h);
    string_index_entry_t *file_idx = read_string_index(f, h.file_tab_off, h.num_file_blocks, "file index");
    string_index_entry_t *func_idx = read_string_index(f, h.func_tab_off, h.num_func_blocks, "func index");

    print_chunk_index(chunks);
    print_string_index("File", file_idx, h.file_blob_size);
    print_string_index("Func", func_idx, h.func_blob_size);

    // Huffman table
    uint8_t *huff_buf = malloc(h.huff_tab_size);
    seek_symt(f, h.huff_tab_off, "huffman table");
    read_exact(f, huff_buf, h.huff_tab_size, "huffman table");
    huff_decoder_t dec;
    huff_decoder_init(&dec, huff_buf, h.huff_tab_size);
    print_huff_table(&dec);

    // Decode string tables
    char **file_strings = decode_strings(f, h.num_files, file_idx, h.file_blob_off, h.file_blob_size, &dec, "File");
    char **func_strings = decode_strings(f, h.num_funcs, func_idx, h.func_blob_off, h.func_blob_size, &dec, "Func");

    // Decode symbols
    dump_symbols(f, &h, chunks, file_strings, func_strings);

    // Cleanup
    for (int i = 0; i < stbds_arrlen(file_strings); i++) free(file_strings[i]);
    for (int i = 0; i < stbds_arrlen(func_strings); i++) free(func_strings[i]);
    stbds_arrfree(file_strings);
    stbds_arrfree(func_strings);
    stbds_arrfree(chunks);
    stbds_arrfree(file_idx);
    stbds_arrfree(func_idx);
    free(huff_buf);
    fclose(f);
    return 0;
}
