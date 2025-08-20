/*
    cpaktool - Controller Pak manipulation tool
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
#ifndef CPAKTOOL_H
#define CPAKTOOL_H

#include <stdbool.h>
#include "../../include/cpak.h"
#include "../../include/cpakfs.h"
#include "../../include/dir.h"

// Command types
typedef enum {
    CMD_NONE = 0,
    CMD_LIST,
    CMD_EXTRACT,
    CMD_ADD,
    CMD_DELETE,
    CMD_INFO,
    CMD_TEST,
    CMD_FORMAT,
} command_t;

// Global options
typedef struct {
    int verbose;
    bool force;
    bool dry_run;
    const char *output_dir;
    int skip_header_bytes;  // >= 0 = skip N bytes, < 0 = auto-detect
} global_options_t;

// Command-specific options
typedef struct {
    // List options
    bool long_format;
    bool human_readable;
    bool show_crc;
    const char *sort_by;
    bool reverse_sort;
    bool json_output;
    
    // Extract options
    bool overwrite;
    const char *directory;      // Directory where to extract to
    bool extract_stdout;        // Extract to stdout instead of file
    
    // Add options
    bool create_pak;
    int pak_size;
    bool update_only;
    bool allow_partial;         // Allow partially written files on error
    const char *gamecode;       // Game code in format "ABCD.EF" (default: "DRAG.ON")
    int debug_bufsize;          // Debug: buffer size for file operations (default: 4096)
    
    // Delete options
    bool interactive;
    
    // Info options
    bool show_stats;
    bool show_banks;
    bool show_filesystem;
    bool header_only;
    
    // Test options
    bool fix_errors;
    int report_level;   // Minimum fsck level to display: 0=INFO, 1=WARNING (default), 2=ERROR
    
    // Format options
    int num_banks;
    bool size_specified;
    bool banks_specified;
} command_options_t;

// Global variables for options
extern global_options_t g_global_opts;
extern command_options_t g_command_opts;

// Command implementations (defined in cpakcmds.cpp)
int cmd_list(const char *pak_file, char *patterns[], int num_patterns);
int cmd_extract(const char *pak_file, char *patterns[], int num_patterns);
int cmd_add(const char *pak_file, char *files[], int num_files);
int cmd_delete(const char *pak_file, char *patterns[], int num_patterns);
int cmd_info(const char *pak_file);
int cmd_test(const char *pak_file);
int cmd_format(const char *pak_file);
int cmd_convert(const char *input_file, const char *output_file);
int cmd_compare(const char *pak_file1, const char *pak_file2);

// Utility functions (defined in cpaktool.cpp)
void fatal_error(const char *fmt, ...);
void warning(const char *fmt, ...);
void verbose_log(const char *fmt, ...);
bool file_exists(const char *path);
bool is_directory(const char *path);

// Exports from cpaklib.c
#ifdef __cplusplus
extern "C" {
#endif
    extern int g_num_banks;
    extern FILE *g_pak;
    extern int g_pak_offset;
    
    // C wrapper functions for direct cpak filesystem access
    void* cpak_file_open(const char *name, int flags);
    int cpak_file_read(void *file, void *buffer, int len);
    int cpak_file_write(void *file, const void *buffer, int len);
    int cpak_file_lseek(void *file, int offset, int whence);
    int cpak_file_fstat(void *file, struct stat *st);
    int cpak_file_close(void *file);
    int cpak_dir_findfirst(const char *path, dir_t *dir);
    int cpak_dir_findnext(const char *path, dir_t *dir);
    int cpak_file_unlink(const char *name);
    int cpak_file_ioctl(void *file, int request, void *arg);
#ifdef __cplusplus
}
#endif

#endif // CPAKTOOL_H
