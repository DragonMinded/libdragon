/**
 * @file symtable_internal.h
 * @brief Internal API for accessing the SYMT symbol table
 * @ingroup backtrace
 */

#ifndef LIBDRAGON_SYMTABLE_INTERNAL_H
#define LIBDRAGON_SYMTABLE_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

/** 
 * @brief Symbol table file header
 * 
 * The SYMT file is made of three main tables:
 * 
 * * Chunk Index: A sparse index of compressed chunks. Each entry points to a chunk in the
 *   compressed stream and the starting address of the symbols in that chunk.
 * * String Blobs (File/Func): Compressed strings using Front Coding in blocks of 16.
 * * Compressed Stream: The actual symbol data, delta-encoded and split into chunks.
 */
typedef struct {
    char head[4];           ///< Magic ID "SYMT"
    uint32_t version;       ///< Version of the symbol table
    uint32_t num_symbols;   ///< Total number of symbols
    uint32_t num_chunks;    ///< Total number of compressed symbol chunks

    // Offsets to sections
    uint32_t chunk_idx_off;  ///< Offset of chunk_index_t[]
    uint32_t file_tab_off;   ///< Offset of uint32_t[] (offsets into file_blob)
    uint32_t func_tab_off;   ///< Offset of uint32_t[] (offsets into func_blob)
    uint32_t huff_tab_off;   ///< Offset of the Global Huffman Table
    uint32_t file_blob_off;  ///< Offset of the file string blob
    uint32_t func_blob_off;  ///< Offset of the func string blob
    uint32_t stream_off;     ///< Offset of the compressed symbol stream
    
    // Size of sections (useful for bounds checking)
    uint32_t num_files;      ///< Number of file blocks
    uint32_t num_funcs;      ///< Number of func blocks
    uint32_t huff_tab_size;  ///< Size of Huffman table in bytes
    uint32_t file_blob_size; ///< Size of file blob in bytes
    uint32_t func_blob_size; ///< Size of func blob in bytes
    uint32_t stream_size;    ///< Size of compressed stream in bytes
} __attribute__((aligned(8))) symtable_header_t;

/** @brief Entry in the Chunk Index */
typedef struct {
    uint32_t start_addr;    ///< Address of the first symbol in this chunk
    uint32_t stream_off;    ///< Offset of the chunk in the compressed stream
} __attribute__((aligned(8))) symtable_chunk_index_entry_t;

/** @brief Entry in the String Index */
typedef struct {
    uint32_t start_idx;     ///< Index of the first string in this block
    uint32_t blob_off;      ///< Offset of the block in the string blob
} __attribute__((aligned(8))) symtable_string_index_entry_t;

/** @brief Symbol table entry **/
typedef struct {
    uint32_t func_sidx;     ///< Index of the function name in the string table
    uint32_t file_sidx;     ///< Index of the file name in the string table
    uint16_t line;          ///< Line number (or 0 if this symbol generically refers to a whole function)
    uint16_t func_off;      ///< Offset of the symbol within its function
} symtable_entry_t;

/** 
 * @brief Open the SYMT symbol table in the rompak.
 * 
 * If not found, return a null header.
 */
symtable_header_t symt_open(void *addr);

/** @brief Placeholder used in frames where symbols are not available */
extern const char *UNKNOWN_SYMBOL;

/**
 * @brief Find a symbol in the SYMT file by address.
 * 
 * @param symt      SYMT file header
 * @param addr      Address to search for
 * @param entry     Output entry structure
 * @return true if found, false otherwise
 */
bool symt_find_symbol(symtable_header_t *symt, uint32_t addr, symtable_entry_t *entry);

/**
 * @brief Fetch the function name of an entry
 * 
 * @param symt  SYMT file
 * @param entry Symbol entry
 * @param addr  Address being looked up (for special cases)
 * @param buf   Destination buffer
 * @param size  Size of the destination buffer
 * @return char*  Fetched string within the destination buffer
 */
char* symt_get_func_name(symtable_header_t *symt, symtable_entry_t *entry, uint32_t addr, char *buf, int size);

/**
 * @brief Fetch the file name of an entry
 * 
 * @param symt  SYMT file
 * @param entry Symbol entry
 * @param addr  Address being looked up
 * @param buf   Destination buffer
 * @param size  Size of the destination buffer
 * @return char*  Fetched string within the destination buffer
 */
char* symt_get_file_name(symtable_header_t *symt, symtable_entry_t *entry, uint32_t addr, char *buf, int size);

#endif // LIBDRAGON_SYMTABLE_INTERNAL_H

