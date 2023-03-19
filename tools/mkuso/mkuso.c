#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include "../common/binout.h"

#define STB_DS_IMPLEMENTATION
#include "../common/stb_ds.h"

// Compression library
#include "../common/assetcomp.h"
#include "../common/assetcomp.c"

//Macros copied from utils.h in libdragon src directory
#define ROUND_UP(n, d) ({ \
    typeof(n) _n = n; typeof(d) _d = d; \
    (((_n) + (_d) - 1) / (_d) * (_d)); \
})
#define MAX(a,b)  ({ typeof(a) _a = a; typeof(b) _b = b; _a > _b ? _a : _b; })

//ELF structs
#include "mips_elf.h"

typedef struct elf_section_s {
    char *name;
    Elf32_Word type;
    Elf32_Word flags;
    Elf32_Addr addr;
    Elf32_Off offset;
    Elf32_Word size;
    Elf32_Word link;
    Elf32_Word info;
    Elf32_Word align;
} elf_section_t;

typedef struct elf_symbol_s {
    char *name;
    Elf32_Addr value;
    Elf32_Word size;
    unsigned char info;
    unsigned char other;
    Elf32_Section section;
} elf_symbol_t;

typedef struct elf_info_s {
    FILE *file;
    Elf32_Ehdr header;
    elf_section_t *sections;
    char *strtab;
    char *section_strtab;
    elf_symbol_t *syms;
    Elf32_Section *uso_src_sections;
    elf_symbol_t **uso_syms;
    elf_symbol_t **uso_ext_syms;
} elf_info_t;

//USO Internals
#include "../../src/uso_format.h"

#include "mips_elf.h"

bool verbose_flag = false;

static void bswap32(uint32_t *ptr)
{
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    *ptr = (((*ptr >> 24) & 0xFF) << 0)
        | (((*ptr >> 16) & 0xFF) << 8)
        | (((*ptr >> 8)  & 0xFF) << 16)
        | (((*ptr >> 0)  & 0xFF) << 24);
    #endif
}

static void bswap16(uint16_t *ptr)
{
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    *ptr = (((*ptr >> 8) & 0xFF) << 0)
        | (((*ptr >> 0) & 0xFF) << 8);
    #endif
}

static bool read_checked(FILE *file, size_t offset, void *dst, size_t size)
{
    return fseek(file, offset, SEEK_SET) == 0 && fread(dst, size, 1, file) == 1;
}

// Printf if verbose
void verbose(const char *fmt, ...) {
    if (verbose_flag) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
}

void print_args(char *name)
{
    fprintf(stderr, "Usage: %s [flags] <input elfs>\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -v/--verbose                Verbose output\n");
    fprintf(stderr, "   -o/--output <dir>           Specify output directory (default: .)\n");
    fprintf(stderr, "   -c/--compress               Compress output\n");
    fprintf(stderr, "\n");
}

elf_info_t *elf_info_init(const char *filename)
{
    elf_info_t *elf_info = calloc(1, sizeof(elf_info_t));
    elf_info->file = fopen(filename, "rb");
    return elf_info;
}

void elf_info_free(elf_info_t *elf_info)
{
    //Close attached file
    if(!elf_info->file) {
        fclose(elf_info->file);
    }
    //Free arrays
    arrfree(elf_info->sections);
    arrfree(elf_info->syms);
    arrfree(elf_info->uso_src_sections);
    arrfree(elf_info->uso_syms);
    arrfree(elf_info->uso_ext_syms);
    free(elf_info->strtab); //Free string table
    free(elf_info->section_strtab); //Free section string table
    free(elf_info);
}

bool elf_header_read(elf_info_t *elf_info)
{
    //Try to read ELF header
    if(!read_checked(elf_info->file, 0, &elf_info->header, sizeof(Elf32_Ehdr))) {
        fprintf(stderr, "Failed to read ELF header\n");
        return false;
    }
    //Verify that input is an ELF file
    if (memcmp(elf_info->header.e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "Invalid ELF file\n");
        return false;
    }
    //Verify that ELF is 32-bit big endian
    if(elf_info->header.e_ident[EI_CLASS] != ELFCLASS32 || elf_info->header.e_ident[EI_DATA] != ELFDATA2MSB) {
        fprintf(stderr, "ELF is not for a 32-bit big endian platform\n");
        return false;
    }
    //Byteswap ELF type and machine
    bswap16(&elf_info->header.e_type);
    bswap16(&elf_info->header.e_machine);
    //Verify that ELF is relocatable MIPS ELF
    if(elf_info->header.e_type != ET_REL || elf_info->header.e_machine != EM_MIPS) {
        fprintf(stderr, "ELF is not a valid MIPS object file\n");
        return false;
    }
    //Byteswap rest of ELF fields
    bswap32(&elf_info->header.e_version);
    bswap32(&elf_info->header.e_entry);
    bswap32(&elf_info->header.e_phoff);
    bswap32(&elf_info->header.e_shoff);
    bswap32(&elf_info->header.e_flags);
    bswap16(&elf_info->header.e_ehsize);
    bswap16(&elf_info->header.e_phentsize);
    bswap16(&elf_info->header.e_phnum);
    bswap16(&elf_info->header.e_shentsize);
    bswap16(&elf_info->header.e_shnum);
    bswap16(&elf_info->header.e_shstrndx);
    return true;
}

bool elf_section_header_read(elf_info_t *elf_info, uint16_t index, Elf32_Shdr *section)
{
    size_t section_offset = elf_info->header.e_shoff+(index*elf_info->header.e_shentsize);
    //Warn if invalid section is read
    if(index >= elf_info->header.e_shnum) {
        fprintf(stderr, "Trying to read invalid section %d\n", index);
        return false;
    }
    //Read section header
    if(!read_checked(elf_info->file, section_offset, section, sizeof(Elf32_Shdr))) {
        fprintf(stderr, "Failed to read ELF section %d\n", index);
        return false;
    }
    //Byteswap section header
    bswap32(&section->sh_name);
    bswap32(&section->sh_type);
    bswap32(&section->sh_flags);
    bswap32(&section->sh_addr);
    bswap32(&section->sh_offset);
    bswap32(&section->sh_size);
    bswap32(&section->sh_link);
    bswap32(&section->sh_info);
    bswap32(&section->sh_addralign);
    bswap32(&section->sh_entsize);
    return true;
}

bool elf_section_get_all(elf_info_t *elf_info)
{
    Elf32_Shdr section_strtab;
    if(!elf_section_header_read(elf_info, elf_info->header.e_shstrndx, &section_strtab)) {
        fprintf(stderr, "Failed to read section string table header\n");
        return false;
    }
    elf_info->section_strtab = malloc(section_strtab.sh_size);
    if(!read_checked(elf_info->file, section_strtab.sh_offset, elf_info->section_strtab, section_strtab.sh_size)) {
        fprintf(stderr, "Failed to read section string table data\n");
        return false;
    }
    for(uint16_t i=0; i<elf_info->header.e_shnum; i++) {
        //Read and push section
        elf_section_t section;
        Elf32_Shdr elf_section;
        if(!elf_section_header_read(elf_info, i, &elf_section)) {
            fprintf(stderr, "Failed to read ELF section %d\n", i);
            return false;
        }
        section.name = elf_info->section_strtab+elf_section.sh_name;
        section.type = elf_section.sh_type;
        section.flags = elf_section.sh_flags;
        section.addr = elf_section.sh_addr;
        section.offset = elf_section.sh_offset;
        section.size = elf_section.sh_size;
        section.link = elf_section.sh_link;
        section.info = elf_section.sh_info;
        section.align = elf_section.sh_addralign;
        arrpush(elf_info->sections, section);
    }
    return true;
}

void elf_section_collect_uso(elf_info_t *elf_info)
{
    //Insert null section into section list
    arrpush(elf_info->uso_src_sections, SHN_UNDEF);
    //Insert SHF_ALLOC sections into section list
    for(size_t i=0; i<arrlenu(elf_info->sections); i++) {
        if(elf_info->sections[i].flags & SHF_ALLOC) {
            arrpush(elf_info->uso_src_sections, i);
        }
    }
}

bool elf_section_map_uso(elf_info_t *elf_info, size_t elf_section_index, size_t *uso_section_idx)
{
    for(size_t i=0; i<arrlenu(elf_info->uso_src_sections); i++) {
        if(elf_info->uso_src_sections[i] == elf_section_index) {
            *uso_section_idx = i;
            return true;
        }
    }
    return false;
}

bool elf_section_search_uso(elf_info_t *elf_info, char *name, size_t *uso_section_idx)
{
    for(size_t i=0; i<arrlenu(elf_info->uso_src_sections); i++) {
        if(!strcmp(elf_info->sections[elf_info->uso_src_sections[i]].name, name)) {
            *uso_section_idx = i;
            return true;
        }
    }
    return false;
}

bool elf_sym_read(FILE *file, elf_section_t *symtab_section, size_t sym_index, Elf32_Sym *sym)
{
    size_t sym_section_offset = sym_index*sizeof(Elf32_Sym);
    //Warn if invalid symbol is read
    if(sym_section_offset > symtab_section->size) {
        fprintf(stderr, "Trying to read invalid symbol %ld\n", sym_index);
        return false;
    }
    //Read ELF symbol
    if(!read_checked(file, symtab_section->offset+sym_section_offset, sym, sizeof(Elf32_Sym))) {
        fprintf(stderr, "Failed to read symbol %ld\n", sym_index);
        return false;
    }
    //Byteswap ELF symbol
    bswap32(&sym->st_name);
    bswap32(&sym->st_value);
    bswap32(&sym->st_size);
    bswap16(&sym->st_shndx);
    return true;
}

bool elf_sym_get_all(elf_info_t *elf_info)
{
    //Find the symbol table section
    elf_section_t *symtab_section = NULL;
    elf_section_t *strtab_section = NULL;
    for(size_t i=0; i<arrlenu(elf_info->sections); i++) {
        if(elf_info->sections[i].type == SHT_SYMTAB) {
            assert(!symtab_section);
            symtab_section = &elf_info->sections[i];
        }
    }
    //Error out if not found
    if(!symtab_section) {
        fprintf(stderr, "No symbol table present\n");
        return false;
    }
    //Read string table
    strtab_section = &elf_info->sections[symtab_section->link];
    elf_info->strtab = calloc(1, strtab_section->size);
    if(!read_checked(elf_info->file, strtab_section->offset, elf_info->strtab, strtab_section->size)) {
        fprintf(stderr, "Failed to read string table\n");
        return false;
    }
    //Add symbols in the section
    for(size_t i=0; i<symtab_section->size/sizeof(Elf32_Sym); i++) {
        elf_symbol_t sym;
        Elf32_Sym elf_sym;
        if(!elf_sym_read(elf_info->file, symtab_section, i, &elf_sym)) {
            return false;
        }
        if(elf_sym.st_shndx == SHN_COMMON) {
            fprintf(stderr, "Found common section symbol %s.\n", elf_info->strtab+elf_sym.st_name);
            fprintf(stderr, "Compile with -fno-common, link with -d," 
                "or add FORCE_COMMON_ALLOCATION to the linker script to fix.\n");
            return false;
        }
        //Populate and push custom ELF symbol struct
        sym.name = elf_info->strtab+elf_sym.st_name;
        sym.value = elf_sym.st_value;
        sym.size = elf_sym.st_size;
        sym.info = elf_sym.st_info;
        sym.other = elf_sym.st_other;
        sym.section = elf_sym.st_shndx;
        arrpush(elf_info->syms, sym);
    }
    return true;
}

int elf_sym_compare(const void *a, const void *b)
{
    //Sort in lexicographical order (standard strcmp uses)
    elf_symbol_t *symbol_1 = *(elf_symbol_t **)a;
    elf_symbol_t *symbol_2 = *(elf_symbol_t **)b;
    return strcmp(symbol_1->name, symbol_2->name);
}

void elf_sym_collect_uso(elf_info_t *elf_info)
{
    for(size_t i=0; i<arrlenu(elf_info->syms); i++) {
        elf_symbol_t *sym = &elf_info->syms[i];
        unsigned char bind = ELF32_ST_BIND(sym->info);
        unsigned char visibility = ELF32_ST_VISIBILITY(sym->other);
        //Do not add local symbols to either list
        if(bind == STB_LOCAL) {
            continue;
        }
        if(sym->section == SHN_UNDEF) {
            //Add external (section of SHN_UNDEF(0)) symbol
            arrpush(elf_info->uso_ext_syms, sym);
        } else {
            //Only add default visibility symbols to export
            //But also export __dso_handle
            if(!strcmp(sym->name, "__dso_handle") || visibility == STV_DEFAULT) {
                arrpush(elf_info->uso_syms, sym);
            }
        }
    }
}

bool elf_sym_map_uso(elf_info_t *elf_info, size_t elf_sym_index, size_t *uso_symbol_idx, bool external)
{
    elf_symbol_t **uso_sym_list;
    if(external) {
        uso_sym_list = elf_info->uso_ext_syms;
    } else {
        uso_sym_list = elf_info->uso_syms;
    }
    //Read symbol list
    for(size_t i=0; i<arrlenu(uso_sym_list); i++) {
        //Check index in symbol list
        if(uso_sym_list[i]-elf_info->syms == elf_sym_index) {
            //Push symbol index
            *uso_symbol_idx = i;
            return true;
        }
    }
    return false;
}

void elf_uso_sym_sort(elf_info_t *elf_info)
{
    //Sort both tables of USO symbols
    qsort(elf_info->uso_syms, arrlenu(elf_info->uso_syms), sizeof(elf_symbol_t *), elf_sym_compare);
    qsort(elf_info->uso_ext_syms, arrlenu(elf_info->uso_ext_syms), sizeof(elf_symbol_t *), elf_sym_compare);
}

bool elf_reloc_read(FILE *file, elf_section_t *reloc_section, uint32_t reloc_index, Elf32_Rel *reloc)
{
    uint32_t offset = reloc_index*sizeof(Elf32_Rel);
    //Warn if invalid symbol is read
    if(offset > reloc_section->size) {
        fprintf(stderr, "Trying to read invalid relocation %d\n", reloc_index);
        return false;
    }
    //Read ELF symbol
    if(!read_checked(file, reloc_section->offset+offset, reloc, sizeof(Elf32_Rel))) {
        fprintf(stderr, "Failed to read relocation %d\n", reloc_index);
        return false;
    }
    //Byteswap relocation fields
    bswap32(&reloc->r_offset);
    bswap32(&reloc->r_info);
    return true;
}

bool elf_reloc_check_gp_relative(Elf32_Rel *reloc)
{
    uint8_t reloc_type = ELF32_R_TYPE(reloc->r_info);
    return reloc_type == R_MIPS_GPREL16 //Small data accesses
        || reloc_type == R_MIPS_GOT16 //Global offset table entry offset
        || reloc_type == R_MIPS_CALL16 //Global offset table function entry offset
        || reloc_type == R_MIPS_GPREL32 //32-bit GP-Relative accesses
        || reloc_type == R_MIPS_GOT_DISP //Global offset table displacement
        || reloc_type == R_MIPS_GOT_PAGE //Global offset table page
        || reloc_type == R_MIPS_GOT_OFST //Global offset table offset
        || reloc_type == R_MIPS_GOT_HI16 //Global offset table entry offset high 16 bits
        || reloc_type == R_MIPS_GOT_LO16 //Global offset table entry offset low 16 bits
        || reloc_type == R_MIPS_CALL_HI16 //GP-Relative call high 16 bits
        || reloc_type == R_MIPS_CALL_LO16; //GP-Relative call low 16 bits
}

uso_module_t *uso_module_alloc()
{
    uso_module_t *module = calloc(1, sizeof(uso_module_t));
    module->magic = USO_HEADER_MAGIC; //Add magic
    return module;
}

void uso_module_free(uso_module_t *module)
{
    //Free sections
    for(uint16_t i=0; i<module->num_sections; i++) {
        uso_section_t *section = &module->sections[i];
        free(section->data); //Free section data
        //Free relocations
        free(section->relocs.data);
        free(section->ext_relocs.data);
    }
    free(module->sections); //Free section array
    //Free symbol tables
    free(module->syms.data);
    free(module->ext_syms.data);
    //Free module itself
    free(module);
}

void uso_reloc_table_insert(uso_reloc_table_t *reloc_table, uso_reloc_t *reloc)
{
    //Add relocation onto end of extended relocation table
    reloc_table->size++;
    reloc_table->data = realloc(reloc_table->data, reloc_table->size*sizeof(uso_reloc_t));
    reloc_table->data[reloc_table->size-1] = *reloc;
}

bool uso_section_build_relocs(uso_section_t *section, elf_info_t *elf_info, elf_section_t *reloc_section)
{
    for(uint32_t i=0; i<reloc_section->size/sizeof(Elf32_Rel); i++) {
        uso_reloc_table_t *reloc_table;
        Elf32_Rel entry;
        uso_reloc_t reloc;
        Elf32_Section sym_section;
        //Read relocation
        if(!elf_reloc_read(elf_info->file, reloc_section, i, &entry)) {
            fprintf(stderr, "Failed to read relocation entry %d\n", i);
            return false;
        }
        reloc.offset = entry.r_offset; //Write relocation offset
        //Throw error if relocation is GP-relative
        if(elf_reloc_check_gp_relative(&entry)) {
            fprintf(stderr, "GP-Relative relocations present in ELF\n");
            fprintf(stderr, "Compile with -mno-gpopt (not -G 0) and without "
                "-fPIC, -fpic, -mshared, or -mabicalls to fix\n");
            return false;
        }
        reloc.info = ELF32_R_TYPE(entry.r_info) << 24;
        sym_section = elf_info->syms[ELF32_R_SYM(entry.r_info)].section;
        if(sym_section == SHN_UNDEF) {
            //Initialize external relocation
            size_t symbol_idx = ELF32_R_SYM(entry.r_info);
            elf_sym_map_uso(elf_info, ELF32_R_SYM(entry.r_info), &symbol_idx, true);
            reloc.info |= (symbol_idx & 0xFFFFFF); //Add symbol index to external relocation
            reloc.sym_value = 0; //External relocations have symbol value of 0
            reloc_table = &section->ext_relocs; 
        } else {
            //Initialize resolved relocation
            size_t reloc_sym_section;
            if(!elf_section_map_uso(elf_info, sym_section, &reloc_sym_section)) {
                //Map failed accesses to section 0 (absolute section)
                verbose("Remapping access to section %d to absolute access.\n", sym_section);
                reloc_sym_section = 0;
            }
            reloc.info |= (reloc_sym_section & 0xFFFFFF); //Add section index to external relocation
            reloc.sym_value = elf_info->syms[ELF32_R_SYM(entry.r_info)].value; //Set relocation symbol value
            reloc_table = &section->relocs;
        }
        //Add entry to relevant relocation table
        uso_reloc_table_insert(reloc_table, &reloc);
    }
    return true;
}

bool uso_section_build(uso_section_t *section, elf_info_t *elf_info, size_t uso_section)
{
    elf_section_t *reloc_elf_section = NULL;
    Elf32_Section elf_section_index = elf_info->uso_src_sections[uso_section];
    //Search for ELF relocation section targeting mapped section index
    for(Elf32_Section i=elf_section_index; i<arrlenu(elf_info->sections); i++) {
        if(elf_info->sections[i].type == SHT_REL && elf_info->sections[i].info == elf_section_index) {
            reloc_elf_section = &elf_info->sections[i];
            break;
        }
    }
    //Mark relocation tables as being empty
    section->relocs.size = 0;
    section->relocs.data = NULL;
    section->ext_relocs.size = 0;
    section->ext_relocs.data = NULL;
    if(reloc_elf_section) {
        //Add relocations if relevant ELF section is found
        if(!uso_section_build_relocs(section, elf_info, reloc_elf_section)) {
            return false;
        }
    }
    if(elf_info->sections[elf_section_index].flags & SHF_ALLOC) {
        section->size = elf_info->sections[elf_section_index].size;
        section->align = elf_info->sections[elf_section_index].align;
        //Force minimum alignment to 1 for SHF_ALLOC sections
        if(section->align == 0) {
            section->align = 1;
        }
        //Allocate and read data for non-nobits sections
        if(elf_info->sections[elf_section_index].type != SHT_NOBITS) {
            section->data = malloc(section->size);
            //Read section data if not 0-sized
            if(section->size != 0
            && !read_checked(elf_info->file, elf_info->sections[elf_section_index].offset, section->data, section->size)) {
                fprintf(stderr, "Failed to read section data\n");
                return false;
            }
        } else {
            //Force data pointer to null if SHT_NOBITS
            section->data = NULL;
        }
    } else {
        //Mark section as being dropped
        section->size = 0;
        section->align = 0;
        section->data = NULL;
    }
    return true;
}

void uso_sym_table_insert(uso_sym_table_t *sym_table, uso_sym_t *symbol)
{
    //Push symbol to end of symbol table
    sym_table->size++;
    sym_table->data = realloc(sym_table->data, sym_table->size*sizeof(uso_sym_t));
    sym_table->data[sym_table->size-1] = *symbol;
}

void uso_sym_table_build(elf_info_t *elf_info, uso_sym_table_t *sym_table, bool external)
{
    elf_symbol_t **elf_symbols;
    if(external) {
        elf_symbols = elf_info->uso_ext_syms;
    } else {
        elf_symbols = elf_info->uso_syms;
    }
    for(size_t i=0; i<arrlenu(elf_symbols); i++) {
        uso_sym_t symbol;
        
        //Copy over symbol properies
        symbol.name = elf_symbols[i]->name;
        if(external) {
            //External symbols have 0 value and 0 section
            symbol.value = 0;
            symbol.info = 0;
        } else {
            size_t uso_section_idx = 0;
            symbol.value = elf_symbols[i]->value; //Copy symbol value
            //Convert ELF section to USO section
            elf_section_map_uso(elf_info, elf_symbols[i]->section, &uso_section_idx);
            symbol.info = ((uso_section_idx & 0xFF) << 24);
        }
        //Mark symbol as weak
        if(ELF32_ST_BIND(elf_symbols[i]->info) == STB_WEAK) {
            symbol.info |= 0x800000;
        }
        //Add symbol size
        symbol.info |= elf_symbols[i]->size & 0x7FFFFF;
        //Insert symbol
        uso_sym_table_insert(sym_table, &symbol);
    }
}

void uso_module_insert_section(uso_module_t *module, uso_section_t *section)
{
    //Push section at end of sections list
    module->num_sections++;
    module->sections = realloc(module->sections, module->num_sections*sizeof(uso_section_t));
    module->sections[module->num_sections-1] = *section;
}

void uso_module_set_section_id(elf_info_t *elf_info, char *name, uint8_t *dst)
{
    size_t section_id = 0;
    //Search for section IDs
    if(!elf_section_search_uso(elf_info, name, &section_id)) {
        //Map not found section to section 0
        verbose("Section %s is not in USO module\n", name);
        section_id = 0;
    }
    //Write found section ID to destination
    *dst = section_id;
}

bool uso_module_build(uso_module_t *module, elf_info_t *elf_info)
{
    //Build section table
    for(size_t i=0; i<arrlenu(elf_info->uso_src_sections); i++) {
        uso_section_t temp_section;
        if(!uso_section_build(&temp_section, elf_info, i)) {
            return false;
        }
        uso_module_insert_section(module, &temp_section);
    }
    //Build symbol tables
    uso_sym_table_build(elf_info, &module->syms, false);
    uso_sym_table_build(elf_info, &module->ext_syms, true);
    //Set USO section IDs
    uso_module_set_section_id(elf_info, ".eh_frame", &module->eh_frame_section);
    uso_module_set_section_id(elf_info, ".ctors", &module->ctors_section);
    uso_module_set_section_id(elf_info, ".dtors", &module->dtors_section);
    //Set text section ID
    for(size_t i=0; i<arrlenu(elf_info->uso_src_sections); i++) {
        if(elf_info->sections[elf_info->uso_src_sections[i]].flags & SHF_EXECINSTR) {
            if(module->text_section != 0) {
                fprintf(stderr, "Found multiple executable sections in input ELF\n");
                return false;
            }
            module->text_section = i;
        }
    }
    return true;
}

uso_file_sym_t uso_generate_file_sym(uso_sym_t *sym)
{
    uso_file_sym_t temp;
    temp.name_ofs = 0; //Placeholder
    temp.value = sym->value;
    temp.info = sym->info;
    return temp;
}

uso_file_sym_table_t uso_generate_file_sym_table(uso_sym_table_t *sym_table)
{
    uso_file_sym_table_t temp;
    temp.size = sym_table->size;
    temp.data_ofs = 0; //Placeholder
    return temp;
}

uso_file_module_t uso_generate_file_module(uso_module_t *module)
{
    uso_file_module_t temp;
    temp.magic = module->magic;
    temp.sections_ofs = 0; //Placeholder
    temp.syms = uso_generate_file_sym_table(&module->syms);
    temp.ext_syms = uso_generate_file_sym_table(&module->ext_syms);
    temp.num_sections = module->num_sections;
    temp.eh_frame_section = module->eh_frame_section;
    temp.ctors_section = module->ctors_section;
    temp.dtors_section = module->dtors_section;
    temp.text_section = module->text_section;
    temp.__padding[0] = module->__padding[0];
    temp.__padding[1] = module->__padding[1];
    temp.__padding[2] = module->__padding[2];
    return temp;
}

uso_file_reloc_table_t uso_generate_file_reloc_table(uso_reloc_table_t *reloc_table)
{
    uso_file_reloc_table_t temp;
    temp.size = reloc_table->size;
    temp.data_ofs = 0; //Placeholder
    return temp;
}

uso_file_section_t uso_generate_file_section(uso_section_t *section)
{
    uso_file_section_t temp;
    temp.data_ofs = 0; //Placeholder
    temp.size = section->size;
    temp.align = section->align;
    temp.relocs = uso_generate_file_reloc_table(&section->relocs);
    temp.ext_relocs = uso_generate_file_reloc_table(&section->ext_relocs);
    return temp;
}

void uso_write_reloc_list(uso_reloc_t *relocs, uint32_t num_relocs, uint32_t offset, FILE *out)
{
    fseek(out, offset, SEEK_SET);
    for(uint32_t i=0; i<num_relocs; i++) {
        w32(out, relocs[i].offset);
        w32(out, relocs[i].info);
        w32(out, relocs[i].sym_value);
    }
}

void uso_write_file_reloc_table(uso_file_reloc_table_t *reloc_table, uint32_t offset, FILE *out)
{
    fseek(out, offset, SEEK_SET);
    w32(out, reloc_table->size);
    w32(out, reloc_table->data_ofs);
}

void uso_write_file_section(uso_file_section_t *file_section, uint32_t offset, FILE *out)
{
    fseek(out, offset, SEEK_SET);
    w32(out, file_section->data_ofs);
    w32(out, file_section->size);
    w32(out, file_section->align);
    uso_write_file_reloc_table(&file_section->relocs, offset+offsetof(uso_file_section_t, relocs), out);
    uso_write_file_reloc_table(&file_section->ext_relocs, offset+offsetof(uso_file_section_t, ext_relocs), out);
}

void uso_write_file_sym(uso_file_sym_t *file_sym, uint32_t offset, FILE *out)
{
    fseek(out, offset, SEEK_SET);
    w32(out, file_sym->name_ofs);
    w32(out, file_sym->value);
    w32(out, file_sym->info);
}

void uso_write_file_sym_table(uso_file_sym_table_t *file_sym_table, uint32_t offset, FILE *out)
{
    fseek(out, offset, SEEK_SET);
    w32(out, file_sym_table->size);
    w32(out, file_sym_table->data_ofs);
}

void uso_write_file_module(uso_file_module_t *file_module, uint32_t offset, FILE *out)
{
    fseek(out, offset, SEEK_SET);
    w32(out, file_module->magic);
    w32(out, file_module->sections_ofs);
    uso_write_file_sym_table(&file_module->syms, offset+offsetof(uso_file_module_t, syms), out);
    uso_write_file_sym_table(&file_module->ext_syms, offset+offsetof(uso_file_module_t, ext_syms), out);
    w8(out, file_module->num_sections);
    w8(out, file_module->eh_frame_section);
    w8(out, file_module->ctors_section);
    w8(out, file_module->dtors_section);
    w8(out, file_module->text_section);
    w8(out, file_module->__padding[0]);
    w8(out, file_module->__padding[1]);
    w8(out, file_module->__padding[2]);
}

uint32_t uso_write_syms(uso_sym_t *sym_list, uint32_t num_syms, uint32_t offset, FILE *out)
{
    uint32_t name_ofs = num_syms*sizeof(uso_file_sym_t);
    for(uint32_t i=0; i<num_syms; i++) {
        uso_file_sym_t file_sym = uso_generate_file_sym(&sym_list[i]);
        size_t name_len = strlen(sym_list[i].name);
        file_sym.name_ofs = name_ofs;
        uso_write_file_sym(&file_sym, offset+(i*sizeof(uso_file_sym_t)), out);
        //Write name and null terminator
        fseek(out, offset+name_ofs, SEEK_SET);
        fwrite(sym_list[i].name, name_len, 1, out);
        w8(out, 0);
        //Allocate room for next string
        name_ofs += name_len+1;
    }
    return offset+name_ofs;
}

uint32_t uso_write_sections(uso_section_t *sections, uint16_t num_sections, uint32_t file_ofs, FILE *out)
{
    uint32_t data_ofs = file_ofs+(num_sections*sizeof(uso_file_section_t));
    uint32_t data_end_ofs = data_ofs;
    uint32_t reloc_ofs;
    for(uint16_t i=0; i<num_sections; i++) {
        if(sections[i].data) {
            data_end_ofs = ROUND_UP(data_end_ofs, sections[i].align);
            data_end_ofs += sections[i].size;
        }
    }
    reloc_ofs = ROUND_UP(data_end_ofs, 4);
    for(uint16_t i=0; i<num_sections; i++) {
        uso_file_section_t file_section = uso_generate_file_section(&sections[i]);
        uint32_t section_ofs = file_ofs+(i*sizeof(uso_file_section_t));
        if(sections[i].data) {
            data_ofs = ROUND_UP(data_ofs, file_section.align);
            file_section.data_ofs = data_ofs;
            data_ofs += file_section.size;
        }
        if(file_section.relocs.size != 0) {
            file_section.relocs.data_ofs = reloc_ofs;
            reloc_ofs += file_section.relocs.size*sizeof(uso_reloc_t);
        }
        if(file_section.ext_relocs.size != 0) {
            file_section.ext_relocs.data_ofs = reloc_ofs;
            reloc_ofs += file_section.ext_relocs.size*sizeof(uso_reloc_t);
        }
        uso_write_file_section(&file_section, section_ofs, out);
        if(file_section.data_ofs != 0 && file_section.size != 0) {
            fseek(out, file_section.data_ofs, SEEK_SET);
            fwrite(sections[i].data, file_section.size, 1, out);
        }
        //Write section relocation tables
        uso_write_reloc_list(sections[i].relocs.data, file_section.relocs.size, file_section.relocs.data_ofs, out);
        uso_write_reloc_list(sections[i].ext_relocs.data, file_section.ext_relocs.size, file_section.ext_relocs.data_ofs, out);
    }
    return reloc_ofs;
}

void uso_write_load_info(uso_load_info_t *load_info, FILE *out)
{
    uint8_t *temp_buffer;
    size_t orig_size;
    //Get file size
    fseek(out, 0, SEEK_END);
    orig_size = ftell(out);
    fseek(out, 0, SEEK_SET);
    load_info->size = orig_size;
    //Copy rest of file to temporary buffer
    temp_buffer = malloc(load_info->size);
    fread(temp_buffer, load_info->size, 1, out);
    fseek(out, 0, SEEK_SET);
    //Write prepended load info
    w32(out, load_info->size);
    w32(out, load_info->noload_size);
    w16(out, load_info->align);
    w16(out, load_info->noload_align);
    //Write rest of file
    fwrite(temp_buffer, load_info->size, 1, out);
    free(temp_buffer);
}

void uso_init_module_load_info(uso_module_t *module, uso_load_info_t *load_info)
{
    load_info->size = 0; //Placeholder
    load_info->noload_size = 0;
    load_info->align = 4;
    load_info->noload_align = 1;
    //Calculate maximum alignments 
    for(uint16_t i=0; i<module->num_sections; i++) {
        uso_section_t *section = &module->sections[i];
        if(section->align != 0) {
            load_info->align = MAX(load_info->align, section->align);
            if(!section->data) {
                load_info->noload_align = MAX(load_info->noload_align, section->align);
                //Calculate position of next noload section
                load_info->noload_size = ROUND_UP(load_info->noload_size, section->align);
                load_info->noload_size += section->size;
            }
        }
    }
}

void uso_write_module(uso_module_t *module, FILE *out)
{
    uso_load_info_t load_info;
    uso_file_module_t file_module = uso_generate_file_module(module);
    file_module.sections_ofs = sizeof(uso_file_module_t);
    uso_write_file_module(&file_module, 0, out); //Write header
    //Write sections
    file_module.syms.data_ofs = uso_write_sections(module->sections, module->num_sections, file_module.sections_ofs, out);
    //Write symbols
    file_module.ext_syms.data_ofs = uso_write_syms(module->syms.data, module->syms.size, file_module.syms.data_ofs, out);
    file_module.ext_syms.data_ofs = ROUND_UP(file_module.ext_syms.data_ofs, 4);
    uso_write_syms(module->ext_syms.data, module->ext_syms.size, file_module.ext_syms.data_ofs, out);
    file_module.syms.data_ofs -= offsetof(uso_file_module_t, syms);
    file_module.ext_syms.data_ofs -= offsetof(uso_file_module_t, ext_syms);
    uso_write_file_module(&file_module, 0, out); //Update header
    //Write load info
    uso_init_module_load_info(module, &load_info);
    uso_write_load_info(&load_info, out);
}

bool convert(char *infn, char *outfn)
{
    bool ret = false;
    FILE *out_file;
    elf_info_t *elf_info = elf_info_init(infn);
    uso_module_t *module = NULL;
    //Try opening ELF file
    if(!elf_info->file) {
        fprintf(stderr, "Error: cannot open file: %s\n", infn);
        goto end1;
    }
    //Parse ELF file
    verbose("Parsing ELF file\n");
    if(!elf_header_read(elf_info)) {
        goto end1;
    }
    verbose("Reading ELF sections\n");
    if(!elf_section_get_all(elf_info)) {
        goto end1;
    }
    verbose("Reading ELF symbols\n");
    if(!elf_sym_get_all(elf_info)) {
        goto end1;
    }
    //Collect sections and symbols for USO
    verbose("Collecting ELF sections to use in USO\n");
    elf_section_collect_uso(elf_info);
    //Check if more than 255 sections were collected
    if(arrlenu(elf_info->uso_src_sections) > USO_MAX_SECTIONS) {
        fprintf(stderr, "Collected %ld sections in USO\n", arrlenu(elf_info->uso_src_sections));
        fprintf(stderr, "Expected no more than %d sections\n", USO_MAX_SECTIONS);
        goto end2;
    }
    verbose("Collecting ELF symbols to use in USO\n");
    elf_sym_collect_uso(elf_info);
    //Sort symbols in lexicographical gorder
    verbose("Sorting collected symbols\n");
    elf_uso_sym_sort(elf_info);
    //Build USO module
    module = uso_module_alloc();
    verbose("Building USO module\n");
    if(!uso_module_build(module, elf_info)) {
        goto end2;
    }
    //Write USO module
    verbose("Writing USO module\n");
    out_file = fopen(outfn, "w+b");
    if(!out_file) {
        fprintf(stderr, "cannot open output file: %s\n", outfn);
        goto end2;
    }
    uso_write_module(module, out_file);
    verbose("Successfully converted input to USO\n");
    ret = true; //Mark as having succeeded
    //Cleanup code
    fclose(out_file);
    end2:
    uso_module_free(module);
    end1:
    elf_info_free(elf_info);
    return ret;
}

int main(int argc, char *argv[])
{
    bool compression = false;
    char *outdir = ".";
    if(argc < 2) {
        //Print usage if too few arguments are passed
        print_args(argv[0]);
        return 1;
    }
    for(int i=1; i<argc; i++) {
        char *infn;
        char *outfn;
        if(argv[i][0] == '-') {
            //Option detected
            if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                //Print help
                print_args(argv[0]);
                return 0;
            } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
                //Mark output as verbose
                verbose_flag = true;
            } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
                //Set output directory in next argument
                if(++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                outdir = argv[i];
            } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--compress")) {
                //Set up for compression
                compression = true;
            } else {
                //Complain about invalid flag
                fprintf(stderr, "invalid flag: %s\n", argv[i]);
                return 1;
            }
            continue;
        }
        infn = argv[i];
        //Generate output filename
        char *basename = strrchr(infn, '/');
        if (!basename) basename = infn; else basename += 1;
        char* basename_noext = strdup(basename);
        char* ext = strrchr(basename_noext, '.');
        if (ext) *ext = '\0';
        asprintf(&outfn, "%s/%s.uso", outdir, basename_noext);
        //Convert input to output
        verbose("Converting: %s -> %s\n", infn, outfn);
        if(!convert(infn, outfn)) {
            return 1;
        }
        if(compression) {
            //Compress this file
            struct stat st_decomp = {0}, st_comp = {0};
            stat(outfn, &st_decomp);
            asset_compress(outfn, outfn, DEFAULT_COMPRESSION);
            stat(outfn, &st_comp);
            if (verbose_flag)
                printf("compressed: %s (%d -> %d, ratio %.1f%%)\n", outfn,
                (int)st_decomp.st_size, (int)st_comp.st_size, 100.0 * (float)st_comp.st_size / (float)(st_decomp.st_size == 0 ? 1 :st_decomp.st_size));
        }
        free(outfn);
    }
    return 0;
}
