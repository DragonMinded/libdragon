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
/** @brief Main executable symbol table */
static uso_sym_table_t *mainexe_sym_table;
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

static void fixup_sym_table(uso_sym_table_t *sym_table, uso_section_t *sections, uint8_t num_sections)
{
    //Fixup pointer to symbol table data
    sym_table->data = PTR_DECODE(sym_table, sym_table->data);
    //Fixup symbol fields
    for(uint32_t i=0; i<sym_table->length; i++) {
        uso_sym_t *sym = &sym_table->data[i];
        uint8_t section = sym->info >> 24;
        //Fixup symbol name pointer
        sym->name = PTR_DECODE(sym_table->data, sym->name);
        //Fixup symbol value if section is valid
        if(section < num_sections) {
            sym->value = (uintptr_t)PTR_DECODE(sections[section].data, sym->value);
        }
    }
}

static __attribute__((unused)) void load_mainexe_sym_table()
{
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
    mainexe_sym_table = malloc(mainexe_sym_info.size);
    data_cache_hit_writeback_invalidate(mainexe_sym_table, mainexe_sym_info.size);
    dma_read_raw_async(mainexe_sym_table, rom_addr+sizeof(mainexe_sym_info), mainexe_sym_info.size);
    dma_wait();
    //Fixup main executable symbol table
    fixup_sym_table(mainexe_sym_table, &dummy_section, 1);
}

static __attribute__((unused)) void reset_error()
{
    error_present = false;
}

static __attribute__((unused)) void output_error(const char *fmt, ...)
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

static __attribute__((unused)) void flush_module(uso_module_t *module)
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

static void link_module(uso_module_t *module, void *noload_start)
{
    
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

void *dlopen(const char *filename, int flags)
{
    dl_module_t *handle;
    assertf(strncmp(filename, "rom:/", 5) == 0, "Cannot open %s: dlopen only supports files in ROM (rom:/)", filename);
    handle = search_module_filename(filename);
    if(flags & RTLD_NOLOAD) {
        if(handle) {
            handle->flags = flags & ~RTLD_NOLOAD;
        }
        return handle;
    }
    if(handle) {
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
        handle->flags = flags;
        handle->module_size = module_size;
        handle->filename = PTR_DECODE(handle, sizeof(dl_module_t));
        handle->module = PTR_DECODE(handle, alloc_size-module_size);
        module_noload = PTR_DECODE(handle, alloc_size-load_info.noload_size);
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

void *dlsym(void *restrict handle, const char *restrict symbol)
{
    return NULL;
}

int dlclose(void *handle)
{
    return 0;
}

int dladdr(const void *addr, Dl_info *sym_info)
{
    return 1;
}

char *dlerror(void)
{
    if(!error_present) {
        return NULL;
    }
    return error_string;
}

dl_module_t *__dl_get_module(void *addr)
{
    return NULL;
}

size_t __dl_get_num_modules()
{
    dl_module_t *curr = module_list_head;
    size_t num_modules = 0;
    while(curr) {
        curr = curr->next;
        num_modules++;
    }
    return num_modules;
}

dl_module_t *__dl_get_first_module()
{
    return module_list_head;
}

dl_module_t *__dl_get_next_module(dl_module_t *module)
{
    if(!module) {
        return NULL;
    }
    return module->next;
}