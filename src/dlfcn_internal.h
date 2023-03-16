#ifndef __DLFCN_INTERNAL_H
#define __DLFCN_INTERNAL_H

#include <stdbool.h>
#include "uso_format.h"

/** @brief Loaded module data */
struct dl_module_s {
    struct dl_module_s *prev;   ///< Previous loaded dynamic library
    struct dl_module_s *next;   ///< Next loaded dynamic library
    uso_module_t *module;       ///< USO file
    uint32_t debugsym_romaddr;  ///< Debug symbol data rom address
    char *path;                 ///< Dynamic library path
    size_t ref_count;           ///< Dynamic library reference count
    uint32_t ehframe_obj[6];    ///< Exception frame object
    int flags;                  ///< Dynamic library flags
};

extern struct dl_module_s *__dl_module_head;
extern struct dl_module_s *__dl_module_tail;

/**
 * @brief Get Loaded module from address
 * 
 * @param addr                  Address to search
 * @return struct dl_module_s*  Pointer to module address is found inside
 */
struct dl_module_s *__dl_get_loaded_module(void *addr);

#endif