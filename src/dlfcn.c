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

/**
 * @defgroup dl Dynamic linker subsystem
 * @ingroup libdragon
 * @brief Interface to libdl-style dynamic linker
 *
 * The dynamic linker subsystem allows users to load code from the
 * program's DragonFS filesystem (see dfs.h). Code is stored in a custom
 * dynamically linked format (extension of .dso) to allow for loading
 * and running code placed at arbitrary memory addresses and resolving
 * external references to the main executable and other dynamically
 * linked modules. External references are resolved by name with symbol
 * tables provided by each dynamically linked module and are also
 * provided by a file in the rompak (MSYM) (see rompak_internal.h) for
 * the main executable.
 *
 * To access this system, one must first call dlopen to load a
 * dynamically linked module and return a handle to the module.
 * Then, one can all dlsym to access functions and variables exported
 * from this module with the returned handle. This function can also
 * access symbols that are in the global symbol table with the
 * special handle RTLD_DEFAULT. Once one is done with the module, one
 * can call dlclose to close the module.
 *
 * @{
 */

/** @brief Macro to round up pointer */
#define PTR_ROUND_UP(ptr, d) ((void *)ROUND_UP((uintptr_t)(ptr), (d)))
/** @brief Macro to add base to pointer */
#define PTR_DECODE(base, ptr) ((void*)(((uint8_t*)(base)) + (uintptr_t)(ptr)))

/** @brief Function to register exception frames */
extern void __register_frame_info(void *ptr, void *object);
/** @brief Function to unregister exception frames */
extern void __deregister_frame_info(void *ptr);
/** @brief Function to run atexit destructors for a module */
extern void __cxa_finalize(void *dso);

/** @brief Demangler function */
demangle_func __dl_demangle_func;

/** @brief Module resolver */
module_lookup_func __dl_lookup_module;
/** @brief Module list head */
dl_module_t *__dl_list_head;
/** @brief Module list tail */
dl_module_t *__dl_list_tail;
/** @brief Number of loaded modules */
size_t __dl_num_loaded_modules;
/** @brief String of last error */
static char error_string[256];
/** @brief Whether an error is present */
static bool error_present;
/** @brief Main executable symbol table */
static dso_sym_t *mainexe_sym_table;
/** @brief Number of symbols in main executable symbol table */
static uint32_t mainexe_sym_count;

/**
 * @brief Insert module into module list
 * 
 * This function is non-static to help debuggers support overlays.
 *
 * @param module  Pointer to module
 */
void __attribute__((noinline)) __dl_insert_module(dl_module_t *module)
{
    dl_module_t *prev = __dl_list_tail;
    //Insert module at end of list
    if(!prev) {
        __dl_list_head = module;
    } else {
        prev->next = module;
    }
    //Set up module links
    module->prev = prev;
    module->next = NULL;
    __dl_list_tail = module; //Mark this module as end of list
	__dl_num_loaded_modules++; //Mark one more loaded module
}

/**
 * @brief Remove module from module list
 * 
 * This function is non-static to help debuggers support overlays.
 *
 * @param module  Pointer to module
 */
void __attribute__((noinline)) __dl_remove_module(dl_module_t *module)
{
    dl_module_t *next = module->next;
    dl_module_t *prev = module->prev;
    //Remove back links to this module
    if(!next) {
        __dl_list_tail = prev;
    } else {
        next->prev = prev;
    }
    //Remove forward links to this module
    if(!prev) {
        __dl_list_head = next;
    } else {
        prev->next = next;
    }
	__dl_num_loaded_modules--; //Remove one loaded module
}

static void fixup_sym_names(dso_sym_t *syms, uint8_t *name_base, uint32_t num_syms)
{
    //Fixup symbol name pointers
    for(uint32_t i=0; i<num_syms; i++) {
        syms[i].name = PTR_DECODE(name_base, syms[i].name);
    }
}

static void load_mainexe_sym_table()
{
    mainexe_sym_info_t __attribute__((aligned(8))) mainexe_sym_info;
    //Search for main executable symbol table
    uint32_t rom_addr = rompak_search_ext(".msym");
    assertf(rom_addr != 0, "Main executable symbol table not found");
    //Read header for main executable symbol table
    data_cache_hit_writeback_invalidate(&mainexe_sym_info, sizeof(mainexe_sym_info));
    dma_read_raw_async(&mainexe_sym_info, rom_addr, sizeof(mainexe_sym_info));
    dma_wait();
    //Verify main executable symbol table
    if(mainexe_sym_info.magic != DSO_MAINEXE_SYM_DATA_MAGIC || mainexe_sym_info.size == 0) {
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
    fixup_sym_names(mainexe_sym_table, (uint8_t *)mainexe_sym_table, mainexe_sym_count);
}

static int sym_compare(const void *arg1, const void *arg2)
{
    const dso_sym_t *sym1 = arg1;
    const dso_sym_t *sym2 = arg2;
    return strcmp(sym1->name, sym2->name);
}

static dso_sym_t *search_sym_array(dso_sym_t *syms, uint32_t num_syms, const char *name)
{
    dso_sym_t search_sym = { (char *)name, 0, 0 };
    return bsearch(&search_sym, syms, num_syms, sizeof(dso_sym_t), sym_compare);
}

static dso_sym_t *search_module_exports(dso_module_t *module, const char *name)
{
    uint32_t first_export_sym = module->num_import_syms+1;
    return search_sym_array(&module->syms[first_export_sym], module->num_syms-first_export_sym, name);
}

static dso_sym_t *search_module_next_sym(dl_module_t *from, const char *name)
{
    //Iterate through further modules symbol tables
    dl_module_t *curr = from;
    while(curr) {
        //Search only symbol tables with symbols exposed
        if(curr->mode & RTLD_GLOBAL) {
            //Search through module symbol table
            dso_sym_t *symbol = search_module_exports(curr, name);
            if(symbol) {
                //Found symbol in module symbol table
                return symbol;
            }
        }
        curr = curr->next; //Iterate to next module
    }
    return NULL;
}

static dso_sym_t *search_global_sym(const char *name)
{
    //Load main executable symbol table if not loaded
    if(!mainexe_sym_table) {
        load_mainexe_sym_table();
    }
    //Search main executable symbol table if present
    if(mainexe_sym_table) {
        dso_sym_t *symbol = search_sym_array(mainexe_sym_table, mainexe_sym_count, name);
        if(symbol) {
            //Found symbol in main executable
            return symbol;
        }
    }
    //Search whole list of modules
    return search_module_next_sym(__dl_list_head, name);
}

static void resolve_syms(dso_module_t *module)
{
    for(uint32_t i=0; i<module->num_syms; i++) {
        if(i >= 1 && i < module->num_import_syms+1) {
            dso_sym_t *found_sym = search_global_sym(module->syms[i].name);
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
    dl_module_t *curr = __dl_list_head;
    while(curr) {
        if(!strcmp(filename, curr->filename)) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

static void flush_module(dso_module_t *module)
{
    //Invalidate data cache
    data_cache_hit_writeback_invalidate(module->prog_base, module->prog_size);
    inst_cache_hit_invalidate(module->prog_base, module->prog_size);
}

static void relocate_module(dso_module_t *module)
{
    //Process relocations
    for(uint32_t i=0; i<module->num_relocs; i++) {
        dso_reloc_t *reloc = &module->relocs[i];
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
                    dso_reloc_t *new_reloc = &module->relocs[j];
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

static void link_module(dso_module_t *module)
{
    //Relocate module pointers
    module->syms = PTR_DECODE(module, module->syms);
    module->relocs = PTR_DECODE(module, module->relocs);
    module->prog_base = PTR_DECODE(module, module->prog_base);
    module->src_elf = PTR_DECODE(module, module->src_elf);
    module->filename = PTR_DECODE(module, module->filename);
    fixup_sym_names(module->syms, (uint8_t *)module, module->num_syms);
    resolve_syms(module);
    relocate_module(module);
    flush_module(module);
}

static void start_module(dl_module_t *handle)
{
    dso_sym_t *eh_frame_begin = search_module_exports(handle, "__EH_FRAME_BEGIN__");
    if(eh_frame_begin) {
        __register_frame_info((void *)eh_frame_begin->value, handle->ehframe_obj);
    }
    dso_sym_t *ctor_list = search_module_exports(handle, "__CTOR_LIST__");
    if(ctor_list) {
        func_ptr *curr = (func_ptr *)ctor_list->value;
        while(*curr) {
            (*curr)();
            curr--;
        }
    }
}

static dl_module_t *lookup_module(const void *addr)
{
    //Iterate over modules
    dl_module_t *curr = __dl_list_head;
    while(curr) {
        //Get module address range
        void *min_addr = curr->prog_base;
        void *max_addr = PTR_DECODE(min_addr, curr->prog_size);
        if(addr >= min_addr && addr < max_addr) {
            //Address is inside module
            return curr;
        }
        curr = curr->next; //Iterate to next module
    }
    //Address is not inside any module
    return NULL;
}

void *dlopen(const char *filename, int mode)
{
    dl_module_t *handle;
    assertf(strlen(filename) <= 255, "dlopen only supports filenames with no more than 255 characters");
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
        handle->ref_count++;
    } else {
        handle = asset_load(filename, NULL);
        assertf(handle->magic == DSO_MAGIC, "Invalid DSO file");
        link_module(handle);
        handle->mode = mode;
		if(strncmp(filename, "rom:/", 5) == 0) {
			sprintf(handle->filename, "%s.sym", filename+5);
			handle->sym_romofs = dfs_rom_addr(handle->filename) & 0x1FFFFFFF;
			if(handle->sym_romofs == 0) {
				//Warn if symbol file was not found in ROM
				debugf("Could not find module symbol file %s.\n", handle->filename);
				debugf("Will not get symbolic backtraces through this module.\n");
			}
		} else {
			//Warn that no symbolic backtraces are possible
			debugf("DSO file %s does not come from ROM.\n", filename);
			debugf("Symbolic backtraces will be disabled.\n");
			handle->sym_romofs = 0;
		}
        
        strcpy(handle->filename, filename);
        //Add module handle to list
        handle->ref_count = 1;
		__dl_lookup_module = lookup_module;
        __dl_insert_module(handle);
        //Start running module
        start_module(handle);
    }
    //Return module handle
    return handle;
}

static bool is_valid_module(dl_module_t *module)
{
    //Iterate over loaded modules
    dl_module_t *curr = __dl_list_head;
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
    dso_sym_t *symbol_info;
    if(handle == RTLD_DEFAULT) {
        //RTLD_DEFAULT searches through global symbols
        symbol_info = search_global_sym(symbol);
    } else if(handle == RTLD_NEXT) {
        //RTLD_NEXT starts searching at module dlsym was called from
        dl_module_t *module = lookup_module(__builtin_return_address(0));
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
        symbol_info = search_module_exports(module, symbol);
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
    void *min_addr = module->prog_base;
    void *max_addr = PTR_DECODE(min_addr, module->prog_size);
    //Iterate over modules
    dl_module_t *curr = __dl_list_head;
    while(curr) {
        //Skip this module
        if(curr == module) {
            curr = curr->next; //Iterate to next module
            continue;
        }
        //Search through imports referencing this module
        for(uint32_t i=0; i<curr->num_import_syms; i++) {
            void *addr = (void *)curr->syms[i+1].value;
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
    //Call atexit destructors for this module
    dso_sym_t *dso_handle = search_module_exports(module, "__dso_handle");
    if(dso_handle) {
        __cxa_finalize((void *)dso_handle->value);
    }
    //Run destructors for this module
    dso_sym_t *dtor_list = search_module_exports(module, "__DTOR_LIST__");
    if(dtor_list) {
        func_ptr *curr = (func_ptr *)dtor_list->value;
        while(*curr) {
            (*curr)();
            curr++;
        }
    }
    //Deregister exception frames for this module
    dso_sym_t *eh_frame_begin = search_module_exports(module, "__EH_FRAME_BEGIN__");
    if(eh_frame_begin) {
        __register_frame_info((void *)eh_frame_begin->value, module->ehframe_obj);
    }
}

static void close_module(dl_module_t *module)
{
    //Deinitialize module
    end_module(module);
    //Remove module from memory
    __dl_remove_module(module);
    free(module);
}

static void close_unused_modules()
{
    //Iterate through modules
    dl_module_t *curr = __dl_list_head;
    while(curr) {
        dl_module_t *next = curr->next; //Find next module before being removed
        //Close module if 0 uses remain and module is not referenced
        if(curr->ref_count == 0 && !is_module_referenced(curr)) {
            close_module(curr);
        }
        curr = next; //Iterate to next module
    }
}

int dlclose(void *handle)
{
    dl_module_t *module = handle;
    //Output error if module handle is not valid
    if(!is_valid_module(module)) {
        output_error("shared object not open");
        return 1;
    }
    //Do nothing but report success if module mode is RTLD_NODELETE
    if(module->mode & RTLD_NODELETE) {
        return 0;
    }
    //Decrease use count to minimum of 0
    if(module->ref_count > 0) {
        --module->ref_count;
    }
    //Close this module if possible
    if(module->ref_count == 0 && !is_module_referenced(module)) {
        close_module(module);
    }
    //Close any modules that are now unused
    close_unused_modules();
    //Report success
    return 0;
}

int dladdr(const void *addr, Dl_info *info)
{
    dl_module_t *module = lookup_module(addr);
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
    info->dli_fbase = module;
    //Initialize symbol properties to NULL
    info->dli_sname = NULL;
    info->dli_saddr = NULL;
    //Iterate over export symbols
    uint32_t first_export_sym = module->num_import_syms+1;
    for(uint32_t i=0; i<module->num_syms-first_export_sym; i++) {
        dso_sym_t *sym = &module->syms[first_export_sym+i];
        //Calculate symbol address range
        void *sym_min = (void *)sym->value;
        uint32_t sym_size = sym->info & 0x3FFFFFFF;
        void *sym_max = PTR_DECODE(sym_min, sym_size);
        if(addr >= sym_min && addr <= sym_max) {
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

/** @} */