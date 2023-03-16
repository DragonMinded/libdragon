#ifndef __USO_INTERNAL_H
#define __USO_INTERNAL_H

#include <stdbool.h>
#include "uso_format.h"

/** @brief Loaded USO data */
struct loaded_uso_s {
    struct loaded_uso_s *prev;  ///< Previous loaded USO
    struct loaded_uso_s *next;  ///< Next loaded USO
    uso_module_t *module;       ///< USO module
    uint32_t debugsym_romaddr;  ///< Debug symbol data rom address
    char *path;                 ///< USO path
    size_t ref_count;           ///< USO reference count
    uint32_t ehframe_obj[6];    ///< Exception frame object
    int flags;                  ///< Flag to export symbols
};

extern struct loaded_uso_s *__uso_list_head;
extern struct loaded_uso_s *__uso_list_tail;

/**
 * @brief Get Loaded USO from address
 * 
 * @param addr                      Address to search for
 * @return struct loaded_uso_s*     Pointer to loaded USO
 */
struct loaded_uso_s *__uso_get_addr_loaded_uso(void *addr);

#endif