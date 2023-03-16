/**
 * @file dlfcn.h
 * @brief Dynamic linker subsystem
 * @ingroup dl
 */
#ifndef __LIBDRAGON_DLFCN_H
#define __LIBDRAGON_DLFCN_H

/** @brief RTLD flags */
#define RTLD_LAZY 0x0       ///< For compatibility
#define RTLD_NOW 0x0        ///< For compatibility
#define RTLD_GLOBAL 0x1     ///< Export symbols to other dynamic libraries
#define RTLD_LOCAL 0x0      ///< Don't export symbols to other dynamic libraries
#define RTLD_NODELETE 0x2   ///< Never unload dynamic library from memory
#define RTLD_NOLOAD 0x4     ///< Never unload USO from memory

/** @brief Special dlsym handles */
#define RTLD_DEFAULT ((void *)0x1)  ///< Find first occurrence of symbol
#define RTLD_NEXT ((void *)0x2)     ///< Find next occurrence of symbol

/** @brief dl_addr info structure */
typedef struct {
    const char *dli_fname;  /* Pathname of shared object that
                                contains address */
    void       *dli_fbase;  /* Base address at which shared
                               object is loaded */
    const char *dli_sname;  /* Name of symbol whose definition
                               overlaps addr */
    void       *dli_saddr;  /* Exact address of symbol named
                               in dli_sname */
} Dl_info;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Open dynamic library
 * 
 * @param path      Path to dynamic library
 * @param flags     Flags for loading dynamic library
 * @return void *   Handle for loaded dynamic library
 */
void *dlopen(const char *path, int flags);

/**
 * @brief Grab symbol from loaded dynamic library
 * 
 * @param handle    Dynamic library handle to search symbol from
 * @param name      Name of symbol to search for
 * @return void*    Pointer to symbol
 */
void *dlsym(void *handle, const char *name);

/**
 * @brief Close loaded dynamic library
 * 
 * @param handle    Dynamic library handle to close
 * @return int      Return non-zero on error
 */
int dlclose(void *handle);

/**
 * @brief Convert address to symbol
 * 
 * @param addr  Address to find corresponding shared object for
 * @param info  Info structure to update
 * @return int  Return zero on success
 */
int dladdr(const void *addr, Dl_info *info);

/**
 * @brief Return last error that occurred in dynamic linker
 * 
 * @return char *   String describing last error occurring in dynamic linker
 */
char *dlerror(void);

#ifdef __cplusplus
}
#endif

#endif