#ifndef __USO_FORMAT_H
#define __USO_FORMAT_H

#include <stdint.h>
#include <stdbool.h>

/** @brief USO magic number */
#define USO_MAGIC 0x55534F30 //'USO0'
/** @brief Main executable symbol table magic */
#define USO_MAINEXE_SYM_DATA_MAGIC 0x4D53594D //'MSYM'

/** @brief USO symbol */
typedef struct uso_sym_s {
    char *name;         ///< Name of symbol
    uintptr_t value;    ///< Pointer to symbol
    uint32_t info;      ///< Top bit: absolute flag;  Next bit: weak flag; lowest 30 bits: size
} uso_sym_t;

/** @brief USO file symbol */
typedef struct uso_file_sym_s {
    uint32_t name_ofs;  ///< Offset of name of symbol relative to first entry of symbol table
    uint32_t value;     ///< Value of symbol
    uint32_t info;      ///< Top bit: absolute flag; Next bit: weak flag; lowest 30 bits: size
} uso_file_sym_t;

/** @brief USO relocation */
typedef struct uso_reloc_s {
    uint32_t offset;        ///< Program-relative offset of relocation target
    uint32_t info;          ///< Top 8 bits: type; lowest 24 bits: symbol index
} uso_reloc_t;

/** @brief USO module */
typedef struct uso_module_s {
    uso_sym_t *syms;            ///< Symbols array
    uint32_t num_syms;          ///< Number of symbols (includes dummy symbol at start of array)
    uint32_t num_import_syms;   ///< Number of symbols imported
    uso_reloc_t *relocs;        ///< Relocation array
    uint32_t num_relocs;        ///< Number of relocations
    void *prog_base;            ///< Pointer to program memory image
    uint32_t prog_size;         ///< Size of program memory image
} uso_module_t;

/** @brief USO file module */
typedef struct uso_file_module_s {
    uint32_t syms_ofs;          ///< Offset to symbols array
    uint32_t num_syms;          ///< Number of symbols (includes dummy symbol at start of array)
    uint32_t num_import_syms;   ///< Number of symbols imported
    uint32_t relocs_ofs;        ///< Offset to relocation array
    uint32_t num_relocs;        ///< Number of relocations
    uint32_t prog_ofs;          ///< Offset to program memory image (must be at end of file)
    uint32_t prog_size;         ///< Size of program memory image
} uso_file_module_t;

/** @brief Information to load USO */
typedef struct uso_load_info_s {
    uint32_t magic;         ///< Magic number
    uint32_t size;          ///< File size excluding this struct
    uint32_t extra_mem;     ///< Size of extra memory needed for file
    uint32_t mem_align;     ///< Required memory alignment
} uso_load_info_t;

/** @brief Information to load main executable symbol table */
typedef struct mainexe_sym_info_s {
    uint32_t magic;     ///< Magic number
    uint32_t size;      ///< Size of data to load
    uint32_t num_syms;  ///< Number of symbols in this symbol table
} mainexe_sym_info_t;

#endif