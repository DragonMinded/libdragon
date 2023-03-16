/**
 * @file uso.h
 * @brief USO subsystem
 * @ingroup uso
 */
#ifndef __LIBDRAGON_USO_H
#define __LIBDRAGON_USO_H

/** @brief One-bit flags for loading USOs */
#define USOLDR_GLOBAL 0x1   ///< Export symbols to other USOs
#define USOLDR_NONE 0x0     ///< No flags set
#define USOLDR_NODELETE 0x2 ///< Never delete USO
#define USOLDR_NOLOAD 0x4   ///< Do not load USO even if required

/** @brief Special USO handles for uso_sym */
#define USOLDR_DEFAULT ((uso_handle_t)0x1)  ///< Find first occurrence of symbol
#define USOLDR_NEXT ((uso_handle_t)0x2)     ///< Find next occurrence of symbol

/** @brief USO handle declaration */
typedef struct loaded_uso_s *uso_handle_t;

typedef struct uso_sym_info_s {
    const char *path;
    uso_handle_t handle;
    const char *sym_name;
} uso_sym_info_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Open USO file
 * 
 * @param path              Path to USO file
 * @param flags             Flags for loading USO file
 * @return uso_handle_t     Handle for loaded USO
 */
uso_handle_t uso_open(const char *path, int flags);

/**
 * @brief Grab symbol from loaded USO handle
 * 
 * @param handle    USO handle to search symbol from
 * @param name      Name of symbol to search for
 * @return void*    Pointer to symbol
 */
void *uso_sym(uso_handle_t handle, const char *name);

/**
 * @brief Close loaded USO handle
 * 
 * @param handle    USO handle to close
 */
void uso_close(uso_handle_t handle);

/**
 * @brief Convert address to symbol
 * 
 * @param addr  Address to find corresponding shared object for
 * @param info  USO info to write back to
 */
void uso_addr(void *addr, uso_sym_info_t *sym_info);

#ifdef __cplusplus
}
#endif

#endif