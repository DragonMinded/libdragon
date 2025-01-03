#ifndef __DSO_FORMAT_H
#define __DSO_FORMAT_H

#include <stdint.h>
#include <stdbool.h>

/** @brief DSO magic number */
#define DSO_MAGIC 0x44534F30 //'DSO0'
/** @brief Main executable symbol table magic */
#define DSO_MAINEXE_SYM_DATA_MAGIC 0x4D53594D //'MSYM'

/** @brief Offset of syms member of dso_module_t on N64 */
#define DSO_SYMS_OFS 0x18
/** @brief Offset of num_import_syms member of dso_module_t on N64 */
#define DSO_NUM_IMPORT_SYMS_OFS 0x20
/** @brief Size of dso_sym_t on N64 */
#define DSO_SYM_SIZE 0xC

/** @brief DSO symbol */
typedef struct dso_sym_s {
    char *name;         ///< Name of symbol
    uint32_t value;     ///< Pointer to symbol
    uint32_t info;      ///< Top bit: absolute flag;  Next bit: weak flag; lowest 30 bits: size
} dso_sym_t;

/** @brief DSO relocation */
typedef struct dso_reloc_s {
    uint32_t offset;        ///< Program-relative offset of relocation target
    uint32_t info;          ///< Top 8 bits: type; lowest 24 bits: symbol index
} dso_reloc_t;

/** @brief DSO module data */
typedef struct dso_module_s {
    uint32_t magic;             ///< Magic number
    struct dso_module_s *prev;  ///< Previous loaded dynamic library
    struct dso_module_s *next;  ///< Next loaded dynamic library
    uint32_t ref_count;         ///< Dynamic library reference count
    char *src_elf;              ///< Path to Source ELF
    char *filename;             ///< Filename data
    dso_sym_t *syms;            ///< Symbols array
    uint32_t num_syms;          ///< Number of symbols (includes dummy symbol at start of array)
    uint32_t num_import_syms;   ///< Number of symbols imported
    dso_reloc_t *relocs;        ///< Relocation array
    uint32_t num_relocs;        ///< Number of relocations
    void *prog_base;            ///< Pointer to program memory image
    uint32_t prog_size;         ///< Size of program memory image
    uint32_t ehframe_obj[6];    ///< Exception frame object
    uint32_t sym_romofs;        ///< Debug symbol data rom address
    uint32_t mode;              ///< Dynamic library flags
} dso_module_t;

/** @brief Information to load main executable symbol table */
typedef struct mainexe_sym_info_s {
    uint32_t magic;     ///< Magic number
    uint32_t size;      ///< Size of data to load
    uint32_t num_syms;  ///< Number of symbols in this symbol table
} mainexe_sym_info_t;

#endif