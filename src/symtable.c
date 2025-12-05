/**
 * @file symtable.c
 * @brief SYMT symbol table access
 * @ingroup backtrace
 *
 * This file implements the SYMT symbol table access. The symbol table is a
 * compact format for storing symbol information in a ROM. It is designed to
 * be accessed directly from ROM, with a maximum of 512 bytes of RAM used at
 * runtime for buffering, so that it can be used in hard scenarios like crashes
 * and end of memory situations.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <string.h>
#include <stdio.h>
#include "symtable_internal.h"
#include "n64sys.h"
#include "dma.h"
#include "rompak_internal.h"
#include "dlfcn_internal.h"
#include "debug.h"
#include "utils.h"

/** @brief Buffer size for decompression */
#define MAX_BUFFER_SIZE 512

/** @brief Maximum code length for Huffman decoding */
#define HUFF_MAX_CODE_LEN 16

/** @brief Exception handler (see inthandler.S) */
extern uint32_t inthandler[];
/** @brief End of exception handler (see inthandler.S) */
extern uint32_t inthandler_end[];

/** @brief Start of main executable text section */
extern uint32_t __text_start[];
/** @brief End of main executable text section */
extern uint32_t __text_end[];

/** @brief Address of the SYMT symbol table in the rompak. */
static uint32_t SYMT_ROM = 0xFFFFFFFF;

/** @brief Placeholder used in frames where symbols are not available */
const char *UNKNOWN_SYMBOL = "???";

/** @brief Module resolver (from backtrace.c or dlfcn.c) */
extern module_lookup_func __bt_lookup_module;

/** @brief Read a variable-length integer from the buffer */
static uint32_t read_varint(uint8_t **ptr)
{
    uint32_t val = 0;
    int shift = 0;
    while (1) {
        uint8_t byte = *(*ptr)++;
        val |= (byte & 0x7F) << shift;
        if (!(byte & 0x80)) break;
        shift += 7;
    }
    return val;
}

/** @brief Read a signed variable-length integer from the buffer (zigzag encoded) */
static int32_t read_signed_varint(uint8_t **ptr)
{
    uint32_t val = read_varint(ptr);
    return (val >> 1) ^ -(val & 1);
}

/** @brief Check if addr is inside main executable text section */
static bool is_main_exe_text_address(uint32_t addr)
{
    return addr >= (uint32_t)__text_start && addr < (uint32_t)__text_end;
}

symtable_header_t symt_open(void *addr) {
    if(is_main_exe_text_address((uint32_t)addr)) {
        //Open SYMT from rompak
        static uint32_t mainexe_symt = 0xFFFFFFFF;
        if (mainexe_symt == 0xFFFFFFFF) {
            mainexe_symt = rompak_search_ext(".sym", NULL);
            if (!mainexe_symt)
                debugf("backtrace: no symbol table found in the rompak\n");
        }
        SYMT_ROM = mainexe_symt;
    } else {
        dl_module_t *module = NULL;
        if(__bt_lookup_module) {
            module = __bt_lookup_module(addr);
        }
        if(module && module->sym_romofs != 0) {
            //Read module SYMT
            SYMT_ROM = module->sym_romofs;
        } else {
            SYMT_ROM = 0;
        }
    }
    
    if (!SYMT_ROM) {
        return (symtable_header_t){0};
    }

    symtable_header_t symt_header;
    data_cache_hit_writeback_invalidate(&symt_header, sizeof(symt_header));
    dma_read(&symt_header, SYMT_ROM, sizeof(symtable_header_t));

    if (symt_header.head[0] != 'S' || symt_header.head[1] != 'Y' || symt_header.head[2] != 'M' || symt_header.head[3] != 'T') {
        debugf("backtrace: invalid symbol table found at 0x%08lx\n", SYMT_ROM);
        SYMT_ROM = 0;
        return (symtable_header_t){0};
    }
    if (symt_header.version != 3) {
        debugf("backtrace: unsupported symbol table version %ld -- please update your n64sym tool\n", symt_header.version);
        SYMT_ROM = 0;
        return (symtable_header_t){0};
    }

    return symt_header;
}

bool symt_find_symbol(symtable_header_t *symt, uint32_t addr, symtable_entry_t *entry)
{
    // Binary search in the chunk index
    int min = 0;
    int max = symt->num_chunks - 1;

    _Static_assert(sizeof(symtable_chunk_index_entry_t) == 8, "symtable_chunk_index_entry_t must be 8 bytes");
    while (min < max) {
        int mid = (min + max + 1) / 2;
        int chunk_start_addr = io_read(SYMT_ROM + symt->chunk_idx_off + mid * 8);
        if (addr < chunk_start_addr)
            max = mid - 1;
        else
            min = mid;
    }
    
    // Read the chunk entry for the found chunk
    int chunk_start_addr = io_read(SYMT_ROM + symt->chunk_idx_off + min * 8 + 0);
    int chunk_stream_off = io_read(SYMT_ROM + symt->chunk_idx_off + min * 8 + 4);
    if (addr < chunk_start_addr)
        return false; // Should not happen if address is valid code

    // Decompress the chunk
    uint8_t alignas(8) chunk_buf[MAX_BUFFER_SIZE];
    uint32_t stream_addr = SYMT_ROM + symt->stream_off + chunk_stream_off;
    
    // We don't know the exact size of the chunk, but we know it fits in MAX_BUFFER_SIZE.
    data_cache_hit_writeback_invalidate(chunk_buf, MAX_BUFFER_SIZE);
    dma_read(chunk_buf, stream_addr, MAX_BUFFER_SIZE);
    
    uint8_t *ptr = chunk_buf;
    
    // Iterate through symbols in the chunk
    uint32_t cur_addr = chunk_start_addr;
    int cur_file = 0;
    int cur_func = 0;
    int cur_line = 0;
    
    uint32_t last_func_addr = 0;
    bool found = false;
    
    while (1) {
        uint8_t op = *ptr++;
        if (op == 0) break; // End of chunk marker
        
        // Decode deltas
        int delta_file = (op & 0x80) ? read_signed_varint(&ptr) : 0;
        int delta_func = (op & 0x40) ? read_signed_varint(&ptr) : 0;
        int delta_line = (op & 0x20) ? read_signed_varint(&ptr) : 0;
        
        // Decode address delta
        uint32_t delta_addr = 0;
        if ((op & 0x07) == 7) {
            delta_addr = (read_varint(&ptr) + 7) * 4;
        } else {
            delta_addr = (op & 0x07) * 4;
        }
        
        cur_file += delta_file;
        cur_func += delta_func;
        cur_line += delta_line;
        uint32_t sym_addr = cur_addr + delta_addr;
        bool is_func = (op & 0x10);
        
        if (is_func) last_func_addr = sym_addr;
        
        if (sym_addr > addr) {
            break; 
        }
        
        // This symbol is <= addr. It's a candidate.
        entry->func_sidx = cur_func;
        entry->file_sidx = cur_file;
        entry->line = cur_line;
        // Estimate func_off. If we saw a function start, use it.
        // Otherwise, use 0 (or we could try to handle it better, but V3 limitation).
        if (last_func_addr)
            entry->func_off = addr - last_func_addr;
        else
            entry->func_off = 0;
            
        found = true;
        cur_addr = sym_addr;
    }
    
    return found;
}

/** @brief Bitreader for the Huffman bitstream */
typedef struct {
    uint8_t *buf;
    uint32_t cache;
    int bits_in_cache;
} BitReader;

static void br_fill(BitReader *br) {
    while (br->bits_in_cache <= 24) {
        br->cache |= (uint32_t)(*br->buf++) << (24 - br->bits_in_cache);
        br->bits_in_cache += 8;
    }
}

static void br_init(BitReader *br, uint8_t *buf) {
    br->buf = buf;
    br->cache = 0;
    br->bits_in_cache = 0;
    br_fill(br);
}

static int br_read_bits(BitReader *br, int n) {
    if (br->bits_in_cache < n) br_fill(br);
    uint32_t val = (br->cache >> (32 - n));
    br->cache <<= n;
    br->bits_in_cache -= n;
    return val;
}

static int br_peek_bits(BitReader *br, int n) {
    if (br->bits_in_cache < n) br_fill(br);
    return (br->cache >> (32 - n));
}

static void br_skip_bits(BitReader *br, int n) {
    if (br->bits_in_cache < n) br_fill(br);
    br->cache <<= n;
    br->bits_in_cache -= n;
}

static int32_t read_exp_golomb_signed(BitReader *br) {
    int zero_bits = 0;
    while (br_peek_bits(br, 1) == 0) {
        br_skip_bits(br, 1);
        zero_bits++;
    }
    
    uint32_t val = br_read_bits(br, zero_bits + 1) - 1;
    return (val >> 1) ^ -(val & 1);
}

typedef struct {
    uint8_t symbol;
    uint8_t len;
} Lut64Entry;

typedef struct {
    Lut64Entry *lut;
    uint16_t *first_code;
    uint16_t *first_symbol;
    uint16_t *num_symbols;
    uint8_t *symbols;
    int max_len;
} HuffDecoder;

static void huff_decoder_init(HuffDecoder *dec, uint8_t *table_buf) {
    // Layout: MaxLen(1), Padding(1), LUT(128), CanonicalArrays, Symbols
    dec->max_len = table_buf[0];
    uint8_t *ptr = table_buf + 2;
    dec->lut = (Lut64Entry*)ptr; ptr += 64 * 2;
    
    int table_len = dec->max_len + 1;
    dec->first_code = (uint16_t*)ptr; ptr += table_len * 2;
    dec->first_symbol = (uint16_t*)ptr; ptr += table_len * 2;
    dec->num_symbols = (uint16_t*)ptr; ptr += table_len * 2;
    dec->symbols = ptr;
}

static int huff_decode_symbol(HuffDecoder *dec, BitReader *br) {
    // Peek 16 bits (enough for max code length)
    int peek16 = br_peek_bits(br, 16);
    
    // Try LUT first (6 bits)
    int peek6 = peek16 >> 10;
    int len = dec->lut[peek6].len;
    if (len > 0) {
        br_skip_bits(br, len);
        return dec->lut[peek6].symbol;
    }
    
    // Canonical fallback
    for (len = 7; len <= dec->max_len; len++) {
        int code = peek16 >> (16 - len);
        
        uint16_t fc = dec->first_code[len];
        uint16_t count = dec->num_symbols[len];
        
        if (code < fc + count) {
            br_skip_bits(br, len);
            uint16_t fs = dec->first_symbol[len];
            return dec->symbols[fs + (code - fc)];
        }
    }
    
    return -1;
}

/**
 * @brief Fetch a string from the string table
 * 
 * @param symt  SYMT file
 * @param idx   Index of the string in the string table (index, not offset)
 * @param buf   Destination buffer
 * @param size  Size of the destination buffer
 * @param is_func True if fetching a function name, false for file name
 * @return char*  Fetched string within the destination buffer
 */
static char* symt_get_string(symtable_header_t *symt, int idx, char *buf, int size, bool is_func)
{
    if (idx < 0) {
        snprintf(buf, size, "%s", UNKNOWN_SYMBOL);
        return buf;
    }

    uint32_t tab_off = is_func ? symt->func_tab_off : symt->file_tab_off;
    uint32_t blob_off = is_func ? symt->func_blob_off : symt->file_blob_off;
    int num_blocks = is_func ? symt->num_funcs : symt->num_files;
    
    // Binary search for the block
    int min = 0;
    int max = num_blocks - 1;

    _Static_assert(sizeof(symtable_string_index_entry_t) == 8, "symtable_string_index_entry_t must be 8 bytes");
    while (min < max) {
        int mid = (min + max + 1) / 2;
        uint32_t entry_start_idx = io_read(SYMT_ROM + tab_off + mid * 8 + 0);
        if (idx < entry_start_idx)
            max = mid - 1;
        else
            min = mid;
    }
    
    // Read the block entry
    uint32_t entry_start_idx = io_read(SYMT_ROM + tab_off + min * 8 + 0);
    uint32_t entry_blob_off = io_read(SYMT_ROM + tab_off + min * 8 + 4);
    if (idx < entry_start_idx) {
        snprintf(buf, size, "%s", UNKNOWN_SYMBOL);
        return buf;
    }
    
    // Read Huffman Table (Global)
    uint8_t alignas(8) huff_tab[MAX_BUFFER_SIZE] __attribute__((uninitialized));
    data_cache_hit_writeback_invalidate(huff_tab, MAX_BUFFER_SIZE);
    dma_read(huff_tab, SYMT_ROM + symt->huff_tab_off, MAX_BUFFER_SIZE);
    
    HuffDecoder dec;
    huff_decoder_init(&dec, huff_tab);

    // Read string block
    uint8_t alignas(8) str_blob[MAX_BUFFER_SIZE] __attribute__((uninitialized));
    data_cache_hit_writeback_invalidate(str_blob, sizeof(str_blob));
    dma_read(str_blob, SYMT_ROM + blob_off + entry_blob_off, sizeof(str_blob));
    
    // Decode strings
    BitReader br;
    br_init(&br, str_blob);
    
    int target_in_block = idx - entry_start_idx;
    
    // Buffer to hold current string state (front coding context)
    char cur_str[MAX_BUFFER_SIZE] __attribute__((uninitialized));
    cur_str[0] = 0;

    int cur_len = 0;
    int prev_prefix_len = 0;    
    for (int i=0; i <= target_in_block; i++) {
        // Decode prefix length delta, and apply it to the current common length
        int32_t prefix_delta = read_exp_golomb_signed(&br);
        int prefix_len = prev_prefix_len + prefix_delta;
        prev_prefix_len = prefix_len;
        
        // Safety checks. We can't really assert here (as the assert would trigger this code again)
        // so better make sure we don't cause buffer overflows in case of corruption.
        if (prefix_len > cur_len) prefix_len = cur_len;
        if (prefix_len >= MAX_BUFFER_SIZE-1) prefix_len = MAX_BUFFER_SIZE - 2;
        
            // Decode suffix characters until \0 (or error)
        cur_len = prefix_len;
        int sym;
        while ((sym = huff_decode_symbol(&dec, &br)) > 0) {            
            if (cur_len < MAX_BUFFER_SIZE - 1) {
                cur_str[cur_len++] = (char)sym;
            }
        }
        cur_str[cur_len] = 0;
    }
    
    snprintf(buf, size, "%s", cur_str);
    return buf;
}

char* symt_get_func_name(symtable_header_t *symt, symtable_entry_t *entry, uint32_t addr, char *buf, int size)
{
    if (addr >= (uint32_t)inthandler && addr < (uint32_t)inthandler_end) {
        // Special case exception handlers. This is just to show something slightly
        // more readable instead of "notcart+0x0" or similar assembly symbols
        snprintf(buf, size, "<EXCEPTION HANDLER>");
        return buf;
    } else {
        return symt_get_string(symt, entry->func_sidx, buf, size, true);
    }
}

char* symt_get_file_name(symtable_header_t *symt, symtable_entry_t *entry, uint32_t addr, char *buf, int size)
{
    return symt_get_string(symt, entry->file_sidx, buf, size, false);
}
