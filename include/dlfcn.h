/**
 * @file dlfcn.h
 * @brief Dynamic linker subsystem
 * @ingroup dl
 */
#ifndef __LIBDRAGON_DLFCN_H
#define __LIBDRAGON_DLFCN_H

/** @brief Flag for compatibility */
#define RTLD_LAZY 0x0
/** @brief Flag for compatibility */
#define RTLD_NOW 0x0
/** @brief Export symbols to other dynamic libraries */
#define RTLD_GLOBAL 0x1
/** @brief Don't export symbols to other dynamic libraries */
#define RTLD_LOCAL 0x0
/** @brief Never unload dynamic library from memory */
#define RTLD_NODELETE 0x2
/** @brief Don't load dynamic library to memory if not loaded */
#define RTLD_NOLOAD 0x4

/** @brief Handle for dlsym to find first occurrence of symbol */
#define RTLD_DEFAULT ((void *)-1)
/** @brief Handle for dlsym to find next occurrence of symbol */
#define RTLD_NEXT ((void *)-2)

/** @brief dl_addr info structure */
typedef struct {
    /** @brief Pathname of shared object that contains address */
    const char *dli_fname;
    /** @brief Base address at which shared object is loaded */
    void       *dli_fbase;
    /** @brief Name of symbol whose definition overlaps addr */
    const char *dli_sname;
    /** @brief Exact address of symbol named in dli_sname */
    void       *dli_saddr;
} Dl_info;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Open dynamic library
 * 
 * @param filename  Path to dynamic library
 * @param mode      Flags for loading dynamic library
 * @return Handle for loaded dynamic library
 */
void *dlopen(const char *filename, int mode);

/**
 * @brief Grab symbol from loaded dynamic library
 * 
 * @param handle    Dynamic library handle to search symbol from
 * @param symbol    Name of symbol to search for
 * @return Pointer to symbol
 */
void *dlsym(void *handle, const char *symbol);

/**
 * @brief Close loaded dynamic library
 * 
 * @param handle    Dynamic library handle to close
 * @return Whether an error occurred
 */
int dlclose(void *handle);

/**
 * @brief Convert address to symbol
 * 
 * @param addr  Address to search
 * @param info  Info of symbol found
 * @return Zero on success and non-zero on failure
 */
int dladdr(const void *addr, Dl_info *info);

/**
 * @brief Return last error that occurred in dynamic linker
 * 
 * @return String describing last error occurring in dynamic linker
 */
char *dlerror(void);

#ifdef __cplusplus
}
#endif

#endif