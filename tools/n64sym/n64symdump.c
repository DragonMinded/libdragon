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

#define STBDS_NO_SHORT_NAMES
#define STB_DS_IMPLEMENTATION
#include "../common/stb_ds.h"

#define MAX_BUFFER_SIZE 512
#define HUFF_MAX_CODE_LEN 16

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
    uint32_t num_files;      // number of file blocks
    uint32_t num_funcs;      // number of func blocks
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

typedef struct {
    uint8_t symbol;
    uint8_t len;
} huff_lut_t;

typedef struct {
    huff_lut_t lut[64];
    uint16_t first_code[HUFF_MAX_CODE_LEN + 1];
    uint16_t first_symbol[HUFF_MAX_CODE_LEN + 2]; // +1 for terminal entry
    uint8_t *symbols;
    int max_len;
    int len_count;
    int symbols_len;
} huff_decoder_t;

typedef struct {
    const uint8_t *buf;
    size_t len;
    size_t byte_pos;
    uint32_t cache;
    int bits_in_cache;
    size_t bits_consumed;
} bit_reader_t;

static uint16_t be16(const uint8_t *p) {
    return (uint16_t)(p[0] << 8 | p[1]);
}

static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void read_exact(FILE *f, void *buf, size_t len, const char *what) {
    if (fread(buf, 1, len, f) != len) {
        fprintf(stderr, "Error: cannot read %s\n", what);
        exit(1);
    }
}

static symt_header_t read_header(FILE *f) {
    enum { SYMT_HEADER_BYTES = 4 + 4 * 16 };
    uint8_t raw[SYMT_HEADER_BYTES];
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
    h.num_files     = be32(raw + 44);
    h.num_funcs     = be32(raw + 48);
    h.huff_tab_size = be32(raw + 52);
    h.file_blob_size= be32(raw + 56);
    h.func_blob_size= be32(raw + 60);
    h.stream_size   = be32(raw + 64);
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
    printf("num_files    : %u blocks\n", h->num_files);
    printf("num_funcs    : %u blocks\n", h->num_funcs);
    printf("huff_tab_size: %u bytes\n", h->huff_tab_size);
    printf("\n");
}

static chunk_index_entry_t *read_chunk_index(FILE *f, symt_header_t *h) {
    chunk_index_entry_t *idx = NULL;
    if (h->num_chunks == 0) return idx;
    fseek(f, h->chunk_idx_off, SEEK_SET);
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
    fseek(f, off, SEEK_SET);
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

static void huff_decoder_init(huff_decoder_t *dec, uint8_t *buf, uint32_t size) {
    dec->max_len = buf[0];
    dec->len_count = dec->max_len >= 7 ? dec->max_len - 6 : 0;
    const uint8_t *ptr = buf + 2;
    for (int i = 0; i < 64; i++) {
        dec->lut[i].symbol = ptr[0];
        dec->lut[i].len = ptr[1];
        ptr += 2;
    }
    memset(dec->first_code, 0, sizeof(dec->first_code));
    memset(dec->first_symbol, 0, sizeof(dec->first_symbol));
    if (dec->len_count > 0) {
        for (int i = 0; i < dec->len_count; i++) {
            dec->first_code[7 + i] = be16(ptr); ptr += 2;
        }
        for (int i = 0; i < dec->len_count + 1; i++) {
            dec->first_symbol[7 + i] = be16(ptr); ptr += 2;
        }
        dec->symbols_len = dec->first_symbol[7 + dec->len_count];
    } else {
        dec->symbols_len = 0;
    }
    dec->symbols = (uint8_t*)ptr;
    (void)size;
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

static void br_fill(bit_reader_t *br) {
    while (br->bits_in_cache <= 24 && br->byte_pos < br->len) {
        br->cache |= (uint32_t)br->buf[br->byte_pos++] << (24 - br->bits_in_cache);
        br->bits_in_cache += 8;
    }
}

static void br_init(bit_reader_t *br, const uint8_t *buf, size_t len) {
    br->buf = buf;
    br->len = len;
    br->byte_pos = 0;
    br->cache = 0;
    br->bits_in_cache = 0;
    br->bits_consumed = 0;
    br_fill(br);
}

static int br_peek_bits(bit_reader_t *br, int n) {
    if (n == 0) return 0;
    if (br->bits_in_cache < n) br_fill(br);
    if (br->bits_in_cache < n) return -1;
    return (int)(br->cache >> (32 - n));
}

static int br_read_bits(bit_reader_t *br, int n) {
    int v = br_peek_bits(br, n);
    if (v < 0) return -1;
    br->cache <<= n;
    br->bits_in_cache -= n;
    br->bits_consumed += n;
    return v;
}

static void br_skip_bits(bit_reader_t *br, int n) {
    br_read_bits(br, n);
}

static int32_t read_exp_golomb_signed(bit_reader_t *br) {
    int zero_bits = 0;
    while (true) {
        int bit = br_peek_bits(br, 1);
        if (bit < 0) return 0;
        if (bit != 0) break;
        br_skip_bits(br, 1);
        zero_bits++;
    }
    uint32_t val = (uint32_t)br_read_bits(br, zero_bits + 1);
    if ((int)val < 0) return 0;
    val -= 1;
    return (val >> 1) ^ -(val & 1);
}

static int huff_decode_symbol(huff_decoder_t *dec, bit_reader_t *br) {
    int peek16 = br_peek_bits(br, 16);
    if (peek16 < 0) return -1;
    int peek6 = peek16 >> 10;
    int len = dec->lut[peek6].len;
    if (len > 0) {
        br_skip_bits(br, len);
        return dec->lut[peek6].symbol;
    }
    for (int idx = 0; idx < dec->len_count; idx++) {
        len = 7 + idx;
        int code = peek16 >> (16 - len);
        uint16_t fc = dec->first_code[len];
        uint16_t fs = dec->first_symbol[len];
        uint16_t count = dec->first_symbol[len + 1] - fs;
        if (code < fc + count) {
            br_skip_bits(br, len);
            int sym_idx = fs + (code - fc);
            if (sym_idx < dec->symbols_len)
                return dec->symbols[sym_idx];
            return -1;
        }
    }
    return -1;
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

static void decode_string_block(const uint8_t *data, size_t len, int count_hint, huff_decoder_t *dec, char ***out) {
    bit_reader_t br;
    br_init(&br, data, len);
    char cur[MAX_BUFFER_SIZE];
    cur[0] = 0;
    int cur_len = 0;
    int prev_prefix = 0;
    int produced = 0;
    size_t total_bits = len * 8;
    while ((count_hint < 0 && br.bits_consumed < total_bits) ||
           (count_hint >= 0 && produced < count_hint)) {
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
        produced++;
        prev_prefix = prefix_len;

        if (count_hint < 0 && br.bits_consumed >= total_bits)
            break;
    }
}

static char **decode_strings(FILE *f, string_index_entry_t *idx, uint32_t blob_off, uint32_t blob_size, huff_decoder_t *dec, const char *title) {
    char **out = NULL;
    printf("=== %s Strings ===\n", title);
    for (int i = 0; i < stbds_arrlen(idx); i++) {
        uint32_t start = idx[i].blob_off;
        uint32_t end = (i + 1 < stbds_arrlen(idx)) ? idx[i + 1].blob_off : blob_size;
        uint32_t sz = end - start;
        uint8_t *buf = malloc(sz);
        fseek(f, blob_off + start, SEEK_SET);
        read_exact(f, buf, sz, "string block");

        int count_hint = -1;
        if (i + 1 < stbds_arrlen(idx))
            count_hint = idx[i + 1].start_idx - idx[i].start_idx;

        decode_string_block(buf, sz, count_hint, dec, &out);
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

static void dump_symbols(FILE *f, symt_header_t *h, chunk_index_entry_t *chunks, char **files, char **funcs) {
    printf("=== Symbols ===\n");
    uint32_t total = 0;
    for (int c = 0; c < stbds_arrlen(chunks); c++) {
        uint32_t chunk_start = chunks[c].stream_off;
        uint32_t chunk_end = (c + 1 < stbds_arrlen(chunks)) ? chunks[c + 1].stream_off : h->stream_size;
        uint32_t chunk_len = chunk_end - chunk_start;
        uint8_t *buf = malloc(chunk_len);
        fseek(f, h->stream_off + chunk_start, SEEK_SET);
        read_exact(f, buf, chunk_len, "symbol chunk");

        const uint8_t *ptr = buf;
        const uint8_t *end = buf + chunk_len;
        uint32_t chunk_func_off = read_varint(&ptr, end);

        uint32_t cur_addr = chunks[c].start_addr;
        int cur_file = 0, cur_func = 0, cur_line = 0;
        uint32_t last_func_addr = chunk_func_off ? (chunks[c].start_addr - chunk_func_off) : 0;

        while (ptr < end) {
            uint8_t op = *ptr++;
            if (op == 0x00) break;
            int delta_file = (op & 0x80) ? read_signed_varint(&ptr, end) : 0;
            int delta_func = (op & 0x40) ? read_signed_varint(&ptr, end) : 0;
            int delta_line = (op & 0x20) ? read_signed_varint(&ptr, end) : 0;

            uint32_t delta_addr = 0;
            if ((op & 0x07) == 7) {
                delta_addr = (read_varint(&ptr, end) + 7) * 4;
            } else {
                delta_addr = (op & 0x07) * 4;
            }

            cur_file += delta_file;
            cur_func += delta_func;
            cur_line += delta_line;
            uint32_t sym_addr = cur_addr + delta_addr;
            bool is_func = op & 0x10;
            bool is_inline = op & 0x08;
            if (is_func) last_func_addr = sym_addr;

            const char *file_str = safe_lookup(files, cur_file);
            const char *func_str = safe_lookup(funcs, cur_func);
            uint32_t func_off = last_func_addr ? sym_addr - last_func_addr : 0;

            printf("  [%6u] addr=0x%08x file=%d:%s func=%d:%s line=%d func_off=0x%x%s%s\n",
                total, sym_addr, cur_file, file_str, cur_func, func_str, cur_line, func_off,
                is_func ? " [FUNC]" : "", is_inline ? " [INLINE]" : "");

            total++;
            cur_addr = sym_addr;
        }

        free(buf);
    }
    printf("Total symbols decoded: %u (header says %u)\n\n", total, h->num_symbols);
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <file.sym>\n", prog);
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

    symt_header_t h = read_header(f);
    if (memcmp(h.head, "SYMT", 4) != 0) {
        fprintf(stderr, "Error: invalid magic\n");
        return 1;
    }
    if (h.version != 3) {
        fprintf(stderr, "Error: unsupported version %u\n", h.version);
        return 1;
    }

    print_header(&h);
    chunk_index_entry_t *chunks = read_chunk_index(f, &h);
    string_index_entry_t *file_idx = read_string_index(f, h.file_tab_off, h.num_files, "file index");
    string_index_entry_t *func_idx = read_string_index(f, h.func_tab_off, h.num_funcs, "func index");

    print_chunk_index(chunks);
    print_string_index("File", file_idx, h.file_blob_size);
    print_string_index("Func", func_idx, h.func_blob_size);

    // Huffman table
    uint8_t *huff_buf = malloc(h.huff_tab_size);
    fseek(f, h.huff_tab_off, SEEK_SET);
    read_exact(f, huff_buf, h.huff_tab_size, "huffman table");
    huff_decoder_t dec;
    huff_decoder_init(&dec, huff_buf, h.huff_tab_size);
    print_huff_table(&dec);

    // Decode string tables
    char **file_strings = decode_strings(f, file_idx, h.file_blob_off, h.file_blob_size, &dec, "File");
    char **func_strings = decode_strings(f, func_idx, h.func_blob_off, h.func_blob_size, &dec, "Func");

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

