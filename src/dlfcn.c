/**
 * @file dlfcn.c
 * @brief Dynamic linker subsystem
 * @ingroup dl
 */
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
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
_Static_assert(sizeof(uso_module_t) == 28, "uso_module_t size is wrong");
_Static_assert(sizeof(uso_load_info_t) == 16, "uso_load_info_t size is wrong");

#define PTR_ROUND_UP(ptr, d) ((void *)ROUND_UP((uintptr_t)(ptr), (d)))
#define PTR_DECODE(base, ptr) ((void*)(((uint8_t*)(base)) + (uintptr_t)(ptr)))

/** @brief Function to register exception frames */
extern void __register_frame_info(void *ptr, void *object);
/** @brief Function to unregister exception frames */
extern void __deregister_frame_info(void *ptr);
/** @brief Function to run atexit destructors for a module */
extern void __cxa_finalize(void *dso);

/** @brief Demangler function */
demangle_func __dl_demangle_func;

/** @brief Module list head */
static dl_module_t *module_list_head;
/** @brief Module list tail */
static dl_module_t *module_list_tail;
/** @brief String of last error */
static char error_string[256];
/** @brief Whether an error is present */
static bool error_present;
/** @brief Main executable symbol table */
static uso_sym_t *mainexe_sym_table;
/** @brief Number of symbols in main executable symbol table */
static uint32_t mainexe_sym_count;

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

static void remove_module(dl_module_t *module)
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

static void fixup_sym_names(uso_sym_t *syms, uint32_t num_syms)
{
    //Fixup symbol name pointers
    for(uint32_t i=0; i<num_syms; i++) {
        syms[i].name = PTR_DECODE(syms, syms[i].name);
    }
}

static void load_mainexe_sym_table()
{
    mainexe_sym_info_t __attribute__((aligned(8))) mainexe_sym_info;
    //Search for main executable symbol table
    uint32_t rom_addr = rompak_search_ext(".msym");
    if(rom_addr == 0) {
        debugf("Main executable symbol table missing\n");
        return;
    }
    //Read header for main executable symbol table
    data_cache_hit_writeback_invalidate(&mainexe_sym_info, sizeof(mainexe_sym_info));
    dma_read_raw_async(&mainexe_sym_info, rom_addr, sizeof(mainexe_sym_info));
    dma_wait();
    //Verify main executable symbol table
    if(mainexe_sym_info.magic != USO_MAINEXE_SYM_DATA_MAGIC || mainexe_sym_info.size == 0) {
        debugf("Invalid main executable symbol table\n");
        return;
    }
    //Read main executable symbol table
    mainexe_sym_table = malloc(mainexe_sym_info.size);
    data_cache_hit_writeback_invalidate(mainexe_sym_table, mainexe_sym_info.size);
    dma_read_raw_async(mainexe_sym_table, rom_addr+sizeof(mainexe_sym_info), mainexe_sym_info.size);
    dma_wait();
    //Fixup main executable symbol table
    mainexe_sym_count = mainexe_sym_info.num_syms;
    fixup_sym_names(mainexe_sym_table, mainexe_sym_count);
}

static int sym_compare(const void *arg1, const void *arg2)
{
    const uso_sym_t *sym1 = arg1;
    const uso_sym_t *sym2 = arg2;
    return strcmp(sym1->name, sym2->name);
}

static uso_sym_t *search_sym_array(uso_sym_t *syms, uint32_t num_syms, const char *name)
{
    uso_sym_t search_sym = { (char *)name, 0, 0 };
    return bsearch(&search_sym, syms, num_syms, sizeof(uso_sym_t), sym_compare);
}

static uso_sym_t *search_module_exports(uso_module_t *module, const char *name)
{
    uint32_t first_export_sym = module->num_import_syms+1;
    return search_sym_array(&module->syms[first_export_sym], module->num_syms-first_export_sym, name);
}

static uso_sym_t *search_module_next_sym(dl_module_t *from, const char *name)
{
    //Iterate through further modules symbol tables
    dl_module_t *curr = from;
    while(curr) {
        //Search only symbol tables with symbols exposed
        if(curr->mode & RTLD_GLOBAL) {
            //Search through module symbol table
            uso_sym_t *symbol = search_module_exports(curr->module, name);
            if(symbol) {
                //Found symbol in module symbol table
                return symbol;
            }
        }
        curr = curr->next; //Iterate to next module
    }
    return NULL;
}

static uso_sym_t *search_global_sym(const char *name)
{
    //Load main executable symbol table if not loaded
    if(!mainexe_sym_table) {
        load_mainexe_sym_table();
    }
    //Search main executable symbol table if present
    if(mainexe_sym_table) {
        uso_sym_t *symbol = search_sym_array(mainexe_sym_table, mainexe_sym_count, name);
        if(symbol) {
            //Found symbol in main executable
            return symbol;
        }
    }
    //Search whole list of modules
    return search_module_next_sym(module_list_head, name);
}

static void resolve_syms(uso_module_t *module)
{
    for(uint32_t i=0; i<module->num_syms; i++) {
        if(i >= 1 && i < module->num_import_syms+1) {
            uso_sym_t *found_sym = search_global_sym(module->syms[i].name);
            bool weak = false;
            if(module->syms[i].info & 0x80000000) {
                weak = true;
            }
            if(!weak) {
                if(!found_sym) {
                    if(__dl_demangle_func) {
                        //Output symbol find error with demangled name if one exists
                        char *demangle_name = __dl_demangle_func(module->syms[i].name);
                        if(demangle_name) {
                            assertf(0, "Failed to find symbol %s(%s)", module->syms[i].name, demangle_name);
                        }
                    }
                    assertf(0, "Failed to find symbol %s", module->syms[i].name);
                }
                module->syms[i].value = found_sym->value;
            } else {
                //Set symbol value if found
                if(found_sym) {
                    module->syms[i].value = found_sym->value;
                }
            }
        } else {
            //Add program base address to non-absolute symbol addresses
            if(!(module->syms[i].info & 0x40000000)) {
                module->syms[i].value = (uintptr_t)PTR_DECODE(module->prog_base, module->syms[i].value);
            }
        }
    }
}

static void output_error(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vsnprintf(error_string, sizeof(error_string), fmt, va);
    debugf(error_string);
    debugf("\n");
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
        curr = curr->next;
    }
    return NULL;
}

static void flush_module(uso_module_t *module)
{
    //Invalidate data cache
    data_cache_hit_writeback_invalidate(module->prog_base, module->prog_size);
    inst_cache_hit_invalidate(module->prog_base, module->prog_size);
}

static void relocate_module(uso_module_t *module)
{
    //Process relocations
    for(uint32_t i=0; i<module->num_relocs; i++) {
        uso_reloc_t *reloc = &module->relocs[i];
        u_uint32_t *target = PTR_DECODE(module->prog_base, reloc->offset);
        uint8_t type = reloc->info >> 24;
        //Calculate symbol address
        uint32_t sym_addr = module->syms[reloc->info & 0xFFFFFF].value;
        switch(type) {
            case R_MIPS_NONE:
                break;
                
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
                for(uint32_t j=i+1; j<module->num_relocs; j++) {
                    uso_reloc_t *new_reloc = &module->relocs[j];
                    type = new_reloc->info >> 24;
                    if(type == R_MIPS_LO16) {
                        //Pair for R_MIPS_HI16 relocation found
                        u_uint32_t *lo_target = PTR_DECODE(module->prog_base, new_reloc->offset);
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

static void link_module(uso_module_t *module)
{
    //Relocate module pointers
    module->syms = PTR_DECODE(module, module->syms);
    module->relocs = PTR_DECODE(module, module->relocs);
    module->prog_base = PTR_DECODE(module, module->prog_base);
    fixup_sym_names(module->syms, module->num_syms);
    resolve_syms(module);
    relocate_module(module);
    flush_module(module);
}

static void start_module(dl_module_t *handle)
{
    uso_module_t *module = handle->module;
    uso_sym_t *eh_frame_begin = search_module_exports(module, "__EH_FRAME_BEGIN__");
    if(eh_frame_begin) {
        __register_frame_info((void *)eh_frame_begin->value, handle->ehframe_obj);
    }
    uso_sym_t *ctor_list = search_module_exports(module, "__CTOR_LIST__");
    if(ctor_list) {
        func_ptr *curr = (func_ptr *)ctor_list->value;
        while(*curr) {
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
        size_t module_size;
        //Open asset file
        FILE *file = asset_fopen(filename);
        fread(&load_info, sizeof(uso_load_info_t), 1, file); //Read load info
        //Verify USO file
        assertf(load_info.magic == USO_MAGIC, "Invalid USO file");
        //Calculate module size
        module_size = load_info.size+load_info.extra_mem;
        //Calculate loaded file size
        size_t alloc_size = sizeof(dl_module_t);
        //Add room for filename including additional .sym extension and null terminator
        size_t filename_len = strlen(filename);
        alloc_size += filename_len+5;
        //Add room for module
        alloc_size = ROUND_UP(alloc_size, load_info.mem_align);
        alloc_size += module_size;
        handle = memalign(load_info.mem_align, alloc_size); //Allocate everything in 1 chunk
        //Initialize handle
        handle->prev = handle->next = NULL; //Initialize module links to NULL
        //Initialize well known module parameters
        handle->mode = mode;
        handle->module_size = module_size;
        //Initialize pointer fields
        handle->filename = PTR_DECODE(handle, sizeof(dl_module_t)); //Filename is after handle data
        handle->module = PTR_DECODE(handle, alloc_size-module_size); //Module is at end of allocation
        //Read module
        memset(handle->module, 0, module_size);
        fread(handle->module, load_info.size, 1, file);
        fclose(file);
        //Copy filename to structure
        strcpy(handle->filename, filename);
        //Try finding symbol file in ROM
        strcpy(&handle->filename[filename_len], ".sym");
        //Calculate physical address of ROM file
        handle->debugsym_romaddr = dfs_rom_addr(handle->filename+5) & 0x1FFFFFFF;
        if(handle->debugsym_romaddr == 0) {
            //Warn if symbol file was not found in ROM
            debugf("Could not find module symbol file %s.\n", handle->filename);
            debugf("Will not get symbolic backtraces through this module.\n");
        }
        handle->filename[filename_len] = 0; //Re-add filename terminator in right spot
        //Link module
        link_module(handle->module);
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

void *dlsym(void *handle, const char *symbol)
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
        symbol_info = search_module_exports(module->module, symbol);
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
    void *min_addr = module->module->prog_base;
    void *max_addr = PTR_DECODE(min_addr, module->module->prog_size);
    //Iterate over modules
    dl_module_t *curr = module_list_head;
    while(curr) {
        //Skip this module
        if(curr == module) {
            curr = curr->next; //Iterate to next module
            continue;
        }
        //Search through imports referencing this module
        for(uint32_t i=0; i<curr->module->num_import_syms; i++) {
            void *addr = (void *)curr->module->syms[i+1].value;
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
    //Call atexit destructors for this module
    uso_sym_t *dso_handle = search_module_exports(module_data, "__dso_handle");
    if(dso_handle) {
        __cxa_finalize((void *)dso_handle->value);
    }
    //Run destructors for this module
    uso_sym_t *dtor_list = search_module_exports(module_data, "__DTOR_LIST__");
    if(dtor_list) {
        func_ptr *curr = (func_ptr *)dtor_list->value;
        while(*curr) {
            (*curr)();
            curr++;
        }
    }
    //Deregister exception frames for this module
    uso_sym_t *eh_frame_begin = search_module_exports(module_data, "__EH_FRAME_BEGIN__");
    if(eh_frame_begin) {
        __register_frame_info((void *)eh_frame_begin->value, module->ehframe_obj);
    }
}

static void close_module(dl_module_t *module)
{
    //Deinitialize module
    end_module(module);
    //Remove module from memory
    remove_module(module);
    free(module);
}

static void close_unused_modules()
{
    //Iterate through modules
    dl_module_t *curr = module_list_head;
    while(curr) {
        dl_module_t *next = curr->next; //Find next module before being removed
        //Close module if 0 uses remain and module is not referenced
        if(curr->use_count == 0 && !is_module_referenced(curr)) {
            close_module(curr);
        }
        curr = next; //Iterate to next module
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
    //Decrease use count to minimum of 0
    if(module->use_count > 0) {
        --module->use_count;
    }
    //Close this module if possible
    if(module->use_count == 0 && !is_module_referenced(module)) {
        close_module(module);
    }
    //Close any modules that are now unused
    close_unused_modules();
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
    //Iterate over export symbols
    uint32_t first_export_sym = module->module->num_import_syms+1;
    for(uint32_t i=0; i<module->module->num_syms-first_export_sym; i++) {
        uso_sym_t *sym = &module->module->syms[first_export_sym+i];
        //Calculate symbol address range
        void *sym_min = (void *)sym->value;
        uint32_t sym_size = sym->info & 0x3FFFFFFF;
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
        void *min_addr = curr->module->prog_base;
        void *max_addr = PTR_DECODE(min_addr, curr->module->prog_size);
        if(addr >= min_addr && addr < max_addr) {
            //Address is inside module
            return curr;
        }
        curr = curr->next; //Iterate to next module
    }
    //Address is not inside any module
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