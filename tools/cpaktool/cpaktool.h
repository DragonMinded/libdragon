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
    CMD_CONVERT,
    CMD_COMPARE
} command_t;

// Global options
typedef struct {
    bool verbose;
    bool force;
    bool dry_run;
    const char *output_dir;
} global_options_t;

// Command-specific options
typedef struct {
    // List options
    bool long_format;
    bool human_readable;
    const char *sort_by;
    bool reverse_sort;
    
    // Extract options
    bool overwrite;
    
    // Add options
    bool create_pak;
    int pak_size;
    bool update_only;
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
    
    // Convert options
    const char *from_format;
    const char *to_format;
    
    // Compare options
    bool brief;
    bool summary;
} command_options_t;

// Command implementations (defined in cpakcmds.cpp)
int cmd_list(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file, char *patterns[], int num_patterns);
int cmd_extract(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file, char *patterns[], int num_patterns);
int cmd_add(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file, char *files[], int num_files);
int cmd_delete(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file, char *patterns[], int num_patterns);
int cmd_info(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file);
int cmd_test(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file);
int cmd_format(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file);
int cmd_convert(global_options_t *global_opts, command_options_t *cmd_opts, const char *input_file, const char *output_file);
int cmd_compare(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file1, const char *pak_file2);

// Utility functions (defined in cpaktool.cpp)
void fatal_error(const char *fmt, ...);
void warning(const char *fmt, ...);
void verbose_log(global_options_t *opts, const char *fmt, ...);
bool file_exists(const char *path);
bool is_directory(const char *path);

// Exports from cpaklib.c
extern "C" {
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
}

#endif // CPAKTOOL_H
