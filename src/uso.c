#include <malloc.h>
#include <string.h>
#include "debug.h"
#include "asset.h"
#include "rompak_internal.h"
#include "uso.h"
#include "uso_internal.h"

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