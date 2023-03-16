#ifndef __USO_FORMAT_H
#define __USO_FORMAT_H

#include <stdint.h>
#include <stdbool.h>

/** @brief USO header magic number */
#define USO_HEADER_MAGIC 0x55534F30 //'USO0'
#define USO_GLOBAL_SYM_DATA_MAGIC 0x4D53594D //'MSYM'
#define USO_MAX_SECTIONS 255

/** @brief USO symbol */
typedef struct uso_sym_s {
    char *name;         ///< Name of symbol
    uintptr_t value;    ///< Pointer to symbol
    uint32_t info;      ///< Top 8 bits: section; Next bit: weak flag; lowest 23 bits: size
} uso_sym_t;

/** @brief USO file symbol */
typedef struct uso_file_sym_s {
    uint32_t name_ofs;  ///< Offset of name of symbol relative to first entry of symbol table
    uint32_t value;     ///< Value of symbol
    uint32_t info;      ///< Top 8 bits: section; Next bit: weak flag; lowest 23 bits: size
} uso_file_sym_t;

/** @brief USO symbol table */
typedef struct uso_sym_table_s {
    uint32_t length;        ///< Size of symbol table
    uso_sym_t *data;        ///< Start of symbol table
} uso_sym_table_t;

/** @brief USO file symbol table */
typedef struct uso_file_sym_table_s {
    uint32_t length;        ///< Size of symbol table
    uint32_t data_ofs;      ///< Start of symbol table
} uso_file_sym_table_t;

/** @brief USO relocation */
typedef struct uso_reloc_s {
    uint32_t offset;        ///< Section-relative offset of relocation target
    uint32_t info;          ///< Top 8 bits: type; lowest 24 bits: index
    uint32_t sym_value;     ///< Value of internal symbols
} uso_reloc_t;

/** @brief USO relocation table */
typedef struct uso_reloc_table_s {
    uint32_t length;        ///< Size of relocation table
    uso_reloc_t *data;      ///< Start of relocation table
} uso_reloc_table_t;

/** @brief USO file relocation table */
typedef struct uso_file_reloc_table_s {
    uint32_t length;        ///< Size of relocation table
    uint32_t data_ofs;      ///< Start of relocation table
} uso_file_reloc_table_t;

/** @brief USO section data */
typedef struct uso_section_s {
    void *data;                             ///< Section data pointer
    uint32_t size;                          ///< Section size
    uint32_t align;                         ///< Section alignment
    uso_reloc_table_t relocs;               ///< List of USO internal relocations
    uso_reloc_table_t ext_relocs;   ///< List of USO external relocations
} uso_section_t;

/** @brief USO file section data */
typedef struct uso_file_section_s {
    uint32_t data_ofs;                          ///< Section data pointer
    uint32_t size;                              ///< Section size
    uint32_t align;                             ///< Section alignment
    uso_file_reloc_table_t relocs;              ///< List of USO internal relocations
    uso_file_reloc_table_t ext_relocs;  ///< List of USO external relocations
} uso_file_section_t;

/** @brief USO module */
typedef struct uso_module_s {
    uint32_t magic;             ///< Magic number
    uso_section_t *sections;    ///< Sections array
    uso_sym_table_t syms;       ///< Internally defined symbols array
    uso_sym_table_t ext_syms;   ///< Externally defined symbols array
    uint8_t num_sections;       ///< Section count
    uint8_t eh_frame_section;   ///< .eh_frame section index
    uint8_t ctors_section;      ///< .ctors section index
    uint8_t dtors_section;      ///< .dtors section index
    uint8_t text_section;       ///< First executable section
    uint8_t __padding[3];       ///< Padding
} uso_module_t;

/** @brief USO file module */
typedef struct uso_file_module_s {
    uint32_t magic;                 ///< Magic number
    uint32_t sections_ofs;          ///< Sections array
    uso_file_sym_table_t syms;      ///< Internally defined symbols array
    uso_file_sym_table_t ext_syms;  ///< Externally defined symbols array
    uint8_t num_sections;           ///< Section count
    uint8_t eh_frame_section;       ///< .eh_frame section index
    uint8_t ctors_section;          ///< .ctors section index
    uint8_t dtors_section;          ///< .dtors section index
    uint8_t text_section;           ///< First executable section
    uint8_t __padding[3];           ///< Padding
} uso_file_module_t;

/** @brief Information to load USO */
typedef struct uso_load_info_s {
    uint32_t size;          ///< USO size excluding this struct
    uint32_t noload_size;   ///< Total noload section size
    uint16_t align;         ///< Required USO alignment
    uint16_t noload_align;  ///< Required USO noload section alignment 
} uso_load_info_t;

typedef struct mainexe_sym_info_s {
    uint32_t magic;
    uint32_t size;
} mainexe_sym_info_t;

#endif