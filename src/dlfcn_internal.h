#ifndef __DLFCN_INTERNAL_H
#define __DLFCN_INTERNAL_H

#include <stdbool.h>
#include "uso_format.h"

/** @brief Generic function pointer */
typedef void (*func_ptr)();
/** @brief Generic function pointer */
typedef char *(*demangle_func)(char *);
/** @brief Unaligned uint32_t */
typedef uint32_t u_uint32_t __attribute__((aligned(1)));

/** @brief MIPS ELF Relocation types */
#define R_MIPS_NONE 0   ///< Empty relocation
#define R_MIPS_32 2     ///< 32-bit pointer relocation
#define R_MIPS_26 4     ///< Jump relocation
#define R_MIPS_HI16 5   ///< High half of HI/LO pair
#define R_MIPS_LO16 6   ///< Low half of HI/LO pair

/** @brief Loaded module data */
typedef struct dl_module_s {
    struct dl_module_s *prev;   ///< Previous loaded dynamic library
    struct dl_module_s *next;   ///< Next loaded dynamic library
    uso_module_t *module;       ///< USO file
    size_t module_size;         ///< USO size
    uint32_t debugsym_romaddr;  ///< Debug symbol data rom address
    char *filename;             ///< Dynamic library filename
    size_t use_count;           ///< Dynamic library reference count
    uint32_t ehframe_obj[6];    ///< Exception frame object
    int mode;                   ///< Dynamic library flags
} dl_module_t;

/** @brief Demangler function */
extern demangle_func __dl_demangle_func;

/**
 * @brief Get pointer to loaded module from address
 * 
 * @param addr          Address to search
 * @return dl_module_t* Pointer to module address is found inside
 */
dl_module_t *__dl_get_module(const void *addr);

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