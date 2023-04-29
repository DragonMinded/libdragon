#ifndef __DSO_FORMAT_H
#define __DSO_FORMAT_H

#include <stdint.h>
#include <stdbool.h>

/** @brief DSO magic number */
#define DSO_MAGIC 0x44534F30 //'DSO0'
/** @brief Main executable symbol table magic */
#define DSO_MAINEXE_SYM_DATA_MAGIC 0x4D53594D //'MSYM'

/** @brief DSO symbol */
typedef struct dso_sym_s {
    char *name;         ///< Name of symbol
    uintptr_t value;    ///< Pointer to symbol
    uint32_t info;      ///< Top bit: absolute flag;  Next bit: weak flag; lowest 30 bits: size
} dso_sym_t;

/** @brief DSO file symbol */
typedef struct dso_file_sym_s {
    uint32_t name_ofs;  ///< Offset of name of symbol relative to first entry of symbol table
    uint32_t value;     ///< Value of symbol
    uint32_t info;      ///< Top bit: absolute flag; Next bit: weak flag; lowest 30 bits: size
} dso_file_sym_t;

/** @brief DSO relocation */
typedef struct dso_reloc_s {
    uint32_t offset;        ///< Program-relative offset of relocation target
    uint32_t info;          ///< Top 8 bits: type; lowest 24 bits: symbol index
} dso_reloc_t;

/** @brief DSO module */
typedef struct dso_module_s {
    dso_sym_t *syms;            ///< Symbols array
    uint32_t num_syms;          ///< Number of symbols (includes dummy symbol at start of array)
    uint32_t num_import_syms;   ///< Number of symbols imported
    dso_reloc_t *relocs;        ///< Relocation array
    uint32_t num_relocs;        ///< Number of relocations
    void *prog_base;            ///< Pointer to program memory image
    uint32_t prog_size;         ///< Size of program memory image
} dso_module_t;

/** @brief DSO file module */
typedef struct dso_file_module_s {
    uint32_t syms_ofs;          ///< Offset to symbols array
    uint32_t num_syms;          ///< Number of symbols (includes dummy symbol at start of array)
    uint32_t num_import_syms;   ///< Number of symbols imported
    uint32_t relocs_ofs;        ///< Offset to relocation array
    uint32_t num_relocs;        ///< Number of relocations
    uint32_t prog_ofs;          ///< Offset to program memory image (must be at end of file)
    uint32_t prog_size;         ///< Size of program memory image
} dso_file_module_t;

/** @brief Information to load DSO */
typedef struct dso_load_info_s {
    uint32_t magic;         ///< Magic number
    uint32_t size;          ///< File size excluding this struct
    uint32_t extra_mem;     ///< Size of extra memory needed for file
    uint32_t mem_align;     ///< Required memory alignment
} dso_load_info_t;

/** @brief Information to load main executable symbol table */
typedef struct mainexe_sym_info_s {
    uint32_t magic;     ///< Magic number
    uint32_t size;      ///< Size of data to load
    uint32_t num_syms;  ///< Number of symbols in this symbol table
} mainexe_sym_info_t;

#endif