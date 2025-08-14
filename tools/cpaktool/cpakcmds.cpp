#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "cpaktool.h"
#include "cpakwrapper.h"
#include "../../include/cpak.h"
#include "../../include/cpakfs.h"
#include "../../src/joybus/cpakfs_internal.h"

//
// COMMAND IMPLEMENTATIONS
//

int cmd_list(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file, char *patterns[], int num_patterns) {
    verbose_log(global_opts, "Listing contents of %s", pak_file);
    
    if (!file_exists(pak_file)) {
        fatal_error("File not found: %s", pak_file);
    }
    
    // TODO: Implement actual list functionality
    printf("LIST command not implemented yet\n");
    printf("Pak file: %s\n", pak_file);
    if (num_patterns > 0) {
        printf("Patterns:\n");
        for (int i = 0; i < num_patterns; i++) {
            printf("  %s\n", patterns[i]);
        }
    }
    
    // Show parsed options for debugging
    if (global_opts->verbose) {
        printf("Options: long=%d, human=%d, sort=%s, reverse=%d\n",
               cmd_opts->long_format, cmd_opts->human_readable,
               cmd_opts->sort_by ? cmd_opts->sort_by : "none",
               cmd_opts->reverse_sort);
    }
    
    return 0;
}

int cmd_extract(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file, char *patterns[], int num_patterns) {
    verbose_log(global_opts, "Extracting from %s", pak_file);
    
    if (!file_exists(pak_file)) {
        fatal_error("File not found: %s", pak_file);
    }
    
    // TODO: Implement actual extract functionality
    printf("EXTRACT command not implemented yet\n");
    printf("Pak file: %s\n", pak_file);
    printf("Overwrite: %s\n", cmd_opts->overwrite ? "yes" : "no");
    
    if (num_patterns > 0) {
        printf("Patterns:\n");
        for (int i = 0; i < num_patterns; i++) {
            printf("  %s\n", patterns[i]);
        }
    }
    
    return 0;
}

int cmd_add(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file, char *files[], int num_files) {
    verbose_log(global_opts, "Adding files to %s", pak_file);
    
    if (!file_exists(pak_file) && !cmd_opts->create_pak) {
        fatal_error("File not found: %s (use --create to create new pak)", pak_file);
    }
    
    // TODO: Implement actual add functionality
    printf("ADD command not implemented yet\n");
    printf("Pak file: %s\n", pak_file);
    printf("Create pak: %s\n", cmd_opts->create_pak ? "yes" : "no");
    printf("Pak size: %d KB\n", cmd_opts->pak_size);
    printf("Update only: %s\n", cmd_opts->update_only ? "yes" : "no");
    
    printf("Files to add:\n");
    for (int i = 0; i < num_files; i++) {
        printf("  %s\n", files[i]);
    }
    
    return 0;
}

int cmd_delete(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file, char *patterns[], int num_patterns) {
    verbose_log(global_opts, "Deleting from %s", pak_file);
    
    if (!file_exists(pak_file)) {
        fatal_error("File not found: %s", pak_file);
    }
    
    // TODO: Implement actual delete functionality
    printf("DELETE command not implemented yet\n");
    printf("Pak file: %s\n", pak_file);
    printf("Interactive: %s\n", cmd_opts->interactive ? "yes" : "no");
    
    printf("Patterns:\n");
    for (int i = 0; i < num_patterns; i++) {
        printf("  %s\n", patterns[i]);
    }
    
    return 0;
}

int cmd_info(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file) {
    verbose_log(global_opts, "Getting info for %s", pak_file);
    
    if (!file_exists(pak_file)) {
        fatal_error("File not found: %s", pak_file);
    }
    
    // TODO: Implement actual info functionality
    printf("INFO command not implemented yet\n");
    printf("Pak file: %s\n", pak_file);
    printf("Show stats: %s\n", cmd_opts->show_stats ? "yes" : "no");
    printf("Show banks: %s\n", cmd_opts->show_banks ? "yes" : "no");
    printf("Show filesystem: %s\n", cmd_opts->show_filesystem ? "yes" : "no");
    printf("Header only: %s\n", cmd_opts->header_only ? "yes" : "no");
    
    return 0;
}

static void fsck_report(void *ctx, cpakfs_issue_t issue, cpakfs_issue_level_t level, const char *fmt, ...) {
    command_options_t *cmd_opts = (command_options_t *)ctx;
    if ((int)level < cmd_opts->report_level) return;

    const char *lvl = (level == CPAKFS_LEVEL_INFO) ? "INFO" : (level == CPAKFS_LEVEL_WARNING ? "WARN" : "ERROR");
    printf("[fsck %s] ", lvl);
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
    printf("\n");
}

int cmd_test(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file) {
    verbose_log(global_opts, "Testing %s", pak_file);
    
    if (!file_exists(pak_file)) {
        fatal_error("File not found: %s", pak_file);
    }

    try {
        ControllerPakWrapper pak(pak_file);
        if (!pak.isValid()) {
            fatal_error("Cannot open file '%s': %s", pak_file, strerror(errno));
        }

        verbose_log(global_opts, "Running fsck on %s (%d banks)", pak_file, pak.getNumBanks());

        int issues = cpakfs_fsck(JOYPAD_PORT_1, cmd_opts->fix_errors, fsck_report, cmd_opts);
        if (issues < 0) {
            fatal_error("Failed to test Controller Pak image: %s", strerror(errno));
        }

        if (issues == 0) {
            printf("No issues found\n");
        } else if (cmd_opts->fix_errors) {
            printf("Fixed %d issue%s\n", issues, issues==1?"":"s");
        } else {
            printf("Found %d issue%s\n", issues, issues==1?"":"s");
        }

        return 0;
        
    } catch (...) {
        fatal_error("Failed to test Controller Pak image: unknown error");
        return -1;
    }
}

int cmd_format(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file) {
    verbose_log(global_opts, "Formatting %s with %d banks", pak_file, cmd_opts->num_banks);
    
    if (file_exists(pak_file) && !global_opts->force) {
        fatal_error("File exists: %s (use --force to overwrite)", pak_file);
    }
    
    try {
        // Use RAII factory method to handle file creation and setup
        auto pak = ControllerPakWrapper::create(pak_file, cmd_opts->num_banks, global_opts->force);
        if (!pak) {
            fatal_error("Cannot create Controller Pak file '%s': %s", pak_file, strerror(errno));
        }
        
        verbose_log(global_opts, "Initializing Controller Pak filesystem");
        
        // Initialize random number generator for filesystem creation
        srand(time(NULL));
        
        // Format the filesystem using cpakfs API directly
        int result = cpakfs_format(JOYPAD_PORT_1, true); // Always erase for fresh format
        if (result < 0) {
            unlink(pak_file); // Remove the file on failure
            fatal_error("Failed to format Controller Pak image: %s", strerror(errno));
        }
        
        size_t total_size = cmd_opts->num_banks * BANK_SIZE;
        verbose_log(global_opts, "Controller Pak image formatted successfully: %zu bytes", total_size);
        
        return 0;
        
    } catch (...) {
        unlink(pak_file); // Clean up on exception
        fatal_error("Failed to format Controller Pak image: unknown error");
        return -1;
    }
}

int cmd_convert(global_options_t *global_opts, command_options_t *cmd_opts, const char *input_file, const char *output_file) {
    verbose_log(global_opts, "Converting %s to %s", input_file, output_file);
    
    if (!file_exists(input_file)) {
        fatal_error("Input file not found: %s", input_file);
    }
    
    // TODO: Implement actual convert functionality
    printf("CONVERT command not implemented yet\n");
    printf("Input: %s\n", input_file);
    printf("Output: %s\n", output_file);
    printf("From format: %s\n", cmd_opts->from_format ? cmd_opts->from_format : "auto-detect");
    printf("To format: %s\n", cmd_opts->to_format ? cmd_opts->to_format : "auto-detect");
    
    return 0;
}

int cmd_compare(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file1, const char *pak_file2) {
    verbose_log(global_opts, "Comparing %s and %s", pak_file1, pak_file2);
    
    if (!file_exists(pak_file1)) {
        fatal_error("File not found: %s", pak_file1);
    }
    if (!file_exists(pak_file2)) {
        fatal_error("File not found: %s", pak_file2);
    }
    
    // TODO: Implement actual compare functionality
    printf("COMPARE command not implemented yet\n");
    printf("File 1: %s\n", pak_file1);
    printf("File 2: %s\n", pak_file2);
    printf("Brief: %s\n", cmd_opts->brief ? "yes" : "no");
    printf("Summary: %s\n", cmd_opts->summary ? "yes" : "no");
    
    return 0;
}
