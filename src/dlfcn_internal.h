#ifndef __DLFCN_INTERNAL_H
#define __DLFCN_INTERNAL_H

#include <stdbool.h>
#include "uso_format.h"

/** @brief Loaded module data */
typedef struct dl_module_s {
    struct dl_module_s *prev;   ///< Previous loaded dynamic library
    struct dl_module_s *next;   ///< Next loaded dynamic library
    uso_module_t *module;       ///< USO file
    size_t module_size;         ///< USO size
    uint32_t debugsym_romaddr;  ///< Debug symbol data rom address
    char *path;                 ///< Dynamic library path
    size_t ref_count;           ///< Dynamic library reference count
    uint32_t ehframe_obj[6];    ///< Exception frame object
    int flags;                  ///< Dynamic library flags
} dl_module_t;

/**
 * @brief Get pointer to loaded module from address
 * 
 * @param addr          Address to search
 * @return dl_module_t* Pointer to module address is found inside
 */
dl_module_t *__dl_get_module(void *addr);

/**
 * @brief Get number of loaded modules
 * 
 * @return size_t   Number of loaded modules
 */
size_t __dl_get_num_modules();

/**
 * @brief Get first loaded module
 * 
 * @return dl_module_t*  Pointer to first module
 */
dl_module_t *__dl_get_first_module();


/**
 * @brief Get next loaded module
 * 
 * @param module                Pointer 
 * @return dl_module_t*  Pointer to next module
 */
dl_module_t *__dl_get_next_module(dl_module_t *module);

#endif