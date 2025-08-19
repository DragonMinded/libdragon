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
#include <set>

#include "cpaktool.h"
#include "cpakwrapper.h"
#include "fnmatch.h"
#include "../../include/cpak.h"
#include "../../include/cpakfs.h"
#include "../../src/joybus/cpakfs_internal.h"
#include "../common/crc32.c"

//
// FORWARD DECLARATIONS FOR HELPER FUNCTIONS
//

static int extract_file(const char *cpak_path);
static int add_file(const char *input_file);

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
    int size;
    uint32_t crc32_value;   // CRC32 checksum of file contents
    bool crc32_error;       // True if CRC32 calculation failed
} file_entry_t;

// Controller Pak information structure
struct InfoPakStats {
    std::string pak_file;
    int num_banks;
    int pages_used;
    int pages_total;
    int notes_used;
    int notes_total;
    double pages_percent;
    double notes_percent;
    struct Issue {
        int code;
        int level;
        std::string message;
    };
    std::vector<Issue> fsck_issues;
};

// Helper for fsck issue reporting
struct FsckContext {
    std::vector<InfoPakStats::Issue> *issues;
};

// Common helper for fsck issue reporting
static void fsck_report_common(void *ctx, int code, int level, const char *msg) {
    FsckContext *c = (FsckContext*)ctx;
    InfoPakStats::Issue i;
    i.code = code;
    i.level = level;
    i.message = msg;
    c->issues->push_back(i);
}

static void fsck_report_static(void *ctx, cpakfs_issue_t issue, cpakfs_issue_level_t level, const char *fmt, ...) {
    char msg[256];
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof(msg), fmt, ap); va_end(ap);
    fsck_report_common(ctx, (int)issue, (int)level, msg);
}

static void fsck_report_json_static(void *ctx, cpakfs_issue_t issue, cpakfs_issue_level_t level, const char *fmt, ...) {
    char msg[256];
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof(msg), fmt, ap); va_end(ap);
    fsck_report_common(ctx, (int)issue, (int)level, msg);
}
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

// Helper function to format file size for human-readable output
static std::string format_size(int64_t size, bool human_readable) {
    if (size < 0) {
        return "<error>";
    }
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

// Helper function to convert \x01 to <NUL> for display
static std::string display_filename(const std::string& filename) {
    std::string result = filename;
    size_t pos = 0;
    while ((pos = result.find('\x01', pos)) != std::string::npos) {
        result.replace(pos, 1, "<NUL>");
        pos += 5; // Length of "<NUL>"
    }
    return result;
}

// Helper function to parse cpak path into components
static bool parse_cpak_path(const std::string& cpak_path, file_entry_t& entry) {
    // Parse the UTF-8 path directly by splitting on separators
    // Expected format: "GAME.PB-filename.ext" or "HEXGAMECODE.HEXPUB-filename.ext"
    
    // Find the first dot (separates game code from publisher code)
    size_t first_dot = cpak_path.find('.');
    if (first_dot == std::string::npos) {
        return false; // Must have at least one dot
    }
    
    // Game code can be 4 characters (ASCII) or 8 characters (hex)
    if (first_dot != 4 && first_dot != 8) {
        return false; // Game code should be exactly 4 or 8 characters
    }
    
    // Find the dash (separates publisher code from filename)
    size_t dash = cpak_path.find('-', first_dot + 1);
    if (dash == std::string::npos) {
        return false; // Must have a dash after publisher code
    }
    
    // Publisher code length depends on game code length
    size_t expected_pub_len = (first_dot == 4) ? 2 : 4; // 2 for ASCII, 4 for hex
    if (dash != first_dot + 1 + expected_pub_len) {
        return false; // Publisher code should be exactly 2 or 4 characters
    }
    
    // Extract game code (4 or 8 characters)
    entry.game_code = cpak_path.substr(0, first_dot);
    
    // Extract publisher code (2 or 4 characters)
    entry.pub_code = cpak_path.substr(first_dot + 1, expected_pub_len);
    
    // Find the last dot (separates filename from extension)
    size_t last_dot = cpak_path.rfind('.');
    
    if (last_dot != std::string::npos && last_dot > dash) {
        // Has extension
        entry.filename = cpak_path.substr(dash + 1, last_dot - dash - 1);
        entry.extension = cpak_path.substr(last_dot + 1);
    } else {
        // No extension
        entry.filename = cpak_path.substr(dash + 1);
        entry.extension = "";
    }
    
    // The full name is just the original path
    entry.full_name = cpak_path;
    
    return true;
}

// Helper function to calculate CRC32 for a file in the pak
static uint32_t calculate_file_crc32(const std::string& filename, bool& error) {
    // CRC32 for a file inside the pak
    error = false;
    try {
        verbose_log("Calculating CRC32 for file in pak: %s", filename.c_str());
        CPakFile file(filename, O_RDONLY);
        if (!file.isValid()) {
            verbose_log("File is not valid: %s", filename.c_str());
            error = true;
            return 0;
        }
        uint32_t crc = 0xffffffffL;
        const size_t BUFFER_SIZE = 4096;
        uint8_t buffer[BUFFER_SIZE];
        size_t bytes_read;
        size_t total_bytes = 0;
        while ((bytes_read = file.read(buffer, BUFFER_SIZE)) > 0) {
            for (size_t i = 0; i < bytes_read; i++) {
                crc = crc_table[(crc ^ buffer[i]) & 0xff] ^ (crc >> 8);
            }
            total_bytes += bytes_read;
        }
        verbose_log("CRC32 calculation completed: %zu bytes processed", total_bytes);
        return crc ^ 0xffffffffL;
    } catch (const std::exception& e) {
        verbose_log("CRC32 calculation failed for %s: %s", filename.c_str(), e.what());
        error = true;
        return 0;
    }
}

// Helper to calculate CRC32 of any file on disk (not inside pak)
static uint32_t calculate_disk_crc32(const char *filepath, bool &error) {
    error = false;
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        error = true;
        return 0;
    }
    uint32_t crc = 0xffffffffL;
    const size_t BUFFER_SIZE = 4096;
    uint8_t buffer[BUFFER_SIZE];
    size_t bytes_read;
    size_t total_bytes = 0;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, f)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            crc = crc_table[(crc ^ buffer[i]) & 0xff] ^ (crc >> 8);
        }
        total_bytes += bytes_read;
    }
    fclose(f);
    verbose_log("Disk CRC32 calculation completed: %zu bytes processed", total_bytes);
    return crc ^ 0xffffffffL;
}

// Helper function to escape a string for JSON output
static std::string json_escape(const std::string& str) {
    std::string escaped_str;
    escaped_str.reserve(str.length());
    for (char c : str) {
        switch (c) {
            case '"':  escaped_str += "\\\""; break;
            case '\\': escaped_str += "\\\\"; break;
            case '\b': escaped_str += "\\b"; break;
            case '\f': escaped_str += "\\f"; break;
            case '\n': escaped_str += "\\n"; break;
            case '\r': escaped_str += "\\r"; break;
            case '\t': escaped_str += "\\t"; break;
            default:
                // Filter out control characters, including the \x01 used for <NUL>
                if (static_cast<unsigned char>(c) < 32) {
                    // Skip control characters
                } else {
                    escaped_str += c;
                }
                break;
        }
    }
    return escaped_str;
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

// Helper to collect file info from a pak, with optional CRC calculation
static void collect_pak_files(CPakFilesystem &pak, std::vector<file_entry_t> &files, bool calc_crc, char *patterns[], int num_patterns) {
    pak.for_each_file([&](const char* filename, const dir_t& dir) -> bool {
        file_entry_t entry;
        entry.size = dir.d_size;
        if (parse_cpak_path(std::string(filename), entry)) {
            // Check if file matches any patterns
            bool should_include = false;
            if (!patterns || num_patterns == 0) {
                should_include = true;
            } else {
                for (int i = 0; i < num_patterns; i++) {
                    if (fnmatch(patterns[i], entry.full_name.c_str()) || fnmatch(patterns[i], filename)) {
                        should_include = true;
                        break;
                    }
                }
            }
            if (should_include) {
                if (calc_crc) {
                    bool crc_error;
                    entry.crc32_value = calculate_file_crc32(std::string(filename), crc_error);
                    entry.crc32_error = crc_error;
                } else {
                    entry.crc32_value = 0;
                    entry.crc32_error = false;
                }
                files.push_back(entry);
            }
        }
        return true;
    });
}

int cmd_list(const char *pak_file, char *patterns[], int num_patterns) {
    verbose_log("Listing contents of %s", pak_file);
    if (!file_exists(pak_file)) {
        fatal_error("File not found: %s", pak_file);
    }
    // Open the pak file and mount filesystem
    try {
        CPakFilesystem pak(pak_file, true, g_global_opts.skip_header_bytes);
        std::vector<file_entry_t> files;
        collect_pak_files(pak, files, g_command_opts.show_crc, patterns, num_patterns);
        // Sort files if requested
        if (!files.empty()) {
            std::sort(files.begin(), files.end(), [](const file_entry_t& a, const file_entry_t& b) {
                return compare_files(a, b, g_command_opts.sort_by, g_command_opts.reverse_sort);
            });
        }
        
        // Display files
        if (g_command_opts.json_output) {
            // JSON output
            printf("[\n");
            for (size_t i = 0; i < files.size(); ++i) {
                const auto& file = files[i];
                printf("  {\n");
                printf("    \"game_code\": \"%s\",\n", json_escape(file.game_code).c_str());
                printf("    \"pub_code\": \"%s\",\n", json_escape(file.pub_code).c_str());
                printf("    \"filename\": \"%s\",\n", json_escape(file.filename).c_str());
                printf("    \"extension\": \"%s\",\n", json_escape(file.extension).c_str());
                printf("    \"full_name\": \"%s\",\n", json_escape(file.full_name).c_str());
                if (file.size < 0) {
                    printf("    \"size\": null");
                } else {
                    printf("    \"size\": %d", file.size);
                }

                if (g_command_opts.show_crc) {
                    printf(",\n");
                    if (file.crc32_error) {
                        printf("    \"crc32\": null\n");
                    } else {
                        printf("    \"crc32\": \"%08X\"\n", file.crc32_value);
                    }
                } else {
                    printf("\n");
                }
                printf("  }");
                if (i < files.size() - 1) {
                    printf(",");
                }
                printf("\n");
            }
            printf("]\n");
        } else if (g_command_opts.long_format) {
            // Long format with dynamic column widths
            
            // Calculate dynamic column widths based on content
            size_t max_game_width = 4;  // Minimum width for "Game" header
            size_t max_pub_width = 3;   // Minimum width for "Pub" header  
            size_t max_filename_width = 8; // Minimum width for "Filename" header
            size_t max_ext_width = 3;   // Minimum width for "Ext" header
            size_t max_size_width = 4;  // Minimum width for "Size" header
            
            // First pass: calculate maximum widths
            for (const auto& file : files) {
                max_game_width = std::max(max_game_width, visual_width(file.game_code));
                max_pub_width = std::max(max_pub_width, visual_width(file.pub_code));
                max_filename_width = std::max(max_filename_width, visual_width(display_filename(file.filename)));
                max_ext_width = std::max(max_ext_width, visual_width(file.extension));
                
                std::string size_str = format_size(file.size, g_command_opts.human_readable);
                max_size_width = std::max(max_size_width, size_str.length());
            }
            
            // Add some padding
            max_game_width += 2;
            max_pub_width += 2;
            max_filename_width += 2;
            max_ext_width += 2;
            max_size_width += 2;
            
            // Print headers
            if (g_command_opts.show_crc) {
                printf("%-*s%-*s%-*s%-*s%-*s%s\n",
                       (int)max_game_width, "Game",
                       (int)max_pub_width, "Pub",
                       (int)max_filename_width, "Filename", 
                       (int)max_ext_width, "Ext",
                       (int)max_size_width, "Size",
                       "CRC32");
                       
                // Print separator line
                printf("%s%s%s%s%s%s\n",
                       std::string(max_game_width, '-').c_str(),
                       std::string(max_pub_width, '-').c_str(),
                       std::string(max_filename_width, '-').c_str(),
                       std::string(max_ext_width, '-').c_str(),
                       std::string(max_size_width, '-').c_str(),
                       "--------");
            } else {
                printf("%-*s%-*s%-*s%-*s%s\n",
                       (int)max_game_width, "Game",
                       (int)max_pub_width, "Pub",
                       (int)max_filename_width, "Filename",
                       (int)max_ext_width, "Ext",
                       "Size");
                       
                // Print separator line
                printf("%s%s%s%s%s\n",
                       std::string(max_game_width, '-').c_str(),
                       std::string(max_pub_width, '-').c_str(),
                       std::string(max_filename_width, '-').c_str(),
                       std::string(max_ext_width, '-').c_str(),
                       std::string(max_size_width, '-').c_str());
            }
            
            // Print file entries
            for (const auto& file : files) {
                std::string size_str = format_size(file.size, g_command_opts.human_readable);
                
                if (g_command_opts.show_crc) {
                    if (file.crc32_error) {
                        printf("%-*s%-*s%-*s%-*s%-*s%s\n",
                               (int)max_game_width, file.game_code.c_str(),
                               (int)max_pub_width, file.pub_code.c_str(),
                               (int)max_filename_width, display_filename(file.filename).c_str(),
                               (int)max_ext_width, file.extension.c_str(),
                               (int)max_size_width, size_str.c_str(),
                               "<error>");
                    } else {
                        char crc_str[16];
                        snprintf(crc_str, sizeof(crc_str), "%08X", file.crc32_value);
                        printf("%-*s%-*s%-*s%-*s%-*s%s\n",
                               (int)max_game_width, file.game_code.c_str(),
                               (int)max_pub_width, file.pub_code.c_str(),
                               (int)max_filename_width, display_filename(file.filename).c_str(),
                               (int)max_ext_width, file.extension.c_str(),
                               (int)max_size_width, size_str.c_str(),
                               crc_str);
                    }
                } else {
                    printf("%-*s%-*s%-*s%-*s%s\n",
                           (int)max_game_width, file.game_code.c_str(),
                           (int)max_pub_width, file.pub_code.c_str(),
                           (int)max_filename_width, display_filename(file.filename).c_str(),
                           (int)max_ext_width, file.extension.c_str(),
                           size_str.c_str());
                }
            }
        } else {
            // Simple format - one file per line
            for (const auto& file : files) {
                printf("%s\n", display_filename(file.full_name).c_str());
            }
        }
        
        // Summary
        if (g_global_opts.verbose || files.empty()) {
            printf("\nFound %zu file%s\n", files.size(), files.size() == 1 ? "" : "s");
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        fatal_error("Cannot open Controller Pak file '%s': %s", pak_file, e.what());
        return -1;
    }
}

int cmd_extract(const char *pak_file, char *patterns[], int num_patterns) {
    verbose_log( "Extracting from %s", pak_file);
    
    if (!file_exists(pak_file)) {
        fatal_error("File not found: %s", pak_file);
    }
    
    // Open the pak file and mount filesystem
    try {
        CPakFilesystem pak(pak_file, true, g_global_opts.skip_header_bytes);
        
        verbose_log( "Note: Controller Pak files are stored in 256-byte blocks with random padding");
        
        int files_extracted = 0;
        
        // Use the new for_each_file method
        pak.for_each_file([&](const char* filename, const dir_t& dir) -> bool {
            // The filename is already in the correct format (GAME.PB-file.ext)
            std::string output_filename = filename;
            
            bool should_extract = false;
            
            if (num_patterns == 0) {
                // No patterns specified, extract all files
                verbose_log( "Found file: '%s'", filename);
                should_extract = true;
            } else {
                // Check if file matches any of the patterns (check both cpak name and output name)
                for (int i = 0; i < num_patterns; i++) {
                    if (fnmatch(patterns[i], filename) || 
                        fnmatch(patterns[i], output_filename.c_str())) {
                        verbose_log( "File '%s' (output: '%s') matches pattern '%s'", 
                                  filename, output_filename.c_str(), patterns[i]);
                        should_extract = true;
                        break;
                    }
                }
                if (!should_extract) {
                    verbose_log( "File '%s' (output: '%s') does not match any pattern", 
                              filename, output_filename.c_str());
                }
            }
            
            if (should_extract) {
                files_extracted += extract_file(filename);
            }
            
            return true; // Continue iteration
        });
        
        verbose_log( "Summary: %d files extracted", files_extracted);
        
        return 0;
        
    } catch (const std::exception& e) {
        fatal_error("Cannot open Controller Pak file '%s': %s", pak_file, e.what());
        return -1;
    }
}

int cmd_add(const char *pak_file, char *files[], int num_files) {
    verbose_log( "Adding files to %s", pak_file);
    
    if (!file_exists(pak_file) && !g_command_opts.create_pak) {
        fatal_error("File not found: %s (use --create to create new pak)", pak_file);
    }
    
    // Create new pak file if requested
    if (g_command_opts.create_pak && !file_exists(pak_file)) {
        verbose_log( "Creating new pak file with %d KB", g_command_opts.pak_size);
        
        // Determine number of banks from pak_size
        int num_banks = (g_command_opts.pak_size * 1024) / BANK_SIZE;
        if (num_banks <= 0) num_banks = 1;
        
        try {
            auto pak = CPakFilesystem::create(pak_file, num_banks, g_global_opts.force);
            if (!pak) {
                fatal_error("Cannot create Controller Pak file '%s': %s", pak_file, strerror(errno));
            }
            
            verbose_log( "Controller Pak file created and formatted");
            
        } catch (...) {
            unlink(pak_file);
            fatal_error("Failed to create Controller Pak image: unknown error");
        }
    }
    
    // Open the pak file and mount filesystem
    try {
        CPakFilesystem pak(pak_file, true, g_global_opts.skip_header_bytes);
        
        int files_added = 0;
        int files_updated = 0;
        int files_failed = 0;
        
        // Process each input file
        for (int i = 0; i < num_files; i++) {
            int result = add_file(files[i]);
            if (result == 1) {
                files_added++;
            } else if (result == 2) {
                files_updated++;
            } else {
                files_failed++;
            }
            // result == 0 means error (already handled by add_file)
        }
        
        verbose_log( "Summary: %d files added, %d files updated, %d files failed", files_added, files_updated, files_failed);
        
        return files_failed > 0 ? 1 : 0;
        
    } catch (const std::exception& e) {
        fatal_error("Cannot open Controller Pak file '%s': %s", pak_file, e.what());
        return -1;
    }
}

int cmd_delete(const char *pak_file, char *patterns[], int num_patterns) {
    verbose_log( "Deleting from %s", pak_file);
    
    if (!file_exists(pak_file)) {
        fatal_error("File not found: %s", pak_file);
    }
    
    // Open the pak file and mount filesystem
    try {
        CPakFilesystem pak(pak_file, true, g_global_opts.skip_header_bytes);
        
        std::vector<std::string> files_to_delete;
        
        // First, collect all files that match the patterns
        pak.for_each_file([&](const char* filename, const dir_t& dir) -> bool {
            // The filename is already in the correct format (GAME.PB-file.ext)
            std::string output_filename = filename;
            
            bool should_delete = false;
            
            // Check if file matches any of the patterns
            for (int i = 0; i < num_patterns; i++) {
                if (fnmatch(patterns[i], filename) || 
                    fnmatch(patterns[i], output_filename.c_str())) {
                    verbose_log( "File '%s' matches pattern '%s'", 
                              filename, patterns[i]);
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
            if (g_command_opts.interactive) {
                printf("Delete '%s'? [y/N] ", filename.c_str());
                fflush(stdout);
                
                char response[16];
                if (fgets(response, sizeof(response), stdin)) {
                    if (response[0] != 'y' && response[0] != 'Y') {
                        delete_file = false;
                        verbose_log( "Skipping '%s'", filename.c_str());
                    }
                } else {
                    delete_file = false;
                }
            }
            
            if (delete_file && !g_global_opts.dry_run) {
                verbose_log( "Deleting '%s'", filename.c_str());
                
                if (cpak_file_unlink(filename.c_str()) == 0) {
                    files_deleted++;
                    if (!g_global_opts.verbose && !g_command_opts.interactive) {
                        printf("Deleted: %s\n", filename.c_str());
                    }
                } else {
                    files_failed++;
                    warning("Failed to delete '%s': %s", filename.c_str(), strerror(errno));
                }
            } else if (delete_file && g_global_opts.dry_run) {
                printf("Would delete: %s\n", filename.c_str());
                files_deleted++; // Count as "would be deleted"
            }
        }
        
        // Summary
        if (g_global_opts.dry_run) {
            verbose_log( "Dry run: %d files would be deleted", files_deleted);
        } else {
            verbose_log( "Summary: %d files deleted, %d failures", files_deleted, files_failed);
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

int cmd_info(const char *pak_file) {
    verbose_log("Getting info for %s", pak_file);

    if (!file_exists(pak_file)) {
        fatal_error("File not found: %s", pak_file);
    }

    try {
        CPakFilesystem pak(pak_file, true, g_global_opts.skip_header_bytes);

        cpakfs_stats_t stats;
        int err = cpakfs_get_stats(JOYPAD_PORT_1, &stats);
        if (err < 0) {
            fatal_error("Cannot get stats for Controller Pak file '%s'", pak_file);
            return -1;
        }

        // Compila la struttura dati
        InfoPakStats info;
        info.pak_file = pak_file;
        info.num_banks = stats.num_banks;
        info.pages_used = stats.pages.used;
        info.pages_total = stats.pages.total;
        info.notes_used = stats.notes.used;
        info.notes_total = stats.notes.total;
        info.pages_percent = (stats.pages.total > 0) ? (100.0 * stats.pages.used / stats.pages.total) : 0.0;
        info.notes_percent = (stats.notes.total > 0) ? (100.0 * stats.notes.used / stats.notes.total) : 0.0;
        int header_bytes = pak.getHeaderBytes();

        // Calcola CRC32 del file pak (sul disco, non come file nel pak)
        bool crc_error = false;
        uint32_t pak_crc32 = calculate_disk_crc32(pak_file, crc_error);

        // Raccolta delle issue da fsck
        FsckContext fsck_ctx;
        fsck_ctx.issues = &info.fsck_issues;
        cpakfs_fsck(JOYPAD_PORT_1, false, fsck_report_static, &fsck_ctx);

        // Scan FAT chain and extract spans for each note, and track per-bank usage
        struct PageSpan { uint16_t start; uint16_t end; };
        struct file_entry_ext_t : public file_entry_t {
            std::vector<PageSpan> spans;
        };
        std::vector<file_entry_ext_t> files_ext;
        // Per-bank stats
        struct BankStats {
            int used_pages = 0;
            int used_notes = 0;
        };
        std::vector<BankStats> banks(info.num_banks);
        // For each note, keep a set of banks touched
        pak.for_each_file([&](const char* filename, const dir_t& dir) -> bool {
            file_entry_ext_t entry;
            entry.size = dir.d_size;
            parse_cpak_path(std::string(filename), entry);
            entry.full_name = filename;
            // Calculate CRC32 for the note
            bool crc_error = false;
            entry.crc32_value = calculate_file_crc32(std::string(filename), crc_error);
            entry.crc32_error = crc_error;
            // Open file and scan FAT chain using CPakFile and il nuovo metodo ioctl
            std::vector<uint16_t> chain;
            std::set<int> banks_touched;
            try {
                CPakFile file(filename, O_RDONLY);
                int pos = 0;
                std::set<uint16_t> seen_pages;
                while (pos < dir.d_size) {
                    uint16_t bankpage = 0;
                    if (file.ioctl(IOCPAKFS_GET_PAGE, &bankpage) < 0) break;
                    if (seen_pages.count(bankpage)) {
                        throw std::runtime_error("Duplicate page detected in FAT chain");
                    }
                    seen_pages.insert(bankpage);
                    chain.push_back(bankpage);
                    // Track per-bank usage
                    int bank = bankpage >> 8;
                    int page = bankpage & 0xFF;
                    if (bank >= 0 && bank < info.num_banks && page > 0 && page < 128) {
                        banks[bank].used_pages++;
                        banks_touched.insert(bank);
                    }
                    pos += 256;
                    if (file.seek(pos, SEEK_SET) < 0) break;
                }
            } catch (const std::exception& e) {
                chain.clear();
            } catch (...) {
                chain.clear();
            }
            // Build spans: group only consecutive pages
            if (!chain.empty()) {
                uint16_t span_start = chain[0], span_end = chain[0];
                for (size_t i = 1; i < chain.size(); ++i) {
                    if (chain[i] == span_end + 1) {
                        span_end = chain[i];
                    } else {
                        entry.spans.push_back({span_start, span_end});
                        span_start = span_end = chain[i];
                    }
                }
                entry.spans.push_back({span_start, span_end});
            }
            // For each bank touched by this note, increment used_notes
            for (int b : banks_touched) banks[b].used_notes++;
            files_ext.push_back(entry);
            return true;
        });
        // Output
        if (g_command_opts.json_output) {
            printf("{\n");
            printf("  \"pak_file\": \"%s\",\n", json_escape(info.pak_file).c_str());
            printf("  \"pak_crc32\": ");
            if (crc_error) {
                printf("null,\n");
            } else {
                printf("\"0x%08X\",\n", pak_crc32);
            }
            printf("  \"header_bytes\": %d,\n", header_bytes);
            printf("  \"banks\": %d,\n", info.num_banks);
            printf("  \"pages\": { \"used\": %d, \"total\": %d, \"percent\": %.2f },\n", info.pages_used, info.pages_total, info.pages_percent);
            printf("  \"notes\": { \"used\": %d, \"total\": %d, \"percent\": %.2f },\n", info.notes_used, info.notes_total, info.notes_percent);
            printf("  \"bank_stats\": [\n");
            for (int b = 0; b < info.num_banks; ++b) {
                int total_pages = 128 - 1; // page 0 reserved
                double percent = (total_pages > 0) ? (100.0 * banks[b].used_pages / total_pages) : 0.0;
                printf("    { \"bank\": %d, \"used_pages\": %d, \"percent\": %.2f, \"used_notes\": %d }", b, banks[b].used_pages, percent, banks[b].used_notes);
                if (b < info.num_banks - 1) printf(",");
                printf("\n");
            }
            printf("  ],\n");
            printf("  \"notes\": [\n");
            for (size_t i = 0; i < files_ext.size(); ++i) {
                const auto& file = files_ext[i];
                printf("    {\n");
                printf("      \"game_code\": \"%s\",\n", json_escape(file.game_code).c_str());
                printf("      \"pub_code\": \"%s\",\n", json_escape(file.pub_code).c_str());
                printf("      \"filename\": \"%s\",\n", json_escape(file.filename).c_str());
                printf("      \"extension\": \"%s\",\n", json_escape(file.extension).c_str());
                printf("      \"full_name\": \"%s\",\n", json_escape(file.full_name).c_str());
                printf("      \"size\": %d,\n", file.size);
                printf("      \"crc32\": ");
                if (file.crc32_error) {
                    printf("null,\n");
                } else {
                    printf("\"0x%08X\",\n", file.crc32_value);
                }
                printf("      \"spans\": [");
                for (size_t j = 0; j < file.spans.size(); ++j) {
                    printf("[%u,%u]", file.spans[j].start, file.spans[j].end);
                    if (j < file.spans.size() - 1) printf(",");
                }
                printf("]\n");
                printf("    }");
                if (i < files_ext.size() - 1) printf(",");
                printf("\n");
            }
            printf("  ],\n");
            printf("  \"fsck_issues\": [\n");
            for (size_t i = 0; i < info.fsck_issues.size(); ++i) {
                const auto &iss = info.fsck_issues[i];
                printf("    { \"code\": %d, \"level\": %d, \"message\": \"%s\" }", iss.code, iss.level, json_escape(iss.message).c_str());
                if (i < info.fsck_issues.size() - 1) printf(",");
                printf("\n");
            }
            printf("  ]\n");
            printf("}\n");
        } else {
            printf("Pak file: %s\n", info.pak_file.c_str());
            printf("Header bytes: %d\n", header_bytes);
            printf("Banks: %d\n", info.num_banks);
            printf("Pages: %d used / %d total (%.2f%%)\n", info.pages_used, info.pages_total, info.pages_percent);
            printf("Notes: %d used / %d total (%.2f%%)\n", info.notes_used, info.notes_total, info.notes_percent);
            printf("\nBank statistics:\n");
            for (int b = 0; b < info.num_banks; ++b) {
                int total_pages = 128 - 1; // page 0 reserved
                double percent = (total_pages > 0) ? (100.0 * banks[b].used_pages / total_pages) : 0.0;
                printf("  Bank %d: %d pages used (%.2f%%), %d notes\n", b, banks[b].used_pages, percent, banks[b].used_notes);
            }
            if (!files_ext.empty()) {
                printf("\nNotes:\n");
                for (const auto& file : files_ext) {
                    printf("  %s.%s-%s", file.game_code.c_str(), file.pub_code.c_str(), display_filename(file.filename).c_str());
                    if (!file.extension.empty()) {
                        printf(".%s", file.extension.c_str());
                    }
                    printf(" (%d bytes)", file.size);
                    if (!file.crc32_error) {
                        printf("     CRC32: %08X", file.crc32_value);
                    } else {
                        printf("     CRC32: <error>");
                    }
                    printf("\n");
                    if (!file.spans.empty()) {
                        printf("    FAT: ");
                        for (size_t j = 0; j < file.spans.size(); ++j) {
                            uint16_t s = file.spans[j].start, e = file.spans[j].end;
                            printf("%02x:%02x-%02x:%02x", s>>8, s&0xFF, e>>8, e&0xFF);
                            if (j < file.spans.size() - 1) printf(", ");
                        }
                        printf("\n");
                    }
                }
            } else {
                printf("\nNo notes found in pak.\n");
            }
            printf("\nFSCK issues: %zu\n", info.fsck_issues.size());
            for (const auto &iss : info.fsck_issues) {
                printf("  [%d] Level %d: %s\n", iss.code, iss.level, iss.message.c_str());
            }
        }

        return 0;
    } catch (const std::exception& e) {
        fatal_error("Cannot open Controller Pak file '%s': %s", pak_file, e.what());
        return -1;
    }
}

int g_fsck_nissues;

static void fsck_report(void *ctx, cpakfs_issue_t issue, cpakfs_issue_level_t level, const char *fmt, ...) {
    (void)ctx; // Unused parameter
    if ((int)level < g_command_opts.report_level) return;

    const char *lvl = (level == CPAKFS_LEVEL_INFO) ? "INFO" : (level == CPAKFS_LEVEL_WARNING ? "WARN" : "ERROR");
    printf("[fsck %s] ", lvl);
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
    printf("\n");
    g_fsck_nissues++;
}

int cmd_test(const char *pak_file) {
    verbose_log( "Testing %s", pak_file);
    
    if (!file_exists(pak_file)) {
        fatal_error("File not found: %s", pak_file);
    }

    try {
        CPakFilesystem pak(pak_file, false, g_global_opts.skip_header_bytes); // Don't auto-mount for testing
        
        verbose_log( "Running fsck on %s (%d banks)", pak_file, pak.getNumBanks());

        // Raccolta delle issue da fsck
        std::vector<InfoPakStats::Issue> issues;
        FsckContext fsck_ctx;
        fsck_ctx.issues = &issues;
        int err = cpakfs_fsck(JOYPAD_PORT_1, g_command_opts.fix_errors,
            g_command_opts.json_output ? fsck_report_json_static : fsck_report,
            g_command_opts.json_output ? &fsck_ctx : nullptr);
        if (err < 0) {
            fatal_error("Failed to test Controller Pak image: %s", strerror(errno));
        }

        if (g_command_opts.json_output) {
            // Output JSON
            printf("{\n");
            printf("  \"pak_file\": \"%s\",\n", json_escape(pak_file).c_str());
            printf("  \"fsck_issues\": [\n");
            for (size_t i = 0; i < issues.size(); ++i) {
                const auto &iss = issues[i];
                printf("    { \"code\": %d, \"level\": %d, \"message\": \"%s\" }", iss.code, iss.level, json_escape(iss.message).c_str());
                if (i < issues.size() - 1) printf(",");
                printf("\n");
            }
            printf("  ]\n");
            printf("}\n");
        } else {
            if (issues.empty()) {
                printf("No issues found\n");
            } else if (g_command_opts.fix_errors) {
                printf("Fixed %zu issue%s\n", issues.size(), issues.size()==1?"":"s");
            } else {
                printf("Found %zu issue%s\n", issues.size(), issues.size()==1?"":"s");
                for (const auto &iss : issues) {
                    printf("  [%d] Level %d: %s\n", iss.code, iss.level, iss.message.c_str());
                }
                if (!issues.empty()) return -1;
            }
        }

        return 0;
        
    } catch (const std::exception& e) {
        fatal_error("Cannot open file '%s': %s", pak_file, e.what());
        return -1;
    }
}

int cmd_format(const char *pak_file) {
    verbose_log( "Formatting %s with %d banks", pak_file, g_command_opts.num_banks);
    
    if (file_exists(pak_file) && !g_global_opts.force) {
        fatal_error("File exists: %s (use --force to overwrite)", pak_file);
    }
    
    try {
        // Use factory method to create and format the pak file
        auto pak = CPakFilesystem::create(pak_file, g_command_opts.num_banks, g_global_opts.force);
        if (!pak) {
            fatal_error("Cannot create Controller Pak file '%s': %s", pak_file, strerror(errno));
        }
        
        size_t total_size = g_command_opts.num_banks * BANK_SIZE;
        verbose_log( "Controller Pak image formatted successfully: %zu bytes", total_size);
        
        return 0;
        
    } catch (...) {
        unlink(pak_file); // Clean up on exception
        fatal_error("Failed to format Controller Pak image: unknown error");
        return -1;
    }
}

int cmd_convert(const char *input_file, const char *output_file) {
    verbose_log( "Converting %s to %s", input_file, output_file);
    
    if (!file_exists(input_file)) {
        fatal_error("Input file not found: %s", input_file);
    }
    
    // TODO: Implement actual convert functionality
    printf("CONVERT command not implemented yet\n");
    printf("Input: %s\n", input_file);
    printf("Output: %s\n", output_file);
    printf("From format: %s\n", g_command_opts.from_format ? g_command_opts.from_format : "auto-detect");
    printf("To format: %s\n", g_command_opts.to_format ? g_command_opts.to_format : "auto-detect");
    
    return 0;
}

int cmd_compare(const char *pak_file1, const char *pak_file2) {
    verbose_log( "Comparing %s and %s", pak_file1, pak_file2);
    
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
    printf("Brief: %s\n", g_command_opts.brief ? "yes" : "no");
    printf("Summary: %s\n", g_command_opts.summary ? "yes" : "no");
    
    return 0;
}

//
// HELPER FUNCTIONS FOR ADD/EXTRACT
//

static int add_file(const char *input_file) {
    verbose_log( "Processing file: %s", input_file);
    
    if (!file_exists(input_file)) {
        fatal_error("File not found: %s", input_file);
    }
    
    // Extract just the filename from the path
    const char *basename = strrchr(input_file, '/');
    basename = basename ? basename + 1 : input_file;
    
    char cpak_path[256];
    cpakfs_path_t parsed_path;
    const char *error_pos = NULL;
    
    // Check if the filename already has a gamecode/pubcode with a very simple logic:
    // check if there is at least a dash, and at least a dot before the dash.
    bool has_gamecode = true;
    const char *dash = strchr(basename, '-');
    const char *dot = strchr(basename, '.');
    if (!dash || !dot || dash < dot)
        has_gamecode = false; // No valid gamecode/pubcode format found

    if (has_gamecode) {
        // It's already in cpak format, use as-is
        strcpy(cpak_path, basename);
    } else {
        // Need to add default game/publisher code
        const char *default_gamecode = g_command_opts.gamecode ? g_command_opts.gamecode : "DRAG.ON";
        
        // Parse default gamecode to extract game and publisher parts
        std::string game, pub;
        if (strlen(default_gamecode) >= 7 && default_gamecode[4] == '.') {
            game = std::string(default_gamecode, 4);
            pub = std::string(default_gamecode + 5, 2);
        } else {
            // Fallback to DRAG.ON if format is invalid
            game = "DRAG";
            pub = "ON";
        }
        
        // Construct the full cpak path: GAME.PUB-filename
        std::string full_path = game + "." + pub + "-" + basename;
        strcpy(cpak_path, full_path.c_str());
    }
    
    // Now validate the final path with cpakfs_path_parse
    cpakfs_parse_err_t err = cpakfs_path_parse(cpak_path, &parsed_path, &error_pos);
    if (err != CPAKFS_PARSE_OK) {
        // Provide user-friendly error message based on the parse error
        const char *error_msg = "Invalid filename";
        
        switch (err) {
            case CPAKFS_PARSE_ERR_GAMECODE_TOO_SHORT:
                error_msg = "Game code too short (min 4 characters)";
                break;
            case CPAKFS_PARSE_ERR_GAMECODE_TOO_LONG:
                error_msg = "Game code too long (max 4 characters, or 8 if hex)";
                break;
            case CPAKFS_PARSE_ERR_GAMECODE_CHAR:
                error_msg = "Game code contains invalid characters";
                break;
            case CPAKFS_PARSE_ERR_PUBCODE_TOO_SHORT:
                error_msg = "Publisher code too short (min 2 characters)";
                break;
            case CPAKFS_PARSE_ERR_PUBCODE_TOO_LONG:
                error_msg = "Publisher code too long (max 2 characters, or 4 if hex)";
                break;
            case CPAKFS_PARSE_ERR_PUBCODE_CHAR:
                error_msg = "Publisher code contains invalid characters";
                break;
            case CPAKFS_PARSE_ERR_FILENAME_TOO_SHORT:
                error_msg = "Filename cannot be empty";
                break;
            case CPAKFS_PARSE_ERR_FILENAME_TOO_LONG:
                error_msg = "Filename too long (max 16 characters before extension)";
                break;
            case CPAKFS_PARSE_ERR_FILENAME_CHAR:
                error_msg = "Filename contains unsupported characters";
                break;
            case CPAKFS_PARSE_ERR_EXTENSION_TOO_LONG:
                error_msg = "Extension too long (max 4 characters)";
                break;
            case CPAKFS_PARSE_ERR_EXTENSION_CHAR:
                error_msg = "Extension contains unsupported characters";
                break;
            default:
                error_msg = "Invalid filename format";
                break;
        }
        
        // Create visual error indicator showing the position
        std::string error_indicator = std::string(error_pos - cpak_path, ' ') + "^";        
        fatal_error("%s: '%s'\n%*s%s", error_msg, cpak_path, 
                    (int)strlen(error_msg) + 3 + 7, "", error_indicator.c_str());
    }
    
    verbose_log( "Target cpak path: %s", cpak_path);
    
    // Check if file already exists
    bool is_update = false;
    try {
        CPakFile existing(cpak_path, O_RDONLY);
        
        if (g_command_opts.update_only) {
            verbose_log( "File already exists in pak, updating: %s", cpak_path);
            is_update = true;
        } else {
            warning("File '%s' already exists in pak. Use --update to overwrite.", cpak_path);
            return 0;
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
    
    bool file_created = false;
    try {
        // Open destination file in pak using our C++ wrapper
        // Use O_TRUNC to ensure existing files are properly truncated
        int flags = O_WRONLY | O_CREAT;
        if (is_update) {
            flags |= O_TRUNC;  // Truncate existing files to avoid leftover data
        }
        CPakFile dst(cpak_path, flags);
        file_created = true;
        
        verbose_log( "Copying %ld bytes from %s to %s", file_size, input_file, cpak_path);
        
        // Copy data using configurable buffer size with RAII
        size_t buffer_size = g_command_opts.debug_bufsize;
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
                verbose_log( "Updated: %s -> %s (%zu bytes)", input_file, cpak_path, bytes_copied);
                return 2; // Indicate update
            } else {
                verbose_log( "Added: %s -> %s (%zu bytes)", input_file, cpak_path, bytes_copied);
                return 1; // Indicate addition
            }
        } else {
            warning("Incomplete copy for file '%s': %zu of %ld bytes", input_file, bytes_copied, file_size);
            return 0;
        }
        
    } catch (const std::exception& e) {
        fclose(src);
        
        if (errno == ENOSPC) {
            warning("Cannot add file '%s': No space left in filesystem", input_file);
        } else if (errno == EMFILE) {
            warning("Cannot add file '%s': Too many files (maximum 16 notes per pak)", input_file);
        } else {
            warning("Cannot add file '%s': %s", input_file, e.what());
        }

        if (file_created) {
            if (!g_command_opts.allow_partial) {
                if (cpak_file_unlink(cpak_path) == 0) {
                    verbose_log("Removed partially written file: %s", cpak_path);
                } else {
                    warning("Failed to clean up partial file '%s': %s", cpak_path, strerror(errno));
                    return 1;
                }
            } else {
                warning("Partial file '%s' kept because --partial is enabled", cpak_path);
            }
        }
        return 0;
    }
}

static int extract_file(const char *cpak_path) {
    try {
        // The cpak path is already in the correct format (GAME.PB-filename.ext)
        std::string output_filename = cpak_path;
        
        verbose_log( "Extracting %s", output_filename.c_str());
        
        // Check if output file exists
        if (file_exists(output_filename.c_str()) && !g_command_opts.overwrite) {
            warning("File exists, skipping: %s (use --overwrite to force)", output_filename.c_str());
            return 0;
        }
        
        // Open source file in pak using C++ wrapper
        CPakFile src_file(cpak_path, O_RDONLY);
        
        // Note: cpakfs doesn't store real file size, files are always padded to 256-byte blocks
        verbose_log( "Note: cpakfs files are padded to 256-byte blocks");
        
        // Open destination file
        FILE *dst = fopen(output_filename.c_str(), "wb");
        if (!dst) {
            warning("Cannot create output file '%s': %s", output_filename.c_str(), strerror(errno));
            return 0;
        }
        
        // Copy file content (will be padded to 256-byte boundary)
        size_t buffer_size = g_command_opts.debug_bufsize;
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
        verbose_log( "Extracted: %s (%zu bytes)", output_filename.c_str(), total_bytes);
        return 1;
        
    } catch (const std::exception& e) {
        warning("Cannot extract file '%s': %s", cpak_path, e.what());
        return 0;
    }
}
