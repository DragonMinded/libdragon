#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <stdexcept>
#include <vector>

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
    
    // Create new pak file if requested
    if (cmd_opts->create_pak && !file_exists(pak_file)) {
        verbose_log(global_opts, "Creating new pak file with %d KB", cmd_opts->pak_size);
        
        // Determine number of banks from pak_size
        int num_banks = (cmd_opts->pak_size * 1024) / BANK_SIZE;
        if (num_banks <= 0) num_banks = 1;
        
        try {
            auto pak = ControllerPakWrapper::create(pak_file, num_banks, global_opts->force);
            if (!pak) {
                fatal_error("Cannot create Controller Pak file '%s': %s", pak_file, strerror(errno));
            }
            
            verbose_log(global_opts, "Controller Pak file created and formatted");
            
        } catch (...) {
            unlink(pak_file);
            fatal_error("Failed to create Controller Pak image: unknown error");
        }
    }
    
    // Open the pak file 
    try {
        ControllerPakWrapper pak(pak_file, "r+b");
        if (!pak.isValid()) {
            fatal_error("Cannot open Controller Pak file '%s': %s", pak_file, strerror(errno));
        }
        
        // Mount the filesystem - this is essential for the cpak filesystem to work
        if (cpakfs_mount(JOYPAD_PORT_1, "cpak:/") < 0) {
            fatal_error("Failed to mount Controller Pak filesystem: %s", strerror(errno));
        }
        
        int files_added = 0;
        int files_updated = 0;
        
        // Process each input file
        for (int i = 0; i < num_files; i++) {
            const char *input_file = files[i];
            
            verbose_log(global_opts, "Processing file: %s", input_file);
            
            if (!file_exists(input_file)) {
                warning("File not found: %s", input_file);
                continue;
            }
            
            // Parse the input file path to determine cpak path
            // Expected format: either direct cpak path like "GAME.PUB-filename.ext"
            // or a regular filename that we'll place in a default game/publisher code
            char cpak_path[256];
            const char *basename = strrchr(input_file, '/');
            basename = basename ? basename + 1 : input_file;
            
            // Default game code - use command line option or default to DRAG.ON
            const char *default_gamecode = cmd_opts->gamecode ? cmd_opts->gamecode : "DRAG.ON";
            
            // Check if the file path already looks like a cpak path (XXXX.XX-...)
            if (strlen(basename) >= 9 && basename[4] == '.' && basename[7] == '-') {
                // It's already in cpak format, convert to internal format GAME.PB/filename.ext
                char game[5], pub[3], *filename;
                strncpy(game, basename, 4); game[4] = '\0';
                strncpy(pub, basename + 5, 2); pub[2] = '\0';
                filename = (char*)basename + 8; // Skip the '-'
                snprintf(cpak_path, sizeof(cpak_path), "%s.%s/%s", game, pub, filename);
            } else {
                // Use default or specified game/publisher code
                char game[5], pub[3];
                if (strlen(default_gamecode) >= 7 && default_gamecode[4] == '.') {
                    strncpy(game, default_gamecode, 4); game[4] = '\0';
                    strncpy(pub, default_gamecode + 5, 2); pub[2] = '\0';
                } else {
                    // Fallback to DRAG.ON if format is invalid
                    strcpy(game, "DRAG");
                    strcpy(pub, "ON");
                }
                snprintf(cpak_path, sizeof(cpak_path), "%s.%s/%s", game, pub, basename);
            }
            
            verbose_log(global_opts, "Target cpak path: %s", cpak_path);
            
            // Check if file already exists
            bool file_exists_in_pak = false;
            try {
                CPakFile existing(cpak_path, O_RDONLY);
                file_exists_in_pak = true;
                
                if (cmd_opts->update_only) {
                    verbose_log(global_opts, "File already exists in pak, updating: %s", cpak_path);
                } else {
                    verbose_log(global_opts, "File already exists in pak, overwriting: %s", cpak_path);
                }
            } catch (const std::exception&) {
                // File doesn't exist, which is fine
            }
            
            // Open source file for reading
            FILE *src = fopen(input_file, "rb");
            if (!src) {
                warning("Cannot open source file '%s': %s", input_file, strerror(errno));
                continue;
            }
            
            // Get source file size
            fseek(src, 0, SEEK_END);
            long file_size = ftell(src);
            fseek(src, 0, SEEK_SET);
            
            try {
                // Open destination file in pak using our C++ wrapper
                CPakFile dst(cpak_path, O_WRONLY | O_CREAT);
                
                verbose_log(global_opts, "Copying %ld bytes from %s to %s", file_size, input_file, cpak_path);
                
                // Copy data using configurable buffer size with RAII
                size_t buffer_size = cmd_opts->debug_bufsize;
                std::vector<char> buffer(buffer_size);
                
                size_t bytes_copied = 0;
                size_t n;
                while ((n = fread(buffer.data(), 1, buffer_size, src)) > 0) {
                    size_t written = dst.write(buffer.data(), n);
                    bytes_copied += written;
                }
                
                fclose(src);
                
                if (bytes_copied == (size_t)file_size) {
                    if (file_exists_in_pak) {
                        files_updated++;
                        verbose_log(global_opts, "Updated: %s -> %s (%zu bytes)", input_file, cpak_path, bytes_copied);
                    } else {
                        files_added++;
                        verbose_log(global_opts, "Added: %s -> %s (%zu bytes)", input_file, cpak_path, bytes_copied);
                    }
                } else {
                    warning("Incomplete copy for file '%s': %zu of %ld bytes", input_file, bytes_copied, file_size);
                }
                
            } catch (const std::exception& e) {
                fclose(src);
                warning("Cannot process file '%s': %s", input_file, e.what());
                continue;
            }
        }
        
        // Unmount filesystem
        if (cpakfs_unmount(JOYPAD_PORT_1) < 0) {
            warning("Failed to unmount Controller Pak filesystem: %s", strerror(errno));
        }
        
        verbose_log(global_opts, "Summary: %d files added, %d files updated", files_added, files_updated);
        
        return 0;
        
    } catch (...) {
        fatal_error("Failed to process Controller Pak file: unknown error");
        return -1;
    }
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
        // Use factory method to create and format the pak file
        auto pak = ControllerPakWrapper::create(pak_file, cmd_opts->num_banks, global_opts->force);
        if (!pak) {
            fatal_error("Cannot create Controller Pak file '%s': %s", pak_file, strerror(errno));
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
