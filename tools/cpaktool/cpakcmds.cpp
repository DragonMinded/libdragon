/*
    cpaktool - Controller Pak manipulation tool
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
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
#include <string>
#include <algorithm>

#include "cpaktool.h"
#include "cpakwrapper.h"
#include "fnmatch.h"
#include "../../include/cpak.h"
#include "../../include/cpakfs.h"
#include "../../src/joybus/cpakfs_internal.h"

//
// FORWARD DECLARATIONS FOR HELPER FUNCTIONS
//

static int extract_file(global_options_t *global_opts, command_options_t *cmd_opts, const char *cpak_path);
static int add_file(global_options_t *global_opts, command_options_t *cmd_opts, const char *input_file);

//
// COMMAND IMPLEMENTATIONS
//

// Helper structure for file listing
typedef struct {
    std::string game_code;
    std::string pub_code;
    std::string filename;
    std::string extension;
    std::string full_name;  // For pattern matching
    int64_t size;
} file_entry_t;

// Helper function to calculate visual width of a UTF-8 string
// Takes into account that Japanese characters (fullwidth) take 2 columns
static size_t visual_width(const std::string& str) {
    size_t width = 0;
    for (size_t i = 0; i < str.length(); ) {
        unsigned char c = str[i];
        if (c < 0x80) {
            // ASCII character - 1 column
            width += 1;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte UTF-8 sequence - 1 column
            width += 1;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte UTF-8 sequence - check if it's a CJK character
            if (i + 2 < str.length()) {
                // CJK characters in range U+3000-U+30FF (including Katakana) are fullwidth
                unsigned char b1 = str[i];
                unsigned char b2 = str[i + 1];
                
                if (b1 == 0xE3 && b2 >= 0x80 && b2 <= 0x83) {
                    // CJK symbols and punctuation (U+3000-U+303F), Hiragana (U+3040-U+309F), 
                    // Katakana (U+30A0-U+30FF) - all fullwidth
                    width += 2;
                } else {
                    // Other 3-byte sequences - assume 1 column
                    width += 1;
                }
            } else {
                width += 1;
            }
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte UTF-8 sequence - 1 column
            width += 1;
            i += 4;
        } else {
            // Invalid UTF-8 sequence - count as 1
            width += 1;
            i += 1;
        }
    }
    return width;
}

// Helper function to pad string to visual width
static std::string pad_to_width(const std::string& str, size_t target_width) {
    size_t current_width = visual_width(str);
    if (current_width >= target_width) {
        return str;
    }
    return str + std::string(target_width - current_width, ' ');
}

// Helper function to format file size for human-readable output
static std::string format_size(int64_t size, bool human_readable) {
    if (!human_readable) {
        return std::to_string(size);
    }
    
    if (size < 1024) {
        return std::to_string(size) + "B";
    } else if (size < 1024 * 1024) {
        double kb = size / 1024.0;
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1fK", kb);
        return std::string(buf);
    } else {
        double mb = size / (1024.0 * 1024.0);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1fM", mb);
        return std::string(buf);
    }
}

// Helper function to parse cpak path into components
static bool parse_cpak_path(const std::string& cpak_path, file_entry_t& entry) {
    // Expected format: "GAME.PB/filename.ext"
    const char *path = cpak_path.c_str();
    
    if (strlen(path) < 9 || path[4] != '.' || path[7] != '/') {
        return false;
    }
    
    entry.game_code = std::string(path, 4);
    entry.pub_code = std::string(path + 5, 2);
    
    std::string filename_part = std::string(path + 8);
    size_t dot_pos = filename_part.find_last_of('.');
    
    if (dot_pos != std::string::npos) {
        entry.filename = filename_part.substr(0, dot_pos);
        entry.extension = filename_part.substr(dot_pos + 1);
    } else {
        entry.filename = filename_part;
        entry.extension = "";
    }
    
    // Create full name for pattern matching (GAME.PB-filename.ext format)
    entry.full_name = entry.game_code + "." + entry.pub_code + "-" + entry.filename;
    if (!entry.extension.empty()) {
        entry.full_name += "." + entry.extension;
    }
    
    return true;
}

// Helper function to compare files for sorting
static bool compare_files(const file_entry_t& a, const file_entry_t& b, const char* sort_by, bool reverse) {
    bool result = false;
    
    if (!sort_by || !strcmp(sort_by, "name")) {
        result = a.full_name < b.full_name;
    } else if (!strcmp(sort_by, "size")) {
        result = a.size < b.size;
    } else {
        // Default to name sorting
        result = a.full_name < b.full_name;
    }
    
    return reverse ? !result : result;
}

int cmd_list(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file, char *patterns[], int num_patterns) {
    verbose_log(global_opts, "Listing contents of %s", pak_file);
    
    if (!file_exists(pak_file)) {
        fatal_error("File not found: %s", pak_file);
    }
    
    // Open the pak file and mount filesystem
    try {
        CPakFilesystem pak(pak_file);
        
        std::vector<file_entry_t> files;
        
        // Use the new for_each_file method
        pak.for_each_file([&](const char* filename, const dir_t& dir) -> bool {
            file_entry_t entry;
            entry.size = dir.d_size >= 0 ? dir.d_size : 0;
            
            if (parse_cpak_path(std::string(filename), entry)) {
                // Check if file matches any patterns
                bool should_include = false;
                
                if (num_patterns == 0) {
                    should_include = true;
                } else {
                    for (int i = 0; i < num_patterns; i++) {
                        if (fnmatch(patterns[i], entry.full_name.c_str()) || 
                            fnmatch(patterns[i], filename)) {
                            should_include = true;
                            break;
                        }
                    }
                }
                
                if (should_include) {
                    files.push_back(entry);
                }
            }
            return true; // Continue iteration
        });
        
        // Sort files if requested
        if (!files.empty()) {
            std::sort(files.begin(), files.end(), [cmd_opts](const file_entry_t& a, const file_entry_t& b) {
                return compare_files(a, b, cmd_opts->sort_by, cmd_opts->reverse_sort);
            });
        }
        
        // Display files
        if (cmd_opts->long_format) {
            // Long format with table headers
            printf("Game       Pub    Filename                         Ext        Size\n");
            printf("----       ---    --------                         ---        ------\n");
            
            for (const auto& file : files) {
                std::string size_str = format_size(file.size, cmd_opts->human_readable);
                
                // Use visual width-aware padding for proper alignment with Japanese characters
                // Max visual widths: game=8 (4 chars * 2), pub=4 (2 chars * 2), filename=32 (16 chars * 2), ext=8 (4 chars * 2)
                std::string padded_game = pad_to_width(file.game_code, 10);      // Game column width
                std::string padded_pub = pad_to_width(file.pub_code, 6);         // Pub column width  
                std::string padded_filename = pad_to_width(file.filename, 32);   // Filename column width
                std::string padded_ext = pad_to_width(file.extension, 10);       // Ext column width
                
                // Don't use printf width specifiers - just print the padded strings directly
                printf("%s %s %s %s %s\n",
                       padded_game.c_str(),
                       padded_pub.c_str(),
                       padded_filename.c_str(),
                       padded_ext.c_str(),
                       size_str.c_str());
            }
        } else {
            // Simple format - one file per line
            for (const auto& file : files) {
                printf("%s\n", file.full_name.c_str());
            }
        }
        
        // Summary
        if (global_opts->verbose || files.empty()) {
            printf("\nFound %zu file%s\n", files.size(), files.size() == 1 ? "" : "s");
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        fatal_error("Cannot open Controller Pak file '%s': %s", pak_file, e.what());
        return -1;
    }
}

int cmd_extract(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file, char *patterns[], int num_patterns) {
    verbose_log(global_opts, "Extracting from %s", pak_file);
    
    if (!file_exists(pak_file)) {
        fatal_error("File not found: %s", pak_file);
    }
    
    // Open the pak file and mount filesystem
    try {
        CPakFilesystem pak(pak_file);
        
        verbose_log(global_opts, "Note: Controller Pak files are stored in 256-byte blocks with random padding");
        
        int files_extracted = 0;
        
        // Use the new for_each_file method
        pak.for_each_file([&](const char* filename, const dir_t& dir) -> bool {
            // Convert to output filename for pattern matching
            std::string output_filename;
            const char *slash = strchr(filename, '/');
            if (slash) {
                std::string game_pub(filename, slash - filename);
                std::string file_part(slash + 1);
                output_filename = game_pub + "-" + file_part;
            } else {
                output_filename = filename;
            }
            
            bool should_extract = false;
            
            if (num_patterns == 0) {
                // No patterns specified, extract all files
                verbose_log(global_opts, "Found file: '%s'", filename);
                should_extract = true;
            } else {
                // Check if file matches any of the patterns (check both cpak name and output name)
                for (int i = 0; i < num_patterns; i++) {
                    if (fnmatch(patterns[i], filename) || 
                        fnmatch(patterns[i], output_filename.c_str())) {
                        verbose_log(global_opts, "File '%s' (output: '%s') matches pattern '%s'", 
                                  filename, output_filename.c_str(), patterns[i]);
                        should_extract = true;
                        break;
                    }
                }
                if (!should_extract) {
                    verbose_log(global_opts, "File '%s' (output: '%s') does not match any pattern", 
                              filename, output_filename.c_str());
                }
            }
            
            if (should_extract) {
                files_extracted += extract_file(global_opts, cmd_opts, filename);
            }
            
            return true; // Continue iteration
        });
        
        verbose_log(global_opts, "Summary: %d files extracted", files_extracted);
        
        return 0;
        
    } catch (const std::exception& e) {
        fatal_error("Cannot open Controller Pak file '%s': %s", pak_file, e.what());
        return -1;
    }
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
            auto pak = CPakFilesystem::create(pak_file, num_banks, global_opts->force);
            if (!pak) {
                fatal_error("Cannot create Controller Pak file '%s': %s", pak_file, strerror(errno));
            }
            
            verbose_log(global_opts, "Controller Pak file created and formatted");
            
        } catch (...) {
            unlink(pak_file);
            fatal_error("Failed to create Controller Pak image: unknown error");
        }
    }
    
    // Open the pak file and mount filesystem
    try {
        CPakFilesystem pak(pak_file);
        
        int files_added = 0;
        int files_updated = 0;
        
        // Process each input file
        for (int i = 0; i < num_files; i++) {
            int result = add_file(global_opts, cmd_opts, files[i]);
            if (result == 1) {
                files_added++;
            } else if (result == 2) {
                files_updated++;
            }
            // result == 0 means error (already handled by add_file)
        }
        
        verbose_log(global_opts, "Summary: %d files added, %d files updated", files_added, files_updated);
        
        return 0;
        
    } catch (const std::exception& e) {
        fatal_error("Cannot open Controller Pak file '%s': %s", pak_file, e.what());
        return -1;
    }
}

int cmd_delete(global_options_t *global_opts, command_options_t *cmd_opts, const char *pak_file, char *patterns[], int num_patterns) {
    verbose_log(global_opts, "Deleting from %s", pak_file);
    
    if (!file_exists(pak_file)) {
        fatal_error("File not found: %s", pak_file);
    }
    
    // Open the pak file and mount filesystem
    try {
        CPakFilesystem pak(pak_file);
        
        std::vector<std::string> files_to_delete;
        
        // First, collect all files that match the patterns
        pak.for_each_file([&](const char* filename, const dir_t& dir) -> bool {
            // Convert to output filename for pattern matching
            std::string output_filename;
            const char *slash = strchr(filename, '/');
            if (slash) {
                std::string game_pub(filename, slash - filename);
                std::string file_part(slash + 1);
                output_filename = game_pub + "-" + file_part;
            } else {
                output_filename = filename;
            }
            
            bool should_delete = false;
            
            // Check if file matches any of the patterns
            for (int i = 0; i < num_patterns; i++) {
                if (fnmatch(patterns[i], filename) || 
                    fnmatch(patterns[i], output_filename.c_str())) {
                    verbose_log(global_opts, "File '%s' (output: '%s') matches pattern '%s'", 
                              filename, output_filename.c_str(), patterns[i]);
                    should_delete = true;
                    break;
                }
            }
            
            if (should_delete) {
                files_to_delete.push_back(std::string(filename));
            }
            
            return true; // Continue iteration
        });
        
        if (files_to_delete.empty()) {
            printf("No files match the specified patterns.\n");
            return 0;
        }
        
        int files_deleted = 0;
        int files_failed = 0;
        
        // Process each file for deletion
        for (const auto& filename : files_to_delete) {
            bool delete_file = true;
            
            // Interactive confirmation if requested
            if (cmd_opts->interactive) {
                printf("Delete '%s'? [y/N] ", filename.c_str());
                fflush(stdout);
                
                char response[16];
                if (fgets(response, sizeof(response), stdin)) {
                    if (response[0] != 'y' && response[0] != 'Y') {
                        delete_file = false;
                        verbose_log(global_opts, "Skipping '%s'", filename.c_str());
                    }
                } else {
                    delete_file = false;
                }
            }
            
            if (delete_file && !global_opts->dry_run) {
                verbose_log(global_opts, "Deleting '%s'", filename.c_str());
                
                if (cpak_file_unlink(filename.c_str()) == 0) {
                    files_deleted++;
                    if (!global_opts->verbose && !cmd_opts->interactive) {
                        printf("Deleted: %s\n", filename.c_str());
                    }
                } else {
                    files_failed++;
                    warning("Failed to delete '%s': %s", filename.c_str(), strerror(errno));
                }
            } else if (delete_file && global_opts->dry_run) {
                printf("Would delete: %s\n", filename.c_str());
                files_deleted++; // Count as "would be deleted"
            }
        }
        
        // Summary
        if (global_opts->dry_run) {
            verbose_log(global_opts, "Dry run: %d files would be deleted", files_deleted);
        } else {
            verbose_log(global_opts, "Summary: %d files deleted, %d failures", files_deleted, files_failed);
            if (files_failed > 0) {
                return 1; // Indicate partial failure
            }
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        fatal_error("Cannot open Controller Pak file '%s': %s", pak_file, e.what());
        return -1;
    }
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
        CPakFilesystem pak(pak_file, false); // Don't auto-mount for testing
        
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
        
    } catch (const std::exception& e) {
        fatal_error("Cannot open file '%s': %s", pak_file, e.what());
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
        auto pak = CPakFilesystem::create(pak_file, cmd_opts->num_banks, global_opts->force);
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

//
// HELPER FUNCTIONS FOR ADD/EXTRACT
//

static int add_file(global_options_t *global_opts, command_options_t *cmd_opts, const char *input_file) {
    verbose_log(global_opts, "Processing file: %s", input_file);
    
    if (!file_exists(input_file)) {
        fatal_error("File not found: %s", input_file);
    }
    
    // Parse the input file path to determine cpak path
    // Expected format: either direct cpak path like "GAME.PUB-filename.ext"
    // or a regular filename that we'll place in a default game/publisher code
    char cpak_path[256];
    const char *basename = strrchr(input_file, '/');
    basename = basename ? basename + 1 : input_file;
    
    // Check for filename length and handle truncation with warning
    std::string processed_basename = basename;
    char *dot = strrchr((char*)basename, '.');
    int name_len = dot ? dot - basename : strlen(basename);
    int ext_len = dot ? strlen(dot + 1) : 0;
    
    // Handle filename truncation if too long
    if (name_len > 16) {
        std::string new_name(basename, 16);
        if (dot) {
            new_name += dot;  // Add extension back
        }
        warning("Filename too long, truncating '%s' to '%s' (max 16 characters before extension)", 
               basename, new_name.c_str());
        processed_basename = new_name;
        basename = processed_basename.c_str();
    }
    
    // Handle extension truncation if too long
    if (ext_len > 4 && dot) {
        std::string new_name = std::string(basename, dot - basename + 1) + std::string(dot + 1, 4);
        warning("Extension too long, truncating '%s' to '%s' (max 4 characters)", 
               dot + 1, std::string(dot + 1, 4).c_str());
        processed_basename = new_name;
        basename = processed_basename.c_str();
    }
    
    // Validate filename characters before proceeding
    std::string unsupported_chars;
    for (const char *p = basename; *p; p++) {
        char c = *p;
        // cpakfs supports: A-Z, a-z, 0-9, space, and these symbols: ! " # ` * + , - . / : = ? @
        // (Based on utf8_to_n64_validate function in cpakfs.c)
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || 
              c == ' ' || c == '!' || c == '"' || c == '#' || c == '`' || c == '*' || 
              c == '+' || c == ',' || c == '-' || c == '.' || c == '/' || c == ':' || 
              c == '=' || c == '?' || c == '@')) {
            if (unsupported_chars.find(c) == std::string::npos) {
                unsupported_chars += c;
            }
        }
    }
    if (!unsupported_chars.empty()) {
        fatal_error("Filename contains unsupported characters: '%s' (unsupported: '%s')", 
                  basename, unsupported_chars.c_str());
    }
    
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
    bool is_update = false;
    try {
        CPakFile existing(cpak_path, O_RDONLY);
        
        if (cmd_opts->update_only) {
            verbose_log(global_opts, "File already exists in pak, updating: %s", cpak_path);
            is_update = true;
        } else {
            verbose_log(global_opts, "File already exists in pak, overwriting: %s", cpak_path);
            is_update = true;
        }
    } catch (const std::exception&) {
        // File doesn't exist, which is fine
    }
    
    // Open source file for reading
    FILE *src = fopen(input_file, "rb");
    if (!src) {
        warning("Cannot open source file '%s': %s", input_file, strerror(errno));
        return 0;
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
            if (is_update) {
                verbose_log(global_opts, "Updated: %s -> %s (%zu bytes)", input_file, cpak_path, bytes_copied);
                return 2; // Indicate update
            } else {
                verbose_log(global_opts, "Added: %s -> %s (%zu bytes)", input_file, cpak_path, bytes_copied);
                return 1; // Indicate addition
            }
        } else {
            warning("Incomplete copy for file '%s': %zu of %ld bytes", input_file, bytes_copied, file_size);
            return 0;
        }
        
    } catch (const std::exception& e) {
        fclose(src);
        
        // Check if it's a filename validation error
        if (errno == EINVAL) {
            // Analyze the filename to provide specific error message
            const char *fname = strrchr(input_file, '/');
            fname = fname ? fname + 1 : input_file;
            
            // Check filename length
            char *dot = strrchr((char*)fname, '.');
            int name_len = dot ? dot - fname : strlen(fname);
            int ext_len = dot ? strlen(dot + 1) : 0;
            
            if (name_len > 16) {
                fatal_error("Filename too long: '%s' (max 16 characters before extension, got %d)", fname, name_len);
            } else if (ext_len > 4) {
                fatal_error("Extension too long: '%s' (max 4 characters, got %d)", dot + 1, ext_len);
            } else {
                // Check for unsupported characters
                std::string unsupported_chars;
                for (const char *p = fname; *p; p++) {
                    char c = *p;
                    // Check if character is supported (A-Z, a-z, 0-9, space, and specific symbols)
                    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || 
                          c == ' ' || c == '!' || c == '"' || c == '#' || c == '`' || c == '*' || 
                          c == '+' || c == ',' || c == '-' || c == '.' || c == '/' || c == ':' || 
                          c == '=' || c == '?' || c == '@')) {
                        if (unsupported_chars.find(c) == std::string::npos) {
                            unsupported_chars += c;
                        }
                    }
                }
                if (!unsupported_chars.empty()) {
                    fatal_error("Filename contains unsupported characters: '%s' (unsupported: '%s')", 
                              fname, unsupported_chars.c_str());
                } else {
                    fatal_error("Invalid filename: '%s' (reason: %s)", fname, e.what());
                }
            }
        } else {
            warning("Cannot add file '%s': %s", input_file, e.what());
        }
        warning("Cannot process file '%s': %s", input_file, e.what());
        return 0;
    }
}

static int extract_file(global_options_t *global_opts, command_options_t *cmd_opts, const char *cpak_path) {
    try {
        // Convert cpak path (GAME.PB/filename.ext) to output filename (GAME.PB-filename.ext)
        std::string output_filename;
        const char *slash = strchr(cpak_path, '/');
        if (slash) {
            std::string game_pub(cpak_path, slash - cpak_path);
            std::string filename(slash + 1);
            // Filenames from cpakfs are already properly null-terminated without trailing spaces
            output_filename = game_pub + "-" + filename;
        } else {
            output_filename = std::string(cpak_path);
        }
        
        verbose_log(global_opts, "Extracting %s -> %s", cpak_path, output_filename.c_str());
        
        // Check if output file exists
        if (file_exists(output_filename.c_str()) && !cmd_opts->overwrite) {
            warning("File exists, skipping: %s (use --overwrite to force)", output_filename.c_str());
            return 0;
        }
        
        // Open source file in pak using C++ wrapper
        CPakFile src_file(cpak_path, O_RDONLY);
        
        // Note: cpakfs doesn't store real file size, files are always padded to 256-byte blocks
        verbose_log(global_opts, "Note: cpakfs files are padded to 256-byte blocks");
        
        // Open destination file
        FILE *dst = fopen(output_filename.c_str(), "wb");
        if (!dst) {
            warning("Cannot create output file '%s': %s", output_filename.c_str(), strerror(errno));
            return 0;
        }
        
        // Copy file content (will be padded to 256-byte boundary)
        size_t buffer_size = cmd_opts->debug_bufsize;
        std::vector<char> buffer(buffer_size);
        size_t total_bytes = 0;
        size_t bytes_read;
        
        while ((bytes_read = src_file.read(buffer.data(), buffer.size())) > 0) {
            if (fwrite(buffer.data(), 1, bytes_read, dst) != bytes_read) {
                fclose(dst);
                unlink(output_filename.c_str());
                warning("Error writing to file '%s': %s", output_filename.c_str(), strerror(errno));
                return 0;
            }
            total_bytes += bytes_read;
        }
        
        fclose(dst);
        verbose_log(global_opts, "Extracted: %s (%zu bytes)", output_filename.c_str(), total_bytes);
        return 1;
        
    } catch (const std::exception& e) {
        warning("Cannot extract file '%s': %s", cpak_path, e.what());
        return 0;
    }
}
