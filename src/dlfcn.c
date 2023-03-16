/**
 * @file dlfcn.c
 * @brief Dynamic linker subsystem
 * @ingroup dl
 */
#include <malloc.h>
#include <string.h>
#include <stdarg.h>
#include "debug.h"
#include "asset.h"
#include "rompak_internal.h"
#include "dlfcn.h"
#include "dlfcn_internal.h"

_Static_assert(sizeof(uso_sym_t) == 12, "uso_sym_t size is wrong");
_Static_assert(sizeof(uso_sym_table_t) == 8, "uso_sym_table_t size is wrong");
_Static_assert(sizeof(uso_reloc_table_t) == 8, "uso_reloc_table_t size is wrong");
_Static_assert(sizeof(uso_section_t) == 28, "uso_section_t size is wrong");
_Static_assert(sizeof(uso_module_t) == 32, "uso_module_t size is wrong");
_Static_assert(sizeof(uso_load_info_t) == 12, "uso_load_info_t size is wrong");

/** @brief Module list head */
static dl_module_t *module_list_head;
/** @brief Module list tail */
static dl_module_t *module_list_tail;
/** @brief String of last error */
static char error_string[256];
/** @brief Whether an error is present */
static bool error_present;

static __attribute__((unused)) void insert_module(dl_module_t *module)
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

void *dlopen(const char *path, int flags)
{
    return NULL;
}

void *dlsym(void *handle, const char *name)
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