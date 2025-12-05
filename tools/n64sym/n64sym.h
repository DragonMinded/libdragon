/*
    n64sym: generate a symbol table for an N64 ROM
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
*/
#ifndef N64SYM_H
#define N64SYM_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/utils.h"
#define STBDS_NO_SHORT_NAMES
#include "../common/stb_ds.h"

#define HUFF_MAX_CODE_LEN 16

// Huffman types
typedef struct huff_node_s {
    int freq;
    int ch; // -1 for internal nodes, 0-255 for leaves
    struct huff_node_s *left, *right;
} huff_node_t;

typedef struct {
    uint32_t code;
    int len;
} huff_code_t;

typedef struct {
    uint8_t symbol;
    uint8_t len;
} Lut64Entry;

typedef struct {
    uint8_t max_len;
    Lut64Entry lut[64];
    uint16_t first_code[HUFF_MAX_CODE_LEN + 1];
    uint16_t first_symbol[HUFF_MAX_CODE_LEN + 1];
    uint16_t num_symbols[HUFF_MAX_CODE_LEN + 1];
    uint8_t *symbols; // dynamic
} CanonicalTables;

// BitWriter
typedef struct {
    uint8_t *buf;      // stbds array
    uint8_t cur_byte;
    int cur_bit;       // 0..7
} BitWriter;

void bw_init(BitWriter *bw);
void bw_write(BitWriter *bw, uint32_t code, int len);
void bw_write_byte(BitWriter *bw, uint8_t b);
void bw_flush(BitWriter *bw);
void bw_write_varint(BitWriter *bw, uint32_t val);
void bw_write_exp_golomb(BitWriter *bw, uint32_t val);
int exp_golomb_len(uint32_t val);
uint32_t zigzag_encode(int32_t val);

// Huffman
void collect_string_freqs(char **strings, int *char_freqs);
void build_limited_huffman_tree(int *freqs, int limit, huff_code_t *table);
void generate_canonical_tables(huff_code_t *huff_table, CanonicalTables *ct);
void write_huff_header(CanonicalTables *ct, uint8_t **blob);

#endif

