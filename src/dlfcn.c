/**
 * @file dlfcn.c
 * @brief Dynamic linker subsystem
 * @ingroup dl
 */
#include <malloc.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "dlfcn.h"
#include "debug.h"
#include "asset.h"
#include "dragonfs.h"
#include "dma.h"
#include "n64sys.h"
#include "rompak_internal.h"
#include "utils.h"
#include "dlfcn_internal.h"

_Static_assert(sizeof(uso_sym_t) == 12, "uso_sym_t size is wrong");
_Static_assert(sizeof(uso_sym_table_t) == 8, "uso_sym_table_t size is wrong");
_Static_assert(sizeof(uso_reloc_table_t) == 8, "uso_reloc_table_t size is wrong");
_Static_assert(sizeof(uso_section_t) == 28, "uso_section_t size is wrong");
_Static_assert(sizeof(uso_module_t) == 32, "uso_module_t size is wrong");
_Static_assert(sizeof(uso_load_info_t) == 12, "uso_load_info_t size is wrong");

#define PTR_ROUND_UP(ptr, d) ((void *)ROUND_UP((uintptr_t)(ptr), (d)))
#define PTR_DECODE(base, ptr) ((void*)(((uint8_t*)(base)) + (uintptr_t)(ptr)))

/** @brief Function to register exception frames */
extern void __register_frame_info(void *ptr, void *object);
/** @brief Function to unregister exception frames */
extern void __deregister_frame_info(void *ptr);
/** @brief Function to run atexit destructors for a module */
extern void __cxa_finalize(void *dso);

/** @brief Module list head */
static dl_module_t *module_list_head;
/** @brief Module list tail */
static dl_module_t *module_list_tail;
/** @brief String of last error */
static char error_string[256];
/** @brief Whether an error is present */
static bool error_present;
/** @brief USO dummy section for symbol lookups */
static uso_section_t dummy_section = { NULL, 0, 0, { 0, NULL }, { 0, NULL }};

static void insert_module(dl_module_t *module)
{
    dl_module_t *prev = module_list_tail;
    //Insert module at end of list
    if(!prev) {
        module_list_head = module;
    } else {
        prev->next = module;
    }
    //Set up module links
    module->prev = prev;
    module->next = NULL;
    module_list_tail = module; //Mark this module as end of list
}

static __attribute__((unused)) void remove_module(dl_module_t *module)
{
    dl_module_t *next = module->next;
    dl_module_t *prev = module->prev;
    //Remove back links to this module
    if(!next) {
        module_list_tail = prev;
    } else {
        next->prev = prev;
    }
    //Remove forward links to this module
    if(!prev) {
        module_list_head = next;
    } else {
        prev->next = next;
    }
}

static void fixup_sym_table(uso_sym_table_t *sym_table, uso_section_t *sections)
{
    //Fixup pointer to symbol table data
    sym_table->data = PTR_DECODE(sym_table, sym_table->data);
    //Fixup symbol fields
    for(uint32_t i=0; i<sym_table->size; i++) {
        uso_sym_t *sym = &sym_table->data[i];
        uint8_t section = sym->info >> 24;
        //Fixup symbol name pointer
        sym->name = PTR_DECODE(sym_table->data, sym->name);
        //Fixup symbol value if section is valid
        sym->value = (uintptr_t)PTR_DECODE(sections[section].data, sym->value);
    }
}

static uso_sym_table_t *load_mainexe_sym_table()
{
    uso_sym_table_t *sym_table;
    mainexe_sym_info_t __attribute__((aligned(8))) mainexe_sym_info;
    //Search for main executable symbol table
    uint32_t rom_addr = rompak_search_ext(".msym");
    assertf(rom_addr != 0, "Main executable symbol table missing");
    //Read header for main executable symbol table
    data_cache_hit_writeback_invalidate(&mainexe_sym_info, sizeof(mainexe_sym_info));
    dma_read_raw_async(&mainexe_sym_info, rom_addr, sizeof(mainexe_sym_info));
    dma_wait();
    //Verify main executable symbol table
    assertf(mainexe_sym_info.magic == USO_GLOBAL_SYM_DATA_MAGIC, "Invalid main executable symbol table");
    //Read main executable symbol table
    sym_table = malloc(mainexe_sym_info.size);
    data_cache_hit_writeback_invalidate(sym_table, mainexe_sym_info.size);
    dma_read_raw_async(sym_table, rom_addr+sizeof(mainexe_sym_info), mainexe_sym_info.size);
    dma_wait();
    //Fixup main executable symbol table
    fixup_sym_table(sym_table, &dummy_section);
    return sym_table;
}

static uso_sym_t *search_sym_table(uso_sym_table_t *sym_table, const char *name)
{
    uint32_t min = 0;
    uint32_t max = sym_table->size-1;
    while(min < max) {
        uint32_t mid = (min+max)/2;
        int result = strcmp(name, sym_table->data[mid].name);
        if(result == 0) {
            return &sym_table->data[mid];
        } else if(result > 0) {
            min = mid+1;
        } else {
            max = mid-1;
        }
    }
    return NULL;
}

static uso_sym_t *search_module_next_sym(dl_module_t *start_module, const char *name)
{
    //Iterate through further modules symbol tables
    dl_module_t *curr_module = start_module;
    while(curr_module) {
        //Search only symbol tables with symbols exposed
        if(curr_module->mode & RTLD_GLOBAL) {
            //Search through module symbol table
            uso_sym_t *symbol = search_sym_table(&curr_module->module->syms, name);
            if(symbol) {
                //Found symbol in module symbol table
                return symbol;
            }
        }
        curr_module = curr_module->next; //Iterate to next module
    }
    return NULL;
}

static uso_sym_t *search_global_sym(const char *name)
{
    static uso_sym_table_t *mainexe_sym_table = NULL;
    //Load main executable symbol table if not loaded
    if(!mainexe_sym_table) {
        mainexe_sym_table = load_mainexe_sym_table();
    }
    //Search main executable symbol table
    uso_sym_t *symbol = search_sym_table(mainexe_sym_table, name);
    if(symbol) {
        //Found symbol in main executable
        return symbol;
    }
    //Search whole list of modules
    return search_module_next_sym(module_list_head, name);
}

static void resolve_external_syms(uso_sym_t *syms, uint32_t num_syms)
{
    for(uint32_t i=0; i<num_syms; i++) {
        uso_sym_t *found_sym = search_global_sym(syms[i].name);
        bool weak = false;
        if(syms[i].info & 0x800000) {
            weak = true;
        }
        assertf(weak || found_sym, "Failed to find symbol %s", syms[i].name);
        syms[i].value = found_sym->value;
    }
}

static void output_error(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vsnprintf(error_string, sizeof(error_string), fmt, va);
    debugf(error_string);
    error_present = true;
    va_end(va);
}

static dl_module_t *search_module_filename(const char *filename)
{
    dl_module_t *curr = module_list_head;
    while(curr) {
        if(!strcmp(filename, curr->filename)) {
            return curr;
        }
    }
    return NULL;
}

static void flush_module(uso_module_t *module)
{
    //Invalidate data cache for each section
    for(uint8_t i=0; i<module->num_sections; i++) {
        uso_section_t *section = &module->sections[i];
        if(section->data) {
            data_cache_hit_writeback_invalidate(section->data, section->size);
            //Also invalidate instruction cache for the text section
            if(i == module->text_section) {
                inst_cache_hit_invalidate(section->data, section->size);
            }
        }
    }
}

static void fixup_module_sections(uso_module_t *module, void *noload_start)
{
    //Fixup section base pointer
    module->sections = PTR_DECODE(module, module->sections);
    for(uint8_t i=0; i<module->num_sections; i++) {
        uso_section_t *section = &module->sections[i];
        if(section->align != 0) {
            if(section->data) {
                //Fixup section data pointer
                section->data = PTR_DECODE(module, section->data);
            } else {
                //Fixup noload section data pointer
                noload_start = PTR_ROUND_UP(noload_start, section->align); //Align data pointer
                section->data = noload_start;
                //Find next noload section pointer
                noload_start = PTR_DECODE(noload_start, section->size);
            }
        }
        //Fixup relocation section pointers
        if(section->relocs.data) {
            section->relocs.data = PTR_DECODE(module, section->relocs.data);
        }
        if(section->ext_relocs.data) {
            section->ext_relocs.data = PTR_DECODE(module, section->ext_relocs.data);
        }
    }
}

static void relocate_section(uso_module_t *module, uint8_t section_idx, bool external)
{
    uso_section_t *section = &module->sections[section_idx];
    void *base = section->data;
    //Get relocation table to use
    uso_reloc_table_t *table;
    if(external) {
        table = &section->ext_relocs;
    } else {
        table = &section->relocs;
    }
    //Process relocations
    for(uint32_t i=0; i<table->size; i++) {
        uso_reloc_t *reloc = &table->data[i];
        u_uint32_t *target = PTR_DECODE(base, reloc->offset);
        uint8_t type = reloc->info >> 24;
        //Calculate symbol address
        uint32_t sym_addr;
        if(external) {
            sym_addr = module->ext_syms.data[reloc->info & 0xFFFFFF].value;
        } else {
            sym_addr = (uint32_t)PTR_DECODE(module->sections[reloc->info & 0xFFFFFF].data, reloc->sym_value);
        }
        switch(type) {
            case R_MIPS_32:
                *target += sym_addr;
                break;
                
            case R_MIPS_26:
            {
                uint32_t target_addr = ((*target & 0x3FFFFFF) << 2)+sym_addr;
                *target = (*target & 0xFC000000)|((target_addr & 0xFFFFFFC) >> 2);
            }
            break;
            
            case R_MIPS_HI16:
            {
                uint16_t hi = *target & 0xFFFF; //Read hi from instruction
                uint32_t addr = hi << 16; //Setup address from hi
                bool lo_found = false;
                //Search for next R_MIPS_LO16 relocation
                for(uint32_t j=i+1; j<table->size; j++) {
                    uso_reloc_t *new_reloc = &table->data[j];
                    type = new_reloc->info >> 24;
                    if(type == R_MIPS_LO16) {
                        //Pair for R_MIPS_HI16 relocation found
                        u_uint32_t *lo_target = PTR_DECODE(base, new_reloc->offset);
                        int16_t lo = *lo_target & 0xFFFF; //Read lo from target of paired relocation
                        //Update address
                        addr += lo;
                        addr += sym_addr;
                        //Calculate hi
                        hi = addr >> 16;
                        if(addr & 0x8000) {
                            //Do hi carry
                            hi++;
                        }
                        lo_found = true;
                        break;
                    }
                }
                assertf(lo_found, "Unpaired R_MIPS_HI16 relocation");
                *target = (*target & 0xFFFF0000)|hi; //Write hi to instruction
            }
            break;
            
            case R_MIPS_LO16:
            {
                uint16_t lo = *target & 0xFFFF; //Read lo from instruction
                lo += sym_addr; //Calulate new lo
                *target = (*target & 0xFFFF0000)|lo; //Write lo to instruction
            }
            break;
            
            default:
                assertf(0, "Unknown relocation type %d", type);
                break;
        }
    }
}

static void relocate_module(uso_module_t *module)
{
    for(uint8_t i=0; i<module->num_sections; i++) {
        relocate_section(module, i, false);
        relocate_section(module, i, true);
    }
}

static void link_module(uso_module_t *module, void *noload_start)
{
    fixup_module_sections(module, noload_start);
    fixup_sym_table(&module->syms, module->sections);
    fixup_sym_table(&module->ext_syms, &dummy_section);
    resolve_external_syms(module->ext_syms.data, module->ext_syms.size);
    relocate_module(module);
    flush_module(module);
}

static void start_module(dl_module_t *handle)
{
    uso_module_t *module = handle->module;
    uso_section_t *eh_frame = &module->sections[module->eh_frame_section];
    uso_section_t *ctors = &module->sections[module->ctors_section];
    if(eh_frame->data && eh_frame->size > 0) {
        __register_frame_info(eh_frame->data, handle->ehframe_obj);
    }
    if(ctors->data && ctors->size != 0) {
        func_ptr *start = ctors->data;
        func_ptr *end = PTR_DECODE(start, ctors->size);
        func_ptr *curr = end-1;
        while(curr >= start) {
            (*curr)();
            curr--;
        }
    }
}

void *dlopen(const char *filename, int mode)
{
    dl_module_t *handle;
    assertf(strncmp(filename, "rom:/", 5) == 0, "Cannot open %s: dlopen only supports files in ROM (rom:/)", filename);
    handle = search_module_filename(filename);
    if(mode & ~(RTLD_GLOBAL|RTLD_NODELETE|RTLD_NOLOAD)) {
        output_error("invalid mode for dlopen()");
        return NULL;
    }
    if(mode & RTLD_NOLOAD) {
        if(handle) {
            handle->mode = mode & ~RTLD_NOLOAD;
        }
        return handle;
    }
    if(handle) {
        //Increment use count
        handle->use_count++;
    } else {
        uso_load_info_t load_info;
        void *module_noload;
        size_t module_size;
        //Open asset file
        FILE *file = asset_fopen(filename);
        fread(&load_info, sizeof(uso_load_info_t), 1, file); //Read load info
        size_t filename_len = strlen(filename);
        //Calculate module size
        module_size = load_info.size;
        //Add room in module for USO noload data
        module_size = ROUND_UP(module_size, load_info.noload_align);
        module_size += load_info.noload_size;
        //Calculate loaded file size
        size_t alloc_size = sizeof(dl_module_t);
        //Add room for filename including additional .sym extension and null terminator
        alloc_size += filename_len+5;
        //Add room for module
        alloc_size = ROUND_UP(alloc_size, load_info.align);
        alloc_size += module_size;
        handle = memalign(load_info.align, alloc_size); //Allocate module, module noload, and BSS in one chunk
        //Initialize handle
        handle->prev = handle->next = NULL; //Initialize module links to NULL
        //Initialize well known module parameters
        handle->mode = mode;
        handle->module_size = module_size;
        //Initialize pointer fields
        handle->filename = PTR_DECODE(handle, sizeof(dl_module_t)); //Filename is after handle data
        handle->module = PTR_DECODE(handle, alloc_size-module_size); //Module is at end of allocation
        module_noload = PTR_DECODE(handle, alloc_size-load_info.noload_size); //Module noload is after module
        //Read module
        fread(handle->module, load_info.size, 1, file);
        fclose(file);
        //Clear module noload portion
        memset(module_noload, 0, load_info.noload_size);
        //Copy filename to structure
        strcpy(handle->filename, filename);
        //Try finding symbol file in ROK
        strcpy(&handle->filename[filename_len], ".sym");
        handle->debugsym_romaddr = dfs_rom_addr(handle->filename);
        if(handle->debugsym_romaddr == 0) {
            //Warn if symbol file was not found in ROM
            debugf("Could not find module symbol file %s.\n", handle->filename);
            debugf("Will not get symbolic backtraces through this module.\n");
        }
        handle->filename[filename_len] = 0; //Re-add filename terminator in right spot
        //Link module
        link_module(handle->module, module_noload);
        //Add module handle to list
        handle->use_count = 1;
        insert_module(handle);
        //Start running module
        start_module(handle);
    }
    //Return module handle
    return handle;
}

static bool is_valid_module(dl_module_t *module)
{
    //Iterate over loaded modules
    dl_module_t *curr = module_list_head;
    while(curr) {
        if(curr == module) {
            //Found module loaded
            return true;
        }
        curr = curr->next; //Iterate to next module
    }
    //Module is not found
    return false;
}

void *dlsym(void *restrict handle, const char *restrict symbol)
{
    uso_sym_t *symbol_info;
    if(handle == RTLD_DEFAULT) {
        //RTLD_DEFAULT searched through global symbols
        symbol_info = search_global_sym(symbol);
    } else if(handle == RTLD_NEXT) {
        //RTLD_NEXT starts searching at module dlsym was called from
        dl_module_t *module = __dl_get_module(__builtin_return_address(0));
        if(!module) {
            //Report error if called with RTLD_NEXT from code not in module
            output_error("RTLD_NEXT used in code not dynamically loaded");
            return NULL;
        }
        symbol_info = search_module_next_sym(module, symbol);
    } else {
        //Search module symbol table
        dl_module_t *module = handle;
        assertf(is_valid_module(module), "dlsym called on invalid handle");
        symbol_info = search_sym_table(&module->module->syms, symbol);
    }
    //Output error if symbol is not found
    if(!symbol_info) {
        output_error("undefined symbol: %s", symbol);
        return NULL;
    }
    //Return symbol address
    return (void *)symbol_info->value;
}

static bool is_module_referenced(dl_module_t *module)
{
    //Address range for this module
    void *min_addr = module->module;
    void *max_addr = PTR_DECODE(min_addr, module->module_size);
    //Iterate over modules
    dl_module_t *curr = module_list_head;
    while(curr) {
        //Skip this module
        if(curr == module) {
            continue;
        }
        //Search through external symbols referencing this module
        for(uint32_t i=0; i<curr->module->ext_syms.size; i++) {
            void *addr = (void *)curr->module->ext_syms.data[i].value;
            if(addr >= min_addr && addr < max_addr) {
                //Found external symbol referencing this module
                return true;
            }
        }
        curr = curr->next; //Iterate to next module
    }
    //Did not find module being referenced by symbol
    return false;
}

static void end_module(dl_module_t *module)
{
    uso_module_t *module_data = module->module;
    //Grab section pointers
    uso_section_t *eh_frame = &module_data->sections[module_data->eh_frame_section];
    uso_section_t *dtors = &module_data->sections[module_data->dtors_section];
    //Call atexit destructors for this module
    uso_sym_t *dso_handle_symbol = search_sym_table(&module_data->syms, "__dso_handle");
    if(!dso_handle_symbol) {
        __cxa_finalize((void *)dso_handle_symbol->value);
    }
    //Run destructors for this module
    if(dtors->data && dtors->size != 0) {
        func_ptr *start = dtors->data;
        func_ptr *end = PTR_DECODE(start, dtors->size);
        func_ptr *curr = start;
        while(curr < end) {
            (*curr)();
            curr++;
        }
    }
    //Deregister exception frames for this module
    if(eh_frame->data && eh_frame->size > 0) {
        __deregister_frame_info(eh_frame->data);
    }
}

int dlclose(void *handle)
{
    dl_module_t *module = handle;
    //Output error if module handle is not valid
    if(!is_valid_module(handle)) {
        output_error("shared object not open");
        return 1;
    }
    //Do nothing but report success if module mode is RTLD_NODELETE
    if(module->mode & RTLD_NODELETE) {
        return 0;
    }
    //Close module if 0 uses remain and module is not referenced
    if(--module->use_count == 0 && !is_module_referenced(module)) {
        //Deinitialize module
        end_module(module);
        //Remove module from memory
        remove_module(module);
        free(module);
    }
    //Report success
    return 0;
}

int dladdr(const void *addr, Dl_info *info)
{
    dl_module_t *module = __dl_get_module(addr);
    if(!module) {
        //Return NULL properties
        info->dli_fname = NULL;
        info->dli_fbase = NULL;
        info->dli_sname = NULL;
        info->dli_saddr = NULL;
        return 0;
    }
    //Initialize shared object properties
    info->dli_fname = module->filename;
    info->dli_fbase = module->module;
    //Initialize symbol properties to NULL
    info->dli_sname = NULL;
    info->dli_saddr = NULL;
    for(uint32_t i=0; i<module->module->syms.size; i++) {
        uso_sym_t *sym = &module->module->syms.data[i];
        //Calculate symbol address range
        void *sym_min = (void *)sym->value;
        uint32_t sym_size = sym->info & 0x7FFFFF;
        void *sym_max = PTR_DECODE(sym_min, sym_size);
        if(addr >= sym_min && addr < sym_max) {
            //Report symbol info if inside address range
            info->dli_sname = sym->name;
            info->dli_saddr = sym_min;
            break;
        }
    }
    return 1;
}

char *dlerror(void)
{
    if(!error_present) {
        //Return nothing if error status is cleared
        return NULL;
    }
    //Return error and clear error status
    error_present = false;
    return error_string;
}

dl_module_t *__dl_get_module(const void *addr)
{
    //Iterate over modules
    dl_module_t *curr = module_list_head;
    while(curr) {
        //Get module address range
        void *min_addr = curr->module;
        void *max_addr = PTR_DECODE(min_addr, curr->module_size);
        if(addr >= min_addr && addr < max_addr) {
            //Address is inside module
            return curr;
        }
        curr = curr->next; //Iterate to next module
    }
    //Address is
    return NULL;
}

size_t __dl_get_num_modules()
{
    size_t num_modules = 0;
    //Iterate over modules
    dl_module_t *curr = module_list_head;
    while(curr) {
        curr = curr->next; //Iterate to next module
        num_modules++; //Found another module in list
    }
    //Return number of modules found in list
    return num_modules;
}

dl_module_t *__dl_get_first_module()
{
    //Return head of list
    return module_list_head;
}

dl_module_t *__dl_get_next_module(dl_module_t *module)
{
    //Return nothing if null pointer passed
    if(!module) {
        return NULL;
    }
    //Return next field
    return module->next;
}