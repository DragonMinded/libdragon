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

// Embedded GDB script to auto-load DSO symbols
#ifndef N64_DSO
/// @cond
asm(
".pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n"
".byte 4\n"
".ascii \"gdb.inlined-script-dso-autoload\\n\"\n"
".ascii \"import gdb\\n\"\n"
".ascii \"class BreakpointDsoLoad(gdb.Breakpoint):\\n\"\n"
".ascii \"  def stop(self):\\n\"\n"
".ascii \"    frame = gdb.selected_frame()\\n\"\n"
".ascii \"    src_elf = gdb.execute('printf \\\"%s\\\", module->src_elf', False, True)\\n\"\n"
".ascii \"    prog_base = int(gdb.execute('printf \\\"%x\\\", module->prog_base', False, True), 16)\\n\"\n"
".ascii \"    print(\\\"Loading overlay: \\\", src_elf, \\\"(text:\\\", hex(prog_base), \\\")\\\")\\n\"\n"
".ascii \"    gdb.execute(\\\"add-symbol-file -readnow \\\" + src_elf + \\\" \\\" + hex(prog_base), False, True)\\n\"\n"
".ascii \"    return False\\n\"\n"
".ascii \"class BreakpointDsoFree(gdb.Breakpoint):\\n\"\n"
".ascii \"  def stop(self):\\n\"\n"
".ascii \"    frame = gdb.selected_frame()\\n\"\n"
".ascii \"    src_elf = gdb.execute('printf \\\"%s\\\", module->src_elf', False, True)\\n\"\n"
".ascii \"    prog_base = int(gdb.execute('printf \\\"%x\\\", module->prog_base', False, True), 16)\\n\"\n"
".ascii \"    print(\\\"Unloading overlay: \\\", src_elf, \\\"(text:\\\", hex(prog_base), \\\")\\\")\\n\"\n"
".ascii \"    gdb.execute(\\\"remove-symbol-file -a \\\" + hex(prog_base), False, True)\\n\"\n"
".ascii \"    return False\\n\"\n"
".ascii \"bp_load = BreakpointDsoLoad(\\\"__dl_insert_module\\\")\\n\"\n"
".ascii \"bp_load.silent = True\\n\"\n"
".ascii \"bl_free = BreakpointDsoFree(\\\"__dl_remove_module\\\")\\n\"\n"
".ascii \"bl_free.silent = True\\n\"\n"
".byte 0\n"
".popsection\n"
);
/// @endcond
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