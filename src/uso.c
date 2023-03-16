#include <malloc.h>
#include <string.h>
#include "debug.h"
#include "asset.h"
#include "rompak_internal.h"
#include "uso.h"
#include "uso_internal.h"

_Static_assert(sizeof(uso_sym_t) == 12, "uso_sym_t size is wrong");
_Static_assert(sizeof(uso_sym_table_t) == 8, "uso_sym_table_t size is wrong");
_Static_assert(sizeof(uso_reloc_table_t) == 8, "uso_reloc_table_t size is wrong");
_Static_assert(sizeof(uso_section_t) == 28, "uso_section_t size is wrong");
_Static_assert(sizeof(uso_module_t) == 32, "uso_module_t size is wrong");
_Static_assert(sizeof(uso_load_info_t) == 12, "uso_load_info_t size is wrong");

/** @brief USO list head */
struct loaded_uso_s *__uso_list_head;
/** @brief USO list tail */
struct loaded_uso_s *__uso_list_tail;

uso_handle_t uso_open(const char *path, int flags)
{
    return NULL;
}

void *uso_sym(uso_handle_t handle, const char *name)
{
    return NULL;
}

void uso_close(uso_handle_t handle)
{
    
}

void uso_addr(void *addr, uso_sym_info_t *sym_info)
{
    
}

uso_handle_t __uso_get_addr_handle(void *addr)
{
    return NULL;
}