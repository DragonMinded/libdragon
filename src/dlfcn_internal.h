/**
 * @file dlfcn_internal.h
 * @author gamemasterplc <gamemasterplc@gmail.com>
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef __DLFCN_INTERNAL_H
#define __DLFCN_INTERNAL_H

#include <stdbool.h>
#include "dso_format.h"

/**
 * @brief Handle to a dynamically loaded module.
 *
 * This is an alias for #dso_module_t.
 */
typedef dso_module_t dl_module_t;

/** @brief Generic function pointer */
typedef void (*func_ptr)();
/** @brief Demangler function pointer */
typedef char *(*demangle_func)(char *);
/** @brief Module lookup function pointer */
typedef dl_module_t *(*module_lookup_func)(const void *);
/** @brief Unaligned uint32_t */
typedef uint32_t u_uint32_t __attribute__((aligned(1)));

/** @brief MIPS ELF Relocation types */
#define R_MIPS_NONE 0   ///< Empty relocation
#define R_MIPS_32 2     ///< 32-bit pointer relocation
#define R_MIPS_26 4     ///< Jump relocation
#define R_MIPS_HI16 5   ///< High half of HI/LO pair
#define R_MIPS_LO16 6   ///< Low half of HI/LO pair
#define R_MIPS_GPREL16 7  ///< GP relative 16 bit
#define R_MIPS_TLS_TPREL_HI16 49  ///< TP-relative offset, high 16 bits
#define R_MIPS_TLS_TPREL_LO16 50  ///< TP-relative offset, low 16 bits

/** @brief Demangler function */
extern demangle_func __dl_demangle_func;
/** @brief Module lookup function */
extern module_lookup_func __dl_lookup_module;
/** @brief Module list head */
extern dl_module_t *__dl_list_head;
/** @brief Module list tail */
extern dl_module_t *__dl_list_tail;
/** @brief Number of loaded modules */
extern size_t __dl_num_loaded_modules;

#endif