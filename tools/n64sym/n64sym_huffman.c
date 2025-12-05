/*
    n64sym: generate a symbol table for an N64 ROM
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
 */
 #include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "n64sym.h"
#include "../common/utils.h"

void bw_init(BitWriter *bw) {
    bw->buf = NULL;
    bw->cur_byte = 0;
    bw->cur_bit = 0;
}

void bw_write(BitWriter *bw, uint32_t code, int len) {
    for (int i = len - 1; i >= 0; i--) {
        int bit = (code >> i) & 1;
        if (bit) bw->cur_byte |= (1 << (7 - bw->cur_bit));
        bw->cur_bit++;
        if (bw->cur_bit == 8) {
            stbds_arrput(bw->buf, bw->cur_byte);
            bw->cur_byte = 0;
            bw->cur_bit = 0;
        }
    }
}

void bw_write_byte(BitWriter *bw, uint8_t b) {
    bw_write(bw, b, 8);
}

void bw_flush(BitWriter *bw) {
    if (bw->cur_bit > 0) {
        stbds_arrput(bw->buf, bw->cur_byte);
        bw->cur_byte = 0;
        bw->cur_bit = 0;
    }
}

void bw_write_varint(BitWriter *bw, uint32_t val) {
    do {
        uint8_t byte = val & 0x7F;
        val >>= 7;
        if (val) byte |= 0x80;
        bw_write_byte(bw, byte);
    } while (val);
}

void bw_write_exp_golomb(BitWriter *bw, uint32_t val) {
    val++; // Exp-Golomb is 1-based (0 -> 1, 1 -> 010, 2 -> 011)
    
    // Count bits
    int bits = 0;
    uint32_t tmp = val;
    while (tmp) {
        bits++;
        tmp >>= 1;
    }
    
    // Write (bits-1) zeros
    bw_write(bw, 0, bits - 1);
    // Write value
    bw_write(bw, val, bits);
}

int exp_golomb_len(uint32_t val) {
    val++;
    int bits = 0;
    while (val) {
        bits++;
        val >>= 1;
    }
    return (bits - 1) + bits;
}

uint32_t zigzag_encode(int32_t val) {
    return (val << 1) ^ (val >> 31);
}

int huff_cmp_nodes(const void *a, const void *b) {
    const huff_node_t *na = *(const huff_node_t **)a;
    const huff_node_t *nb = *(const huff_node_t **)b;
    return nb->freq - na->freq;
}

void huff_calc_lengths(huff_node_t *node, int len, huff_code_t *table) {
    if (!node) return;
    if (node->ch >= 0) {
        table[node->ch].len = len;
        return;
    }
    huff_calc_lengths(node->left, len + 1, table);
    huff_calc_lengths(node->right, len + 1, table);
}

huff_node_t* build_huffman_tree(int *freqs) {
    huff_node_t *nodes[256];
    int n_nodes = 0;
    
    for (int i=0; i<256; i++) {
        if (freqs[i] > 0) {
            huff_node_t *n = malloc(sizeof(huff_node_t));
            n->freq = freqs[i];
            n->ch = i;
            n->left = n->right = NULL;
            nodes[n_nodes++] = n;
        }
    }
    
    if (n_nodes == 0) return NULL;

    while (n_nodes > 1) {
        int min1 = -1, min2 = -1;
        for (int i=0; i<n_nodes; i++) {
            if (min1 == -1 || nodes[i]->freq < nodes[min1]->freq) {
                min2 = min1; min1 = i;
            } else if (min2 == -1 || nodes[i]->freq < nodes[min2]->freq) {
                min2 = i;
            }
        }
        
        huff_node_t *n1 = nodes[min1];
        huff_node_t *n2 = nodes[min2];
        
        huff_node_t *parent = malloc(sizeof(huff_node_t));
        parent->freq = n1->freq + n2->freq;
        parent->ch = -1;
        parent->left = n1;
        parent->right = n2;
        
        nodes[min1] = parent;
        nodes[min2] = nodes[n_nodes-1];
        n_nodes--;
    }
    
    return nodes[0];
}

void huff_free_tree(huff_node_t *node) {
    if (!node) return;
    huff_free_tree(node->left);
    huff_free_tree(node->right);
    free(node);
}

// Build a Huffman tree with a maximum code length constraint.
// Since we don't have a complex Package-Merge implementation, we use a heuristic:
// if the tree is too deep, we halve the frequencies (rounding up) and retry.
// This tends to flatten the tree.
void build_limited_huffman_tree(int *freqs, int limit, huff_code_t *table) {
    int cur_freqs[256];
    memcpy(cur_freqs, freqs, sizeof(cur_freqs));
    
    while (1) {
        huff_node_t *root = build_huffman_tree(cur_freqs);
        memset(table, 0, 256 * sizeof(huff_code_t));
        huff_calc_lengths(root, 0, table);
        huff_free_tree(root);
        
        int max_len = 0;
        for (int i=0; i<256; i++) {
            if (table[i].len > max_len) max_len = table[i].len;
        }
        
        if (max_len <= limit) break;
        
        // Flatten frequencies
        for (int i=0; i<256; i++) {
            if (cur_freqs[i] > 0)
                cur_freqs[i] = (cur_freqs[i] + 1) / 2;
        }
    }
}

void generate_canonical_tables(huff_code_t *huff_table, CanonicalTables *ct) {
    int bl_count[HUFF_MAX_CODE_LEN + 1] = {0};
    ct->max_len = 0;
    int alphabet_size = 0;
    
    for (int i = 0; i < 256; i++) {
        int len = huff_table[i].len;
        if (len > 0) {
            assert(len <= HUFF_MAX_CODE_LEN && "Huffman code too long!");
            bl_count[len]++;
            if (len > ct->max_len) ct->max_len = len;
            alphabet_size++;
        }
    }

    uint32_t next_code[HUFF_MAX_CODE_LEN + 1];
    uint32_t code = 0;
    bl_count[0] = 0;
    for (int bits = 1; bits <= HUFF_MAX_CODE_LEN; bits++) {
        code = (code + bl_count[bits-1]) << 1;
        next_code[bits] = code;
        ct->first_code[bits] = (uint16_t)code;
        ct->num_symbols[bits] = (uint16_t)bl_count[bits];
    }
    
    int sym_offset = 0;
    for (int bits = 1; bits <= HUFF_MAX_CODE_LEN; bits++) {
        ct->first_symbol[bits] = sym_offset;
        sym_offset += bl_count[bits];
    }
    
    ct->symbols = malloc(alphabet_size);
    uint32_t current_code_idx[HUFF_MAX_CODE_LEN + 1] = {0};

    memset(ct->lut, 0, sizeof(ct->lut));

    // Iterate 0..255 to assign canonical codes and populate symbols/LUT
    for (int i = 0; i < 256; i++) {
        int len = huff_table[i].len;
        if (len > 0) {
            uint32_t c = next_code[len];
            huff_table[i].code = c;
            
            int offset = ct->first_symbol[len] + current_code_idx[len];
            ct->symbols[offset] = (uint8_t)i;
            current_code_idx[len]++;
            next_code[len]++;
            
            if (len <= 6) {
                int shift = 6 - len;
                int start = c << shift;
                int count = 1 << shift;
                for (int j = 0; j < count; j++) {
                    ct->lut[start + j].symbol = (uint8_t)i;
                    ct->lut[start + j].len = (uint8_t)len;
                }
            }
        }
    }
}

void collect_string_freqs(char **strings, int *char_freqs) {
    int n = stbds_arrlen(strings);
    for (int i=0; i<n; i++) {
        const char *cur = strings[i];
        const char *prev = (i > 0) ? strings[i-1] : "";
        int common = 0;
        int min_len = MIN(strlen(prev), strlen(cur));
        while (common < min_len && prev[common] == cur[common]) common++;
        const char *suffix = cur + common;
        // Count terminator
        for (int k=0; k <= strlen(suffix); k++) char_freqs[(unsigned char)suffix[k]]++;
    }
}

void write_huff_header(CanonicalTables *ct, uint8_t **blob) {
    // Write Header & Tables to blob
    stbds_arrput(*blob, ct->max_len);
    stbds_arrput(*blob, 0); // padding
    
    // LUT (64 * 2 bytes)
    for (int k=0; k<64; k++) {
        stbds_arrput(*blob, ct->lut[k].symbol);
        stbds_arrput(*blob, ct->lut[k].len);
    }
    
    // Canonical Arrays
    int table_len = ct->max_len + 1;
    // first_code (uint16)
    for (int k=0; k<table_len; k++) {
        stbds_arrput(*blob, (ct->first_code[k] >> 8) & 0xFF);
        stbds_arrput(*blob, ct->first_code[k] & 0xFF);
    }
    // first_symbol (uint16)
    for (int k=0; k<table_len; k++) {
        stbds_arrput(*blob, (ct->first_symbol[k] >> 8) & 0xFF);
        stbds_arrput(*blob, ct->first_symbol[k] & 0xFF);
    }
    // num_symbols (uint16)
    for (int k=0; k<table_len; k++) {
        stbds_arrput(*blob, (ct->num_symbols[k] >> 8) & 0xFF);
        stbds_arrput(*blob, ct->num_symbols[k] & 0xFF);
    }
    
    // Symbols (alphabet_size bytes)
    // We calculate alphabet size from num_symbols sum
    int total_symbols = 0;
    for (int k=0; k<table_len; k++) total_symbols += ct->num_symbols[k];

    for (int k=0; k<total_symbols; k++) {
        stbds_arrput(*blob, ct->symbols[k]);
    }
}

