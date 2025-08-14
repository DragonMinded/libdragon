#ifndef CPAKTOOL_H
#define CPAKTOOL_H

#include <stdbool.h>
#include "../../include/cpak.h"
#include "../../include/cpakfs.h"

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

// Exports from cpaklib.h
extern "C" {
    extern int g_num_banks;
    extern FILE *g_pak;
    extern int g_pak_offset;
}

#endif // CPAKTOOL_H
