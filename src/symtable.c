/**
 * @file symtable.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief SYMT symbol table access
 * @ingroup backtrace
 *
 * # N64 SYMT Format v4
 *
 * This document describes version 4 of the Symbol Table format (SYMT) used by Libdragon for
 * runtime symbolization and backtracing.
 *
 * The format is designed to be extremely compact in ROM while requiring a minimal fixed amount of
 * RAM (e.g. 512-1024 bytes) to be queried at runtime. It achieves this by using a combination of
 * delta encoding, variable-length integers (VarInt), and dictionary-based compression with Front
 * Coding and Canonical Huffman Coding.
 *
 * ## File Structure
 *
 * The file starts with a global header followed by several variable-length sections. All offsets
 * are relative to the beginning of the file. Everything is assumed to be big-endian unless
 * specified.
 *
 * ### Header
 *
 * ```c
 * typedef struct {
 *     char magic[4];           // "SYMT"
 *     uint32_t version;        // 4
 *     uint32_t num_symbols;    // Total number of symbols
 *     uint32_t num_chunks;     // Total number of compressed symbol chunks
 *
 *     // Offsets to sections
 *     uint32_t chunk_idx_off;  // Chunk Index
 *     uint32_t file_tab_off;   // File Block Table
 *     uint32_t func_tab_off;   // Func Block Table
 *     uint32_t huff_tab_off;   // Global Huffman Table
 *     uint32_t file_blob_off;  // File String Blob
 *     uint32_t func_blob_off;  // Func String Blob
 *     uint32_t stream_off;     // Compressed Symbol Stream
 *     
 *     // Size of sections (useful for bounds checking)
 *     uint32_t num_files;      // Total number of file strings
 *     uint32_t num_funcs;      // Total Number of func strings
 *     uint32_t num_file_blocks; // Number of file blocks
 *     uint32_t num_func_blocks; // Number of func blocks
 *     uint32_t huff_tab_size;  // Size of Huffman table in bytes
 *     uint32_t file_blob_size; // Size of file blob in bytes
 *     uint32_t func_blob_size; // Size of func blob in bytes
 *     uint32_t stream_size;    // Size of compressed stream in bytes
 * } symtable_header_t;
 * ```
 *
 * ## Sections
 *
 * ### 1. Chunk Index (`chunk_idx_off`)
 *
 * A sparse index used to perform binary search on the symbols, using their address. This is the
 * main entrypoint to lookup a symbol given its address. Symbols are divided into "Chunks" which are
 * individually compressed, so the Chunk Index actually identifies the chunk in which a symbol is
 * encoded.
 *
 * The structure is an array of `num_chunks` entries:
 *
 * ```c
 * typedef struct {
 *     uint32_t start_addr;    // The memory address of the first symbol in this chunk
 *     uint32_t stream_off;    // The byte offset of this chunk within the Symbol Stream section
 * } chunk_index_entry_t;
 * ```
 *
 * Since the index is sorted by memory address, it can be queried via bisection, to identify the
 * chunk in which the symbol is contained. Then the chunk must be parsed and decompressed linearly
 * until the symbol is found.
 *
 * ### 2. String Indexes (`file_tab_off`, `func_tab_off`)
 *
 * The SYMT file stores two groups of strings: file names and function names. Within each symbol,
 * the filename and the function name are referenced by an integer ID.
 *
 * The string indexes are sparse indexes used to locate strings in the compressed string blobs.
 * Strings are organized in compressed blocks of variable size. To find a string with ID `N`, we
 * perform a binary search on this index to find the block containing `N`.
 *
 * The structure is an array of `num_file_blocks` (or `num_func_blocks`) entries:
 *
 * ```c
 * typedef struct {
 *     uint32_t start_idx;     // The index of the first string in this block
 *     uint32_t blob_off;      // The byte offset of this block within the respective Blob section
 * } string_index_entry_t;
 * ```
 *
 * ### 3. Global Huffman Table (`huff_tab_off`)
 *
 * Contains the shared Canonical Huffman Table used to decompress string suffixes in both the File
 * and Func blobs. The table allows decoding individual characters from the compressed bitstream.
 *
 * **Format:**
 * *   `MaxLen` (1 byte): Maximum codeword length.
 * *   `Padding` (1 byte): Zeros.
 * *   `LUT` (128 bytes): 64-entry Lookup Table for fast decoding of codes <= 6 bits. Each entry is
 *     `Symbol` (1 byte) + `Len` (1 byte).
 * *   `FirstCode` (array of uint16): First canonical code for each length (7..MaxLen).
 * *   `FirstSymbol` (array of uint16): Index of the first symbol for each length (7..MaxLen),
 *     relative to the start of the symbols array, plus an extra terminal entry equal to the total
 *     number of stored symbols (len >= 6).
 * *   `Symbols` (array of uint8): The alphabet symbols with code length > 6, sorted by code length
 *     then lexicographically. Codes with length <= 6 are fully covered by the LUT and are not
 *     stored here.
 *
 * **Notes:**
 * * Arrays `FirstCode` and `FirstSymbol` only exist if `MaxLen >= 7`. The size of these arrays is
 *   `MaxLen - 6`.
 * * The `num_symbols` canonical array is omitted. The decoder derives the number of symbols for
 *   each length from `FirstSymbol` (or equivalently from successive `FirstCode` values) and from
 *   the total size of the `Symbols` array.
 * * The total size of the Huffman table is stored in the header field `huff_tab_size` and is used
 *   at runtime to know how many bytes of the trailing `Symbols` array are present after the
 *   canonical arrays.
 *
 * ### 4. String Blobs (`file_blob_off`, `func_blob_off`)
 *
 * Contains the actual string data, compressed using **Front Coding** combined with **Huffman
 * Coding**. The data is divided into blocks pointed to by the String Index. Each block is
 * independently compressed and fits into a fixed-size runtime buffer (e.g. 512 bytes).
 *
 * **Block Format:** Strings in the block are stored as delta from the previous string. The common
 * prefix length is stored as a **delta** relative to the previous string's common prefix length,
 * encoded using **Signed Exp-Golomb (k=0)**. The suffix itself is compressed using the Global
 * Huffman Table, including the null terminator (`\0`).
 *
 * ```
 * [PrefixLenDelta] [HuffmanEncodedSuffix...]
 * ```
 *
 * * `PrefixLenDelta` (Exp-Golomb): The difference between the current string's common prefix length
 *   and the *previous* string's common prefix length.
 *     *   `Delta = CurrentCommon - PrevCommon`.
 *     *   For the first string in a block, `PrevCommon` is 0.
 *     *   The delta is mapped to an unsigned integer using ZigZag encoding (`0->0`, `-1->1`,
 *         `1->2`, `-2->3`...) and then written using Exp-Golomb k=0 codes (interleaved in the
 *         bitstream).
 * * `HuffmanEncodedSuffix`: The suffix characters encoded using the global Huffman table. This is a
 *   bitstream. The stream continues until the Huffman code for `\0` is encountered.
 *
 * **Note:** Blocks are padded to 2-byte alignment at the end.
 *
 * ### 5. Compressed Symbol Stream (`stream_off`)
 *
 * Contains the symbol data. This is a continuous stream of bytes divided into Chunks (as defined in
 * the Chunk Index). Each Chunk can be decompressed independently.
 *
 * **Chunk Layout:**
 * * `FirstFuncOff` (VarInt): Offset of the first symbol in the chunk relative to the start of its
 *   function. If 0, the chunk starts exactly at a function boundary or the function base is unknown.
 * * Opcodes sequence (see below).
 * * `0x18` end-of-chunk marker, padded to 2-byte alignment.
 *
 * Inside a Chunk, symbols are stored sequentially using **Delta Encoding**. Address deltas are
 * relative to the previous symbol address, while file/function/line deltas are relative to a state
 * selected by symbol class (`inline` vs `non-inline`). The state consists of:
 * * `CurrentAddress` (Initialized to `chunk.start_addr`)
 * * `FileID[2]` (Initialized to 0, index 0=`non-inline`, 1=`inline`)
 * * `FuncID[2]` (Initialized to 0, index 0=`non-inline`, 1=`inline`)
 * * `Line[2]` (Initialized to 0, index 0=`non-inline`, 1=`inline`)
 *
 * **Symbol Encoding:** Each symbol starts with a 1-byte **Opcode**:
 *
 * | Bit | Name | Description |
 * | :--- | :--- | :--- |
 * | 7 | `New File` | If 1, a signed VarInt follows specifying the delta for `FileID`. |
 * | 6 | `New Func` | If 1, a signed VarInt follows specifying the delta for `FuncID`. |
 * | 5 | `New Line` | If 1, a signed VarInt follows specifying the delta for `Line`. |
 * | 4 | `Is Func` | If 1, this symbol marks the start of a function. |
 * | 3 | `Is Inline` | If 1, this symbol is an inlined function instance. |
 * | 2-0 | `Addr Delta` | Addr inc. <br> `0`: 0. <br> `1..6`: (`V*4`). <br> `7`: VarInt, Delta = (`V+7`)*4. |
 *
 * The opcode value `0x18` (EOM) is used to signal the end of the chunk. This is
 * guaranteed never to appear as a valid opcode for a symbol, because a function
 * start address can never be an inlined symbol.
 *
 * ## Data Types
 *
 * ### VarInt (Unsigned)
 * Variable-length integer encoding (LEB128-style).
 * *   Read byte.
 * *   Take low 7 bits.
 * *   If high bit (0x80) is set, shift and read next byte.
 *
 * ### Signed VarInt (ZigZag)
 * Maps signed integers to unsigned values to efficiently encode small negative numbers:
 * *   `0` -> `0`
 * *   `-1` -> `1`
 * *   `1` -> `2`
 * *   `-2` -> `3`
 * *   ...
 * *   Encoded/Decoded as standard VarInt.
 *
 * ## Runtime Algorithms
 *
 * These algorithms are designed to work with a small fixed-size buffer (e.g., 1KB) without dynamic
 * allocation.
 *
 * ### Algorithm 1: Symbol Lookup
 *
 * **Goal:** Find the symbol containing address `SearchAddr`.
 *
 * 1.  **Binary Search Chunk Index:**
 *     *   Load `chunk_index_t` entries from ROM.
 *     *   Find the chunk `C` such that `C.start_addr <= SearchAddr < (C+1).start_addr`.
 * 2.  **Load Chunk:**
 *     *   DMA the chunk data from `stream_off + C.stream_off` into the RAM buffer.
 * 3.  **Scan Stream:**
 *     *   Initialize state: `CurrAddr = C.start_addr`,
 *         `File[0..1]=0`, `Func[0..1]=0`, `Line[0..1]=0`.
 *     *   Loop through opcodes in the buffer:
 *         *   Decode Address Delta. Update `CurrAddr`.
 *         *   If `CurrAddr > SearchAddr`: Stop. The *previous* valid symbol is the match.
 *         *   Select class state with `sid = IsInline ? 1 : 0`.
 *         *   Update `File[sid]`, `Func[sid]`, `Line[sid]` based on flags and VarInts.
 *         *   Keep track of the "Best Match" seen so far.
 *
 * ### Algorithm 2: String Fetch
 *
 * **Goal:** Retrieve string with ID `TargetID` from a Blob.
 *
 * 1.  **Binary Search String Index:**
 *     *   Load `string_index_entry_t` entries from ROM.
 *     *   Find the entry `E` such that `E.start_idx <= TargetID < (E+1).start_idx`.
 * 2.  **Load Huffman Table:**
 *     *   (Optionally cached or loaded once) Load global Huffman table from `huff_tab_off`.
 * 3.  **Load Block:**
 *     *   DMA the block from `BlobOffset + E.blob_off` into the RAM buffer.
 * 4.  **Front Coding + Huffman Decode:**
 *     *   Initialize `CurrentString` (empty).
 *     *   Initialize `PrevCommon` = 0.
 *     *   Calculate `TargetInBlock = TargetID - E.start_idx`.
 *     *   Initialize bit_reader_t on the data.
 *     *   Loop `i` from 0 to `TargetInBlock`:
 *         *   Read `PrefixLenDelta` (Exp-Golomb, ZigZag).
 *         *   `CurrentCommon = PrevCommon + PrefixLenDelta`.
 *         *   `PrevCommon = CurrentCommon`.
 *         *   Truncate `CurrentString` to `CurrentCommon`.
 *         *   **Decode Suffix:**
 *             *   Loop until `\0` decoded:
 *                 *   Read next Huffman symbol using Table/LUT.
 *                 *   If not `\0`, append char to `CurrentString`.
 *     *   `CurrentString` is now the result.
 *
 * ### Algorithm 3: Backtrace Symbolization
 *
 * 1.  Call **Symbol Lookup** with the PC address.
 *     *   Result: `SymbolAddr`, `FileID`, `FuncID`, `Line`, `Offset`.
 * 2.  If `FuncID` is valid:
 *     *   Call **String Fetch** with `FuncID` to get the function name.
 * 3.  If `FileID` is valid:
 *     *   Call **String Fetch** with `FileID` to get the filename.
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

/** @brief Lookup table for the Huffman decoder */
typedef struct {
    uint8_t symbol;         ///< Symbol value for this entry
    uint8_t len;            ///< Length of the symbol in bits
} huff_lut_t;

/** @brief Huffman decoder */
typedef struct {
    huff_lut_t *lut;            ///< Lookup table for quick lookup (64 symbols)
    uint16_t *first_code;       ///< First code for each length (7..max_len)
    uint16_t *first_symbol;     ///< First symbol for each length (7..max_len)
    uint8_t *symbols;           ///< Symbols array (length > 6)
    int max_len;                ///< Maximum length of the symbols
    int len_count;              ///< Number of lengths (7..max_len)
} huff_decoder_t;

/** @brief Bit reader for the Huffman bitstream */
typedef struct {
    uint8_t *buf;               ///< Pointer to the buffer
    uint32_t cache;             ///< Cache for the bits
    int bits_in_cache;          ///< Number of bits in the cache
} bit_reader_t;

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
    if (symt_header.version != 4) {
        debugf("backtrace: unsupported symbol table version %ld -- please update your n64sym tool\n", symt_header.version);
        SYMT_ROM = 0;
        return (symtable_header_t){0};
    }

    return symt_header;
}

int symt_find_symbol(symtable_header_t *symt, uint32_t addr, symtable_entry_t *entry, int max_entries)
{
    // Binary search in the chunk index
    int min = 0;
    int max = symt->num_chunks - 1;

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
    uint8_t alignas(16) chunk_buf[MAX_BUFFER_SIZE];
    uint32_t stream_addr = SYMT_ROM + symt->stream_off + chunk_stream_off;
    
    // We don't know the exact size of the chunk, but we know it fits in MAX_BUFFER_SIZE.
    data_cache_hit_writeback_invalidate(chunk_buf, MAX_BUFFER_SIZE);
    dma_read(chunk_buf, stream_addr, MAX_BUFFER_SIZE);
    
    const uint8_t *ptr = chunk_buf;
    // First field: function offset of the first symbol in chunk (VarInt)
    uint32_t chunk_func_off = __read_varint_u64(&ptr);
    
    // Iterate through symbols in the chunk
    uint32_t cur_addr = chunk_start_addr;
    int cur_file[2] = {0, 0}; // [0]=non-inline, [1]=inline
    int cur_func[2] = {0, 0}; // [0]=non-inline, [1]=inline
    int cur_line[2] = {0, 0}; // [0]=non-inline, [1]=inline
    
    uint32_t last_func_addr = chunk_func_off ? (chunk_start_addr - chunk_func_off) : 0;
    int found = 0;
    
    while (1) {
        uint8_t op = *ptr++;
        if (op == 0x18) break; // End of chunk marker
        
        // Decode deltas
        int delta_file = (op & 0x80) ? __read_varint_s64(&ptr) : 0;
        int delta_func = (op & 0x40) ? __read_varint_s64(&ptr) : 0;
        int delta_line = (op & 0x20) ? __read_varint_s64(&ptr) : 0;
        
        // Decode address delta
        uint32_t delta_addr = 0;
        if ((op & 0x07) == 7) {
            delta_addr = (__read_varint_u64(&ptr) + 7) * 4;
        } else {
            delta_addr = (op & 0x07) * 4;
        }
        
        bool is_func = (op & 0x10);
        bool is_inline = (op & 0x08);
        int sid = is_inline ? 1 : 0;

        cur_file[sid] += delta_file;
        cur_func[sid] += delta_func;
        cur_line[sid] += delta_line;
        uint32_t sym_addr = cur_addr + delta_addr;
        
        if (is_func) last_func_addr = sym_addr;
        
        // If this is the function start, record it. In case the exact symbol
        // is not found, ee will return this approximation (function start + offset)
        if (sym_addr < addr && is_func) {
            last_func_addr = sym_addr;
            entry[0].func_sidx = cur_func[0];
            entry[0].file_sidx = cur_file[0];
            entry[0].line = 0;
            entry[0].func_off = addr - last_func_addr;
            entry[0].is_inline = 0;
            found = 1;
        }

        // Exact match: this is the symbol we were looking for
        if (sym_addr == addr) {
            if (entry[0].line == 0) found = 0; // Overwrite function-only entry
            if (found < max_entries) {
                entry[found].func_sidx = cur_func[sid];
                entry[found].file_sidx = cur_file[sid];
                entry[found].line = cur_line[sid];
                entry[found].is_inline = is_inline;
                if (last_func_addr) {
                    entry[found].func_off = addr - last_func_addr;
                } else {
                    entry[found].func_off = 0;
                }
            }
            found++;
            // If this is an inline symbol, keep searching for the parent function
            if (!is_inline)
                break;
        }

        // If we are past the address, we return the last found function symbol
        if (sym_addr > addr) {
            break; 
        }
        
        // Keep address for next iteration
        cur_addr = sym_addr;
    }
    
    return found;
}

static void br_fill(bit_reader_t *br) {
    while (br->bits_in_cache <= 24) {
        br->cache |= (uint32_t)(*br->buf++) << (24 - br->bits_in_cache);
        br->bits_in_cache += 8;
    }
}

static void br_init(bit_reader_t *br, uint8_t *buf) {
    br->buf = buf;
    br->cache = 0;
    br->bits_in_cache = 0;
    br_fill(br);
}

static int br_read_bits(bit_reader_t *br, int n) {
    if (br->bits_in_cache < n) br_fill(br);
    uint32_t val = (br->cache >> (32 - n));
    br->cache <<= n;
    br->bits_in_cache -= n;
    return val;
}

static int br_peek_bits(bit_reader_t *br, int n) {
    if (br->bits_in_cache < n) br_fill(br);
    return (br->cache >> (32 - n));
}

static void br_skip_bits(bit_reader_t *br, int n) {
    if (br->bits_in_cache < n) br_fill(br);
    br->cache <<= n;
    br->bits_in_cache -= n;
}

static int32_t read_exp_golomb_signed(bit_reader_t *br) {
    int zero_bits = 0;
    while (br_peek_bits(br, 1) == 0) {
        br_skip_bits(br, 1);
        zero_bits++;
    }
    
    uint32_t val = br_read_bits(br, zero_bits + 1) - 1;
    return (val >> 1) ^ -(val & 1);
}

static void huff_decoder_init(huff_decoder_t *dec, uint8_t *table_buf, uint32_t table_size) {
    // Layout: MaxLen(1), Padding(1), LUT(128), CanonicalArrays, Symbols
    dec->max_len = table_buf[0];
    dec->len_count = dec->max_len >= 7 ? dec->max_len - 6 : 0;
    uint8_t *ptr = table_buf + 2;
    dec->lut = (huff_lut_t*)ptr; ptr += 64 * 2;
    
    if (dec->len_count > 0) {
        dec->first_code = (uint16_t*)ptr; ptr += dec->len_count * 2;
        // first_symbol has one extra terminal entry containing total symbols
        dec->first_symbol = (uint16_t*)ptr; ptr += (dec->len_count + 1) * 2;
    } else {
        dec->first_code = NULL;
        dec->first_symbol = NULL;
    }
    dec->symbols = ptr;
}

static int huff_decode_symbol(huff_decoder_t *dec, bit_reader_t *br) {
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
    for (int idx = 0; idx < dec->len_count; idx++) {
        len = idx + 7;
        int code = peek16 >> (16 - len);
        
        uint16_t fc = dec->first_code[idx];
        uint16_t fs = dec->first_symbol[idx];
        uint16_t count = dec->first_symbol[idx + 1] - fs;
        
        if (code < fc + count) {
            br_skip_bits(br, len);
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
    int num_blocks = is_func ? symt->num_func_blocks : symt->num_file_blocks;
    
    // Binary search for the block
    int min = 0;
    int max = num_blocks - 1;

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
    uint32_t huff_size = MIN(symt->huff_tab_size, MAX_BUFFER_SIZE);
    uint8_t alignas(16) huff_tab[huff_size] __attribute__((uninitialized));
    data_cache_hit_writeback_invalidate(huff_tab, huff_size);
    dma_read(huff_tab, SYMT_ROM + symt->huff_tab_off, huff_size);
    
    huff_decoder_t dec;
    huff_decoder_init(&dec, huff_tab, huff_size);

    // Read string block
    uint8_t alignas(16) str_blob[MAX_BUFFER_SIZE] __attribute__((uninitialized));
    data_cache_hit_writeback_invalidate(str_blob, sizeof(str_blob));
    dma_read(str_blob, SYMT_ROM + blob_off + entry_blob_off, sizeof(str_blob));
    
    // Decode strings
    bit_reader_t br;
    br_init(&br, str_blob);
    
    int target_in_block = idx - entry_start_idx;
    
    // Reuse the caller-provided buffer as the rolling front-coding buffer.
    buf[0] = 0;
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
        if (prefix_len >= size-1) prefix_len = size - 2;
        
        // Decode suffix characters until \0 (or error)
        cur_len = prefix_len;
        int sym;
        while ((sym = huff_decode_symbol(&dec, &br)) > 0) {            
            if (cur_len < size - 1) {
                buf[cur_len++] = (char)sym;
            }
        }
        buf[cur_len] = 0;
    }
    
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
