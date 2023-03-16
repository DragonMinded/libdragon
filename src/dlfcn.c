/**
 * @file dlfcn.c
 * @brief Dynamic linker subsystem
 * @ingroup dl
 */
#include <malloc.h>
#include <string.h>
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

/** @brief USO list head */
struct dl_module_s *__dl_module_head;
/** @brief USO list tail */
struct dl_module_s *__dl_module_tail;

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

struct dl_module_s *__dlget_loaded_module(void *addr)
{
    return NULL;
}

char *dlerror(void)
{
    return NULL;
}
