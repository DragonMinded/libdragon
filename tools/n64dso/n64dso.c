#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include "../common/binout.c"
#include "../common/binout.h"

// Compression library
#include "../common/assetcomp.h"

//Macros copied from utils.h in libdragon src directory
#define ROUND_UP(n, d) ({ \
    typeof(n) _n = n; typeof(d) _d = d; \
    (((_n) + (_d) - 1) / (_d) * (_d)); \
})
#define MAX(a,b)  ({ typeof(a) _a = a; typeof(b) _b = b; _a > _b ? _a : _b; })

//ELF structs
#include "../common/mips_elf.h"

typedef struct elf_symbol_s {
    char *name;
    Elf32_Addr value;
    Elf32_Word size;
    unsigned char info;
    unsigned char other;
    Elf32_Section section;
} elf_symbol_t;

typedef struct elf_load_seg_s {
    void *data;
    Elf32_Off offset;
    Elf32_Word mem_size;
    Elf32_Word file_size;
    Elf32_Word align;
} elf_load_seg_t;

typedef struct elf_info_s {
    char *filename;
    FILE *file;
    Elf32_Ehdr header;
    elf_symbol_t *syms;
    elf_symbol_t **import_syms;
    elf_symbol_t **export_syms;
    elf_load_seg_t load_seg;
    char *strtab;
} elf_info_t;

//DSO Format Internals
#include "../../src/dso_format.h"

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
    fprintf(stderr, "   -c/--compress <level> Compress output files (default: %d)\n", DEFAULT_COMPRESSION);
    fprintf(stderr, "\n");
}

elf_info_t *elf_info_init(const char *filename)
{
    elf_info_t *elf_info = calloc(1, sizeof(elf_info_t));
    elf_info->file = fopen(filename, "rb");
    elf_info->filename = strdup(filename);
    return elf_info;
}

void elf_info_free(elf_info_t *elf_info)
{
    //Close attached file
    if(elf_info->file) {
        fclose(elf_info->file);
    }
    free(elf_info->filename);
    //Free symbol arrays
    stbds_arrfree(elf_info->import_syms);
    stbds_arrfree(elf_info->export_syms);
    stbds_arrfree(elf_info->syms);
    free(elf_info->load_seg.data);
    free(elf_info->strtab); //Free string table
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
    if(elf_info->header.e_type != ET_EXEC || elf_info->header.e_machine != EM_MIPS) {
        fprintf(stderr, "ELF is not a valid MIPS executable file\n");
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

bool elf_program_header_read(elf_info_t *elf_info, Elf32_Half index, Elf32_Phdr *phdr)
{
    size_t section_offset = elf_info->header.e_phoff+(index*elf_info->header.e_phentsize);
    //Warn if invalid program header is read
    if(index >= elf_info->header.e_phnum) {
        fprintf(stderr, "Trying to read invalid program header %d\n", index);
        return false;
    }
    //Read program header
    if(!read_checked(elf_info->file, section_offset, phdr, sizeof(Elf32_Phdr))) {
        fprintf(stderr, "Failed to read ELF program header %d\n", index);
        return false;
    }
    //Byteswap program header
    bswap32(&phdr->p_type);
    bswap32(&phdr->p_offset);
    bswap32(&phdr->p_vaddr);
    bswap32(&phdr->p_paddr);
    bswap32(&phdr->p_filesz);
    bswap32(&phdr->p_memsz);
    bswap32(&phdr->p_flags);
    bswap32(&phdr->p_align);
    return true;
}

bool elf_get_load_seg(elf_info_t *elf_info)
{
    Elf32_Half num_load_segs = 0;
    //Search for loadable segments
    for(Elf32_Half i=0; i<elf_info->header.e_phnum; i++) {
        Elf32_Phdr phdr;
        if(!elf_program_header_read(elf_info, i, &phdr)) {
            return false;
        }
        if(phdr.p_type == PT_LOAD) {
            //Found loadable segment
            if(num_load_segs > 0) {
                //Report error if this is not first loadable segment
                fprintf(stderr, "ELF has multiple loadable segments\n");
                return false;
            }
            //Report info about loadable segment
            elf_info->load_seg.offset = phdr.p_offset;
            elf_info->load_seg.mem_size = phdr.p_memsz;
            elf_info->load_seg.file_size = phdr.p_filesz;
            elf_info->load_seg.align = phdr.p_align;
            elf_info->load_seg.data = calloc(1, phdr.p_memsz);
            num_load_segs++;
            //Read loaded segment
            if(!read_checked(elf_info->file, phdr.p_offset, elf_info->load_seg.data, phdr.p_filesz)) {
                //Report error if this is not first loadable segment
                fprintf(stderr, "Failed to read loadable segment\n");
                return false;
            }
        }
    }
    if(elf_info->load_seg.align > 32) {
        fprintf(stderr, "Program header alignment too large\n");
        return false;
    }
    //Report error if ELF has no loadable segments
    if(num_load_segs == 0) {
        fprintf(stderr, "ELF has no loadable segments\n");
        return false;
    }
    return true;
}

bool elf_section_header_read(elf_info_t *elf_info, Elf32_Half index, Elf32_Shdr *shdr)
{
    size_t section_offset = elf_info->header.e_shoff+(index*elf_info->header.e_shentsize);
    //Warn if invalid section header is read
    if(index >= elf_info->header.e_shnum) {
        fprintf(stderr, "Trying to read invalid section header %d\n", index);
        return false;
    }
    //Read section header
    if(!read_checked(elf_info->file, section_offset, shdr, sizeof(Elf32_Shdr))) {
        fprintf(stderr, "Failed to read ELF section header %d\n", index);
        return false;
    }
    //Byteswap section header
    bswap32(&shdr->sh_name);
    bswap32(&shdr->sh_type);
    bswap32(&shdr->sh_flags);
    bswap32(&shdr->sh_addr);
    bswap32(&shdr->sh_offset);
    bswap32(&shdr->sh_size);
    bswap32(&shdr->sh_link);
    bswap32(&shdr->sh_info);
    bswap32(&shdr->sh_addralign);
    bswap32(&shdr->sh_entsize);
    return true;
}

bool elf_section_fully_inside_prog(elf_info_t *elf_info, Elf32_Shdr *shdr)
{
    Elf32_Off prog_min, prog_max;
    Elf32_Off section_min, section_max;
    //Get section range
    section_min = shdr->sh_offset;
    section_max = section_min+shdr->sh_size;
    //Get program range
    prog_min = elf_info->load_seg.offset;
    prog_max = prog_min+elf_info->load_seg.mem_size;
    if(section_min < prog_min || section_max > prog_max) {
        //Section is at least partially outside program
        return false;
    }
    return true;
}

bool elf_sym_read(FILE *file, Elf32_Shdr *symtab_section, size_t sym_index, Elf32_Sym *sym)
{
    size_t sym_section_offset = sym_index*sizeof(Elf32_Sym);
    //Warn if invalid symbol is read
    if(sym_section_offset > symtab_section->sh_size) {
        fprintf(stderr, "Trying to read invalid symbol %zu\n", sym_index);
        return false;
    }
    //Read ELF symbol
    if(!read_checked(file, symtab_section->sh_offset+sym_section_offset, sym, sizeof(Elf32_Sym))) {
        fprintf(stderr, "Failed to read symbol %zu\n", sym_index);
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
    //Search for SHT_SYMTAB sections
    for(Elf32_Half i=0; i<elf_info->header.e_shnum; i++) {
        Elf32_Shdr shdr;
        if(!elf_section_header_read(elf_info, i, &shdr)) {
            return false;
        }
        if(shdr.sh_type == SHT_SYMTAB) {
            //Found SHT_SYMTAB section
            Elf32_Shdr strtab_shdr;
            if(elf_info->syms) {
                //Report error if this is not first SHT_SYMTAB section
                fprintf(stderr, "Multiple symbol tables present\n");
                return false;
            }
            //Read associated string table
            if(!elf_section_header_read(elf_info, shdr.sh_link, &strtab_shdr)) {
                fprintf(stderr, "Failed to read associated string table\n");
                return false;
            }
            elf_info->strtab =  malloc(strtab_shdr.sh_size);
            if(!read_checked(elf_info->file, strtab_shdr.sh_offset, elf_info->strtab, strtab_shdr.sh_size)) {
                fprintf(stderr, "Failed to read associated string table\n");
                return false;
            }
            //Process all symbols
            for(size_t j=0; j<shdr.sh_size/sizeof(Elf32_Sym); j++) {
                elf_symbol_t sym;
                //Read ELF symbol
                Elf32_Sym elf_sym;
                if(!elf_sym_read(elf_info->file, &shdr, j, &elf_sym)) {
                    return false;
                }
                //Convert ELF symbol
                sym.name = elf_sym.st_name+elf_info->strtab;
                sym.value = elf_sym.st_value;
                sym.size = elf_sym.st_size;
                sym.info = elf_sym.st_info;
                sym.other = elf_sym.st_other;
                sym.section = elf_sym.st_shndx;
                stbds_arrpush(elf_info->syms, sym);
            }
        }
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

void elf_sym_collect(elf_info_t *elf_info)
{
    for(size_t i=0; i<stbds_arrlenu(elf_info->syms); i++) {
        //Skip local symbols
        if(ELF32_ST_BIND(elf_info->syms[i].info) == STB_LOCAL) {
            continue;
        }
        if(elf_info->syms[i].section == SHN_UNDEF) {
            //Push to import symbols list
            stbds_arrpush(elf_info->import_syms, &elf_info->syms[i]);
        } else {
            //Push to export symbol list if visible
            if(ELF32_ST_VISIBILITY(elf_info->syms[i].other) == STV_DEFAULT) {
                stbds_arrpush(elf_info->export_syms, &elf_info->syms[i]);
            }
        }
    }
    //Sort collected symbols by name
    qsort(elf_info->export_syms, stbds_arrlenu(elf_info->export_syms), sizeof(elf_symbol_t *), elf_sym_compare);
    qsort(elf_info->import_syms, stbds_arrlenu(elf_info->import_syms), sizeof(elf_symbol_t *), elf_sym_compare);
}

bool elf_reloc_read(FILE *file, Elf32_Shdr *reloc_section, uint32_t reloc_index, Elf32_Rel *reloc)
{
    uint32_t offset = reloc_index*sizeof(Elf32_Rel);
    //Warn if invalid symbol is read
    if(offset > reloc_section->sh_size) {
        fprintf(stderr, "Trying to read invalid relocation %d\n", reloc_index);
        return false;
    }
    //Read ELF symbol
    if(!read_checked(file, reloc_section->sh_offset+offset, reloc, sizeof(Elf32_Rel))) {
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
        || reloc_type == R_MIPS_TLS_TPREL_HI16 //Thread local storage offset high 16 bits
        || reloc_type == R_MIPS_TLS_TPREL_LO16 //Thread local storage entry offset low 16 bits
        || reloc_type == R_MIPS_CALL_HI16 //GP-Relative call high 16 bits
        || reloc_type == R_MIPS_CALL_LO16; //GP-Relative call low 16 bits
}

dso_module_t *dso_module_alloc()
{
    return calloc(1, sizeof(dso_module_t));
}

void dso_module_free(dso_module_t *module)
{
    //Free buffers
    free(module->syms);
    free(module->relocs);
    //Free modules
    free(module);
}

void dso_build_symbols(dso_module_t *module, elf_info_t *elf_info)
{
    //Calculate symbol counts
    module->num_import_syms = stbds_arrlenu(elf_info->import_syms);
    module->num_syms = 1+module->num_import_syms+stbds_arrlenu(elf_info->export_syms);
    module->syms = malloc(module->num_syms*sizeof(dso_sym_t)); //Allocate symbols
    module->syms[0] = (dso_sym_t){ "", 0, 0 }; //Build dummy symbols
    //Build import symbols
    for(uint32_t i=0; i<module->num_import_syms; i++) {
        dso_sym_t sym;
        //Copy symbol properties
        sym.name = elf_info->import_syms[i]->name;
        sym.value = elf_info->import_syms[i]->value;
        sym.info = elf_info->import_syms[i]->size & 0x3FFFFFFF;
        //Mark symbol as weak if needed
        if(ELF32_ST_BIND(elf_info->import_syms[i]->info) == STB_WEAK) {
            sym.info |= 0x80000000;
        }
        module->syms[i+1] = sym; //Write new symbol
    }
    //Build export symbols
    for(uint32_t i=0; i<stbds_arrlenu(elf_info->export_syms); i++) {
        dso_sym_t sym;
        //Copy symbol properties
        sym.name = elf_info->export_syms[i]->name;
        sym.value = elf_info->export_syms[i]->value;
        sym.info = elf_info->export_syms[i]->size & 0x3FFFFFFF;
        //Mark symbol as absolute when in absolute section
        if(elf_info->export_syms[i]->section == SHN_ABS) {
            sym.info |= 0x40000000;
        }
        //Mark symbol as weak if needed
        if(ELF32_ST_BIND(elf_info->export_syms[i]->info) == STB_WEAK) {
            sym.info |= 0x80000000;
        }
        module->syms[i+module->num_import_syms+1] = sym; //Write new symbol
    }
}

void dso_push_relocation(dso_module_t *module, dso_reloc_t *reloc)
{
    //Allocate new relocation
    module->num_relocs++;
    module->relocs = realloc(module->relocs, sizeof(dso_reloc_t)*module->num_relocs);
    //Push relocation to end of list
    module->relocs[module->num_relocs-1] = *reloc;
}

uint32_t dso_translate_reloc_symbol_idx(elf_info_t *elf_info, uint32_t sym_idx)
{
    //Defined symbols always have index of 0
    if(elf_info->syms[sym_idx].section != SHN_UNDEF) {
        return 0;
    }
    //Search for symbol in import symbols
    elf_symbol_t search_sym;
    elf_symbol_t *search_sym_ptr = &search_sym;
    search_sym_ptr->name = elf_info->syms[sym_idx].name; //Set symbol name for search key
    //Do symbol search
    elf_symbol_t **result = bsearch(&search_sym_ptr, elf_info->import_syms, stbds_arrlenu(elf_info->import_syms), sizeof(elf_symbol_t *), elf_sym_compare);
    //Failed symbol searches assume symbol 0
    if(!result) {
        return 0;
    }
    //Convert result into array index
    return (result-elf_info->import_syms)+1;
}

bool dso_build_relocations(dso_module_t *module, elf_info_t *elf_info)
{
    for(Elf32_Half i=0; i<elf_info->header.e_shnum; i++) {
        Elf32_Shdr shdr;
        if(!elf_section_header_read(elf_info, i, &shdr)) {
            return false;
        }
        if(shdr.sh_type == SHT_REL) {
            //Read applied section
            Elf32_Shdr applied_shdr;
            if(!elf_section_header_read(elf_info, shdr.sh_info, &applied_shdr)) {
                return false;
            }
            //Include relocations applied to sections fully inside program
            if(elf_section_fully_inside_prog(elf_info, &applied_shdr)) {
                for(uint32_t j=0; j<shdr.sh_size/sizeof(Elf32_Rel); j++) {
                    //Read ELF relocation
                    Elf32_Rel elf_reloc;
                    if(!elf_reloc_read(elf_info->file, &shdr, j, &elf_reloc)) {
                        return false;
                    }
                    //Check if relocation is GP-relative
                    if(elf_reloc_check_gp_relative(&elf_reloc)) {
                        fprintf(stderr, "GP-Relative relocations present in ELF\n");
                        fprintf(stderr, "Compile with -mno-gpopt (not -G 0) and without "
                            "-fPIC, -fpic, -mshared, or -mabicalls to fix\n");
                        return false;
                    }
                    //Convert into DSO symbol index
                    uint32_t sym_index = dso_translate_reloc_symbol_idx(elf_info, ELF32_R_SYM(elf_reloc.r_info));
                    //Write DSO relocation
                    dso_reloc_t reloc;
                    reloc.offset = elf_reloc.r_offset; //Offset can be copied directly
                    reloc.info = (ELF32_R_TYPE(elf_reloc.r_info) << 24)|sym_index; //Merge in type with symbol index
                    dso_push_relocation(module, &reloc);
                }
            }
        }
    }
    return true;
}

bool dso_module_build(dso_module_t *module, elf_info_t *elf_info)
{
    module->src_elf = elf_info->filename;
    module->prog_base = elf_info->load_seg.data;
    module->prog_size = elf_info->load_seg.mem_size;
    dso_build_symbols(module, elf_info);
    return dso_build_relocations(module, elf_info);
}

void dso_write_relocs(dso_reloc_t *relocs, uint32_t num_relocs,  FILE *out_file)
{
    walign(out_file, 4);
    placeholder_set(out_file, "relocs");
    //Write relocation pairs
    for(uint32_t i=0; i<num_relocs; i++) {
        w32(out_file, relocs[i].offset);
        w32(out_file, relocs[i].info);
    }
}

void dso_write_symbols(dso_sym_t *syms, uint32_t num_syms, FILE *out_file)
{
    walign(out_file, 4);
    placeholder_set(out_file, "symbols");
    for(uint32_t i=0; i<num_syms; i++) {
        w32_placeholderf(out_file, "symbol%"PRIu32, i);
        w32(out_file, syms[i].value);
        w32(out_file, syms[i].info);
    }
    for(uint32_t i=0; i<num_syms; i++) {
        placeholder_set(out_file, "symbol%"PRIu32, i);
        fwrite(syms[i].name, 1, strlen(syms[i].name)+1, out_file);
    }
}

void dso_write_program(elf_info_t *elf_info, FILE *out_file)
{
    walign(out_file, elf_info->load_seg.align);
    placeholder_set(out_file, "program");
    fwrite(elf_info->load_seg.data, elf_info->load_seg.mem_size, 1, out_file);
}

void dso_write_header(dso_module_t *module, FILE *out_file)
{
    w32(out_file, DSO_MAGIC);
    w32(out_file, 0);
    w32(out_file, 0);
    w32(out_file, 0);
    w32_placeholderf(out_file, "src_elf_path");
    w32_placeholderf(out_file, "filename");
    w32_placeholderf(out_file, "symbols");
    w32(out_file, module->num_syms);
    w32(out_file, module->num_import_syms);
    w32_placeholderf(out_file, "relocs");
    w32(out_file, module->num_relocs);
    w32_placeholderf(out_file, "program");
    w32(out_file, module->prog_size);
    for(int i=0; i<6; i++) {
        w32(out_file, 0);
    }
    w32(out_file, 0);
    w32(out_file, 0);
}

void dso_write_elf_path(dso_module_t *module, FILE *out_file)
{
    placeholder_set(out_file, "src_elf_path");
    fwrite(module->src_elf, 1, strlen(module->src_elf)+1, out_file);
    
}

void dso_write_filename(FILE *out_file)
{
    placeholder_set(out_file, "filename");
    for(int i=0; i<256; i++) {
        w8(out_file, 0);
    }
}

void dso_write_module(dso_module_t *module, elf_info_t *elf_info, FILE *out_file)
{
    dso_write_header(module, out_file);
    dso_write_elf_path(module, out_file);
    dso_write_symbols(module->syms, module->num_syms, out_file);
    dso_write_relocs(module->relocs, module->num_relocs, out_file);
    dso_write_filename(out_file);
    dso_write_program(elf_info, out_file);
    placeholder_clear();
}

bool convert(char *infn, char *outfn)
{
    bool ret = false;
    FILE *out_file;
    elf_info_t *elf_info = elf_info_init(infn);
    dso_module_t *module = NULL;
    //Check if elf file is open
    if(!elf_info->file) {
        fprintf(stderr, "Error: cannot open file: %s\n", infn);
        goto end1;
    }
    //Parse ELF file
    verbose("Parsing ELF file\n");
    if(!elf_header_read(elf_info)) {
        goto end1;
    }
    //Find loadable program segment in ELF file
    verbose("Finding one loadable segment in ELF file\n");
    if(!elf_get_load_seg(elf_info)) {
        goto end1;
    }
    //Read ELF symbols
    verbose("Reading ELF symbols\n");
    if(!elf_sym_get_all(elf_info)) {
        goto end1;
    }
    //Read ELF symbols
    verbose("Collecting ELF symbols\n");
    elf_sym_collect(elf_info);
    //Build DSO module
    module = dso_module_alloc();
    verbose("Building DSO module\n");
    if(!dso_module_build(module, elf_info)) {
        goto end2;
    }
    //Write DSO module
    verbose("Writing DSO module\n");
    out_file = fopen(outfn, "w+b");
    if(!out_file) {
        fprintf(stderr, "cannot open output file: %s\n", outfn);
        goto end2;
    }
    dso_write_module(module, elf_info, out_file);
    verbose("Successfully converted input to DSO\n");
    ret = true; //Mark as having succeeded
    //Cleanup code
    fclose(out_file);
    end2:
    dso_module_free(module);
    end1:
    elf_info_free(elf_info);
    return ret;
}

int main(int argc, char *argv[])
{
    int compression = DEFAULT_COMPRESSION;
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
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                char extra;
                if (sscanf(argv[i], "%d%c", &compression, &extra) != 1) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
                if (compression < 0 || compression > MAX_COMPRESSION) {
                    fprintf(stderr, "invalid compression level: %d\n", compression);
                    return 1;
                }
            }  else {
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
        asprintf(&outfn, "%s/%s.dso", outdir, basename_noext);
        //Convert input to output
        verbose("Converting: %s -> %s\n", infn, outfn);
        if(!convert(infn, outfn)) {
            return 1;
        }
        if(compression != 0) {
            //Compress this file
            struct stat st_decomp = {0}, st_comp = {0};
            stat(outfn, &st_decomp);
            asset_compress(outfn, outfn, compression, 0);
            stat(outfn, &st_comp);
            if (verbose_flag)
                printf("compressed: %s (%d -> %d, ratio %.1f%%)\n", outfn,
                (int)st_decomp.st_size, (int)st_comp.st_size, 100.0 * (float)st_comp.st_size / (float)(st_decomp.st_size == 0 ? 1 :st_decomp.st_size));
        }
        free(outfn);
    }
    return 0;
}
