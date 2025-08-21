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
#include <strings.h>

#include "cpaktool.h"
#include "../common/polyfill.h"

// Global variables for options
global_options_t g_global_opts;
command_options_t g_command_opts = {0};

// Forward declarations
static void print_usage(const char *program_name);
static void print_command_usage(const char *program_name, command_t cmd);
static void print_version(void);
static command_t parse_command(const char *cmd_str);
static bool handle_global_option(const char *arg,  const char *program_name, command_t cmd, bool before_command, int argc, char *argv[], int *arg_idx);
static bool handle_global_option_with_value(const char *arg, const char *provided_value, const char *program_name, command_t cmd, bool before_command, int argc, char *argv[], int *arg_idx);
static int parse_global_options(int argc, char *argv[], int *start_idx);
static int parse_command_options(command_t cmd, int argc, char *argv[], int start_idx);
static int execute_command(command_t cmd, int argc, char *argv[], int start_idx);

//
// GAMECODE PARSING FUNCTIONS
//

/**
 * @brief Check if a character is a valid hex digit
 */
static bool is_hex_char(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

/**
 * @brief Check if a character is valid for gamecode/pubcode (alphanumeric)
 */
static bool is_valid_gamecode_char(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/**
 * @brief Parse and validate gamecode format
 * 
 * Supports formats:
 * - DRAG.ON (ASCII gamecode, ASCII pubcode)
 * - DEADBEEF.FADE (hex gamecode, hex pubcode) 
 * - DRAG.FADE (ASCII gamecode, hex pubcode)
 * - DEADBEEF.ON (hex gamecode, ASCII pubcode)
 * 
 * Returns allocated string with the validated gamecode in the original format, or NULL on error.
 */
static char* parse_and_validate_gamecode(const char *input)
{
    if (!input) return NULL;
    
    // Find the dot separator
    const char *dot = strchr(input, '.');
    if (!dot) {
        fatal_error("Invalid gamecode format: missing '.' separator (expected GAME.PUB)");
        return NULL;
    }
    
    int game_len = dot - input;
    int pub_len = strlen(dot + 1);
    
    // Validate game code length: must be 4 (ASCII) or 8 (hex)
    if (game_len != 4 && game_len != 8) {
        fatal_error("Invalid gamecode: game part must be 4 characters (ASCII) or 8 characters (hex), got %d", game_len);
        return NULL;
    }
    
    // Validate publisher code length: must be 2 (ASCII) or 4 (hex)
    if (pub_len != 2 && pub_len != 4) {
        fatal_error("Invalid gamecode: publisher part must be 2 characters (ASCII) or 4 characters (hex), got %d", pub_len);
        return NULL;
    }
    
    // Determine format for each part and validate characters
    bool game_is_hex = (game_len == 8);
    bool pub_is_hex = (pub_len == 4);
    
    // Validate game code characters
    if (game_is_hex) {
        // Hex format - all characters must be hex digits
        for (int i = 0; i < game_len; i++) {
            if (!is_hex_char(input[i])) {
                fatal_error("Invalid character in hex gamecode: '%c' (only hex digits allowed)", input[i]);
                return NULL;
            }
        }
    } else {
        // ASCII format - all characters must be alphanumeric
        for (int i = 0; i < game_len; i++) {
            if (!is_valid_gamecode_char(input[i])) {
                fatal_error("Invalid character in ASCII gamecode: '%c' (only alphanumeric allowed)", input[i]);
                return NULL;
            }
        }
    }
    
    // Validate publisher code characters
    if (pub_is_hex) {
        // Hex format - all characters must be hex digits
        for (int i = 0; i < pub_len; i++) {
            if (!is_hex_char(dot[1 + i])) {
                fatal_error("Invalid character in hex pubcode: '%c' (only hex digits allowed)", dot[1 + i]);
                return NULL;
            }
        }
    } else {
        // ASCII format - all characters must be alphanumeric
        for (int i = 0; i < pub_len; i++) {
            if (!is_valid_gamecode_char(dot[1 + i])) {
                fatal_error("Invalid character in ASCII pubcode: '%c' (only alphanumeric allowed)", dot[1 + i]);
                return NULL;
            }
        }
    }
    
    // Allocate result buffer and copy the validated gamecode
    size_t total_len = strlen(input);
    char *result = (char*)malloc(total_len + 1);
    if (!result) {
        fatal_error("Memory allocation failed");
        return NULL;
    }
    
    strcpy(result, input);
    
    // Convert ASCII parts to uppercase if needed
    if (!game_is_hex) {
        for (int i = 0; i < game_len; i++) {
            if (result[i] >= 'a' && result[i] <= 'z') {
                result[i] = result[i] - 'a' + 'A';
            }
        }
    }
    
    if (!pub_is_hex) {
        for (int i = 0; i < pub_len; i++) {
            if (result[game_len + 1 + i] >= 'a' && result[game_len + 1 + i] <= 'z') {
                result[game_len + 1 + i] = result[game_len + 1 + i] - 'a' + 'A';
            }
        }
    }
    
    return result;
}

/**
 * @brief Clean up allocated memory in command options
 */
static void cleanup_command_options(void)
{
    if (g_command_opts.gamecode) {
        free(g_command_opts.gamecode);
        g_command_opts.gamecode = NULL;
    }
}

//
// MAIN FUNCTION
//

int main(int argc, char *argv[]) {
    // Initialize global options
    g_global_opts.verbose = 0;
    g_global_opts.force = false;
    g_global_opts.dry_run = false;
    g_global_opts.output_dir = NULL;
    g_global_opts.skip_header_bytes = -1;  // Auto-detect by default
    
    // Initialize command options
    memset(&g_command_opts, 0, sizeof(g_command_opts));
    g_command_opts.pak_size = 32;    // Default pak size in KB (32KB = 1 bank)
    g_command_opts.num_banks = 1;    // Default number of banks
    g_command_opts.debug_bufsize = 4096;  // Default buffer size for file operations
    g_command_opts.report_level = 1;     // Default fsck report level: WARNING
    g_command_opts.long_format = true;  // Default to long format for listing
    g_command_opts.directory = "";   // Default to current directory
    
    command_t cmd = CMD_NONE;
    int cmd_start_idx;
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Parse global options and find command
    if (parse_global_options(argc, argv, &cmd_start_idx) < 0) {
        return 1;
    }
    
    if (cmd_start_idx >= argc) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Parse command
    cmd = parse_command(argv[cmd_start_idx]);
    if (cmd == CMD_NONE) {
        fatal_error("Unknown command: %s", argv[cmd_start_idx]);
    }
    
    // Parse command-specific options (also accepts global long options here)
    int args_start_idx = parse_command_options(cmd, argc, argv, cmd_start_idx + 1);
    if (args_start_idx < 0) {
        return 1;
    }
    
    // Execute command
    return execute_command(cmd, argc, argv, args_start_idx);
}

//
// HELP AND VERSION FUNCTIONS
//

static void print_usage(const char *program_name) {
    printf("cpaktool - Controller Pak manipulation tool\n");
    printf("\n");
    printf("Usage: %s [OPTIONS] <command> [COMMAND-OPTIONS] [ARGS...]\n", program_name);
    printf("\n");
    printf("Global options:\n");
    printf("  -h, --help      Show this help\n");
    printf("  -V, --version   Show version information\n");
    printf("  -v, --verbose   Verbose output\n");
    printf("  -f, --force     Force operation without confirmation\n");
    printf("  -n, --dry-run   Show what would be done without actually doing it\n");
    printf("  --skip-header N     Skip N bytes at the beginning of pak files (0=no skip)\n");
    printf("                      If not specified, DexDrive format is auto-detected\n");
    printf("\n");
    printf("Commands:\n");
    printf("  list    (l)     List contents of Controller Pak\n");
    printf("  extract (x)     Extract files from Controller Pak\n");
    printf("  add     (a)     Add files to Controller Pak\n");
    printf("  delete  (d)     Delete files from Controller Pak\n");
    printf("  info    (i)     Show Controller Pak information\n");
    printf("  test    (t)     Test Controller Pak integrity\n");
    printf("  format  (fmt)   Format new Controller Pak\n");
    printf("\n");
    printf("Use '%s <command> --help' for command-specific help.\n", program_name);
    printf("\n");
    printf("File naming convention for 'add' command:\n");
    printf("  Files must be named as: GAME.PUB-FILENAME.EXT\n");
    printf("  Where:\n");
    printf("    GAME     = 4-char game code (e.g., NO7P, SMKE)\n");
    printf("    PUB      = 2-char publisher code (e.g., 01, 69)\n");
    printf("    FILENAME = Note filename (1-16 chars)\n");
    printf("    EXT      = Extension (1-4 chars, e.g., A, TXT, DAT)\n");
    printf("  Example: NO7P.69-TWINE.A\n");
}

static void print_version(void) {
    printf("cpaktool 1.0 - part of libdragon SDK\n");
    printf("Controller Pak manipulation tool\n");
}

//
// PARSING FUNCTIONS
//

static command_t parse_command(const char *cmd_str) {
    typedef struct {
        const char *name;
        command_t cmd;
    } cmd_entry_t;
    
    static const cmd_entry_t commands[] = {
        {"list", CMD_LIST},
        {"ls", CMD_LIST},
        {"l", CMD_LIST},
        {"extract", CMD_EXTRACT},
        {"x", CMD_EXTRACT},
        {"add", CMD_ADD},
        {"a", CMD_ADD},
        {"delete", CMD_DELETE},
        {"d", CMD_DELETE},
        {"rm", CMD_DELETE},
        {"info", CMD_INFO},
        {"i", CMD_INFO},
        {"test", CMD_TEST},
        {"t", CMD_TEST},
        {"fsck", CMD_TEST},
        {"format", CMD_FORMAT},
        {"fmt", CMD_FORMAT},
    };
    
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (!strcmp(cmd_str, commands[i].name)) {
            return commands[i].cmd;
        }
    }
    
    return CMD_NONE;
}

// Helper function for global options with optional provided value
static bool handle_global_option_with_value(const char *arg, const char *provided_value, const char *program_name, command_t cmd, bool before_command, int argc, char *argv[], int *arg_idx) {
    // Check for --option=value format and extract value if present
    const char *equals_value = NULL;
    char *arg_copy = NULL;
    const char *option_name = arg;
    
    const char *eq = strchr(arg, '=');
    if (eq) {
        // Create a copy of the argument without the =value part
        size_t name_len = eq - arg;
        arg_copy = (char*)malloc(name_len + 1);
        strncpy(arg_copy, arg, name_len);
        arg_copy[name_len] = '\0';
        option_name = arg_copy;
        equals_value = eq + 1;
    }
    
    if (!strcmp(option_name, "--help") || !strcmp(option_name, "-h")) {
        if (before_command) {
            print_usage(program_name ? program_name : "cpaktool");
        } else {
            print_command_usage(NULL, cmd);
        }
        exit(0);
    }
    if (!strcmp(option_name, "--version") || !strcmp(option_name, "-V")) {
        print_version();
        exit(0);
    }
    if (!strcmp(option_name, "--verbose") || !strcmp(option_name, "-v")) {
        g_global_opts.verbose += 1; // Increment verbosity level
        free(arg_copy);
        return true;
    }
    if (!strcmp(option_name, "--dry-run") || !strcmp(option_name, "-n")) {
        g_global_opts.dry_run = true;
        free(arg_copy);
        return true;
    }
    // Accept -f as global force in both positions; resolve conflicts by renaming local flags
    if (!strcmp(option_name, "--force") || !strcmp(option_name, "-f")) {
        g_global_opts.force = true;
        free(arg_copy);
        return true;
    }
    
    // Handle --skip-header specially (requires value)
    if (!strcmp(option_name, "--skip-header")) {
        const char *value;
        if (equals_value) {
            // Use value from --skip-header=value format
            value = equals_value;
        } else if (provided_value) {
            // Use provided value (from parse_command_options)
            value = provided_value;
        } else {
            // Use next argument for --skip-header value format
            if (*arg_idx + 1 >= argc || argv[*arg_idx + 1][0] == '-') {
                free(arg_copy);
                fatal_error("Option --skip-header requires a value");
            }
            value = argv[*arg_idx + 1];
            (*arg_idx)++; // Skip the value argument
        }
        
        char *endptr;
        long skip_bytes;

        if (!strcasecmp(value, "dexdrive")) {
            skip_bytes = 0x1040;
        } else {
            skip_bytes = strtol(value, &endptr, 0);
            if (*endptr != '\0' || skip_bytes < 0) {
                free(arg_copy);
                fatal_error("Invalid value for --skip-header: %s", value);
            }
        }

        g_global_opts.skip_header_bytes = (int)skip_bytes;
        free(arg_copy);
        return true;
    }
    
    free(arg_copy);
    return false;
}

// Single global options handler usable before or after the command
static bool handle_global_option(const char *arg, const char *program_name, command_t cmd, bool before_command, int argc, char *argv[], int *arg_idx) {
    return handle_global_option_with_value(arg, NULL, program_name, cmd, before_command, argc, argv, arg_idx);
}

static int parse_global_options(int argc, char *argv[], int *start_idx) {
    *start_idx = 1;
    
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            // Found non-option argument (command)
            *start_idx = i;
            break;
        }
        
        char *arg = argv[i];
        
        if (handle_global_option(arg, argv[0], CMD_NONE, true, argc, argv, &i)) {
            // handled (or exited)
        } else {
            fatal_error("Unknown global option: %s", arg);
        }
        
        *start_idx = i + 1;
    }
    
    return 0;
}

static int parse_command_options(command_t cmd, int argc, char *argv[], int start_idx) {
    int i;
    for (i = start_idx; i < argc; i++) {
        if (argv[i][0] != '-') {
            // Found non-option argument, stop parsing options
            break;
        }
        
        char *arg = argv[i];
        char *original_arg = arg;  // Save original argument for global options
        char *value = NULL;
        bool has_equals = false;
        
        // Check for --option=value format
        char *eq = strchr(arg, '=');
        if (eq) {
            *eq = '\0';
            value = eq + 1;
            has_equals = true;
        } else if (i + 1 < argc && argv[i + 1][0] != '-') {
            // Next argument might be the value
            value = argv[i + 1];
        }
        
        // Try global options here as well (after command)
        int temp_idx = i;  // Create temporary index for handle_global_option
        if (handle_global_option_with_value(original_arg, has_equals ? value : NULL, NULL, cmd, false, argc, argv, &temp_idx)) {
            if (has_equals) *eq = '='; // restore
            i = temp_idx;  // Update our loop index
            continue; // proceed to next arg
        }
        
        // Parse command-specific options
        switch (cmd) {
            case CMD_LIST:
                if (!strcmp(arg, "-1") || !strcmp(arg, "--only-names")) {
                    g_command_opts.long_format = false;
                } else if (!strcmp(arg, "-H") || !strcmp(arg, "--human-readable")) {
                    g_command_opts.human_readable = true;
                } else if (!strcmp(arg, "--crc")) {
                    g_command_opts.show_crc = true;
                    g_command_opts.long_format = true;  // --crc disables --only-names
                } else if (!strcmp(arg, "-r") || !strcmp(arg, "--reverse")) {
                    g_command_opts.reverse_sort = true;
                } else if (!strcmp(arg, "-s") || !strcmp(arg, "--sort")) {
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    g_command_opts.sort_by = value;
                    if (!has_equals) i++; // Skip next argument as it's the value
                } else if (!strcmp(arg, "-j") || !strcmp(arg, "--json")) {
                    g_command_opts.json_output = true;
                } else {
                    fatal_error("Unknown option for list command: %s", arg);
                }
                break;
            
            case CMD_EXTRACT:
                if (!strcmp(arg, "-o") || !strcmp(arg, "--overwrite")) {
                    g_command_opts.overwrite = true;
                } else if (!strcmp(arg, "-d") || !strcmp(arg, "--directory")) {
                    if (!value) {
                        g_command_opts.directory = "";
                    } else {
                        // Check if we need to add trailing slash
                        size_t len = strlen(value);
                        if (len > 0 && value[len-1] != '/') {
                            // Need to allocate memory for directory + trailing slash
                            char *dir_with_slash = (char*)malloc(len + 2);
                            strcpy(dir_with_slash, value);
                            dir_with_slash[len] = '/';
                            dir_with_slash[len+1] = '\0';
                            g_command_opts.directory = dir_with_slash;
                        } else {
                            g_command_opts.directory = value;
                        }
                    }
                    if (!has_equals) i++; // Skip next argument as it's the value
                } else if (!strcmp(arg, "-s") || !strcmp(arg, "--stdout")) {
                    g_command_opts.extract_stdout = true;
                } else if (!strcmp(arg, "--debug-bufsize")) {
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    g_command_opts.debug_bufsize = atoi(value);
                    if (g_command_opts.debug_bufsize <= 0) {
                        fatal_error("Buffer size must be positive: %d", g_command_opts.debug_bufsize);
                    }
                    if (!has_equals) i++; // Skip next argument as it's the value
                } else {
                    fatal_error("Unknown option for extract command: %s", arg);
                }
                break;
                
            case CMD_ADD:
                if (!strcmp(arg, "-c") || !strcmp(arg, "--create")) {
                    g_command_opts.create_pak = true;
                } else if (!strcmp(arg, "-u") || !strcmp(arg, "--update")) {
                    g_command_opts.update_only = true;
                } else if (!strcmp(arg, "--partial")) {
                    g_command_opts.allow_partial = true;
                } else if (!strcmp(arg, "-s") || !strcmp(arg, "--size")) {
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    g_command_opts.pak_size = atoi(value);
                    if (g_command_opts.pak_size % 32 != 0) {
                        fatal_error("Pak size must be a multiple of 32 KiB: %d", g_command_opts.pak_size);
                    }
                    if (!has_equals) i++; // Skip next argument as it's the value
                } else if (!strcmp(arg, "-g") || !strcmp(arg, "--gamecode")) {
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    // Parse and validate the gamecode, converting hex to ASCII if needed
                    g_command_opts.gamecode = parse_and_validate_gamecode(value);
                    if (!g_command_opts.gamecode) {
                        // Error message already printed by parse_and_validate_gamecode
                        return -1;
                    }
                    if (!has_equals) i++; // Skip next argument as it's the value
                } else if (!strcmp(arg, "--debug-bufsize")) {
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    g_command_opts.debug_bufsize = atoi(value);
                    if (g_command_opts.debug_bufsize <= 0) {
                        fatal_error("Buffer size must be positive: %d", g_command_opts.debug_bufsize);
                    }
                    if (!has_equals) i++; // Skip next argument as it's the value
                } else {
                    fatal_error("Unknown option for add command: %s", arg);
                }
                break;
                
            case CMD_DELETE:
                if (!strcmp(arg, "-i") || !strcmp(arg, "--interactive")) {
                    g_command_opts.interactive = true;
                } else {
                    fatal_error("Unknown option for delete command: %s", arg);
                }
                break;
                
            case CMD_INFO:
                if (!strcmp(arg, "-j") || !strcmp(arg, "--json")) {
                    g_command_opts.json_output = true;
                } else {
                    fatal_error("Unknown option for info command: %s", arg);
                }
                break;
                
            case CMD_TEST:
                if (!strcmp(arg, "-r") || !strcmp(arg, "--repair")) {
                    g_command_opts.fix_errors = true;
                } else if (!strcmp(arg, "-j") || !strcmp(arg, "--json")) {
                    g_command_opts.json_output = true;
                } else if (!strcmp(arg, "--level")) {
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    // Accept case-insensitive values: INFO, WARNING, ERROR
                    if (!strcasecmp(value, "INFO")) g_command_opts.report_level = 0;
                    else if (!strcasecmp(value, "WARNING")) g_command_opts.report_level = 1;
                    else if (!strcasecmp(value, "ERROR")) g_command_opts.report_level = 2;
                    else fatal_error("Invalid level '%s' (use INFO, WARNING, or ERROR)", value);
                    if (!has_equals) i++; // consume value
                } else {
                    fatal_error("Unknown option for test command: %s", arg);
                }
                break;
                
            case CMD_FORMAT:
                if (!strcmp(arg, "-s") || !strcmp(arg, "--size")) {
                    if (g_command_opts.banks_specified) {
                        fatal_error("Cannot specify both --size and --banks options");
                    }
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    g_command_opts.pak_size = atoi(value);
                    if (g_command_opts.pak_size % 32 != 0) {
                        fatal_error("Pak size must be a multiple of 32 KiB: %d", g_command_opts.pak_size);
                    }
                    // Convert size to banks (32KB per bank)
                    g_command_opts.num_banks = g_command_opts.pak_size / 32;
                    g_command_opts.size_specified = true;
                    if (!has_equals) i++; // Skip next argument as it's the value
                } else if (!strcmp(arg, "-b") || !strcmp(arg, "--banks")) {
                    if (g_command_opts.size_specified) {
                        fatal_error("Cannot specify both --size and --banks options");
                    }
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    g_command_opts.num_banks = atoi(value);
                    // Update pak_size to match banks
                    g_command_opts.pak_size = g_command_opts.num_banks * 32;
                    g_command_opts.banks_specified = true;
                    if (!has_equals) i++; // Skip next argument as it's the value
                } else {
                    fatal_error("Unknown option for format command: %s", arg);
                }
                break;
                
            default:
                fatal_error("Unknown command");
                break;
        }
        
        // Restore the '=' if we modified it
        if (has_equals) {
            *eq = '=';
        }
    }
    
    return i;  // Return the index where arguments start
}

static void print_command_usage(const char *program_name, command_t cmd) {
    const char *prog = program_name ? program_name : "cpaktool";
    
    switch (cmd) {
        case CMD_LIST:
            printf("Usage: %s list [OPTIONS] <pak_file> [patterns...]\n", prog);
            printf("\n");
            printf("List contents of a Controller Pak file.\n");
            printf("\n");
            printf("Options:\n");
            printf("  -1, --only-names        Show only the file names, one per line\n");
            printf("  -H, --human-readable    Show file sizes in human-readable format (e.g., 1K, 2M)\n");
            printf("  --crc                   Show CRC32 checksum of file contents\n");
            printf("  -s, --sort <key>        Sort by 'name' (default) or 'size'\n");
            printf("  -r, --reverse           Reverse sort order\n");
            printf("  -j, --json              Output in JSON format\n");
            break;
        case CMD_EXTRACT:
            printf("Usage: %s extract [OPTIONS] <pak_file> [patterns...]\n", prog);
            printf("\n");
            printf("Extract files from a Controller Pak file.\n");
            printf("\n");
            printf("Options:\n");
            printf("  -o, --overwrite         Overwrite existing files\n");
            printf("  -d, --directory <dir>   Extract files to the specified directory\n");
            printf("  -s, --stdout            Extract file contents to stdout instead of files\n");
            break;
        case CMD_ADD:
            printf("Usage: %s add [OPTIONS] <pak_file> <files...>\n", prog);
            printf("\n");
            printf("Add files to a Controller Pak file.\n");
            printf("\n");
            printf("Options:\n");
            printf("  -c, --create            Create a new pak file if it doesn't exist\n");
            printf("  -s, --size <KB>         Size of the new pak file in kilobytes (default: 32)\n");
            printf("  -u, --update            Update existing files instead of erroring\n");
            printf("  -g, --gamecode <id>     Default game/publisher code (e.g., 'DRAG.ON')\n");
            printf("      --partial           Keep partially written files on error\n");
            break;
        case CMD_DELETE:
            printf("Usage: %s delete [OPTIONS] <pak_file> <patterns...>\n", prog);
            printf("\n");
            printf("Delete files from a Controller Pak file.\n");
            printf("\n");
            printf("Options:\n");
            printf("  -i, --interactive       Prompt before every removal\n");
            break;
        case CMD_INFO:
            printf("Usage: %s info [OPTIONS] <pak_file>\n", prog);
            printf("\n");
            printf("Show information about a Controller Pak file.\n");
            printf("\n");
            printf("Options:\n");
            printf("  -j, --json              Output in JSON format\n");
            break;
        case CMD_TEST:
            printf("Usage: %s test [OPTIONS] <pak_file>\n", prog);
            printf("\n");
            printf("Test and optionally repair the filesystem integrity of a Controller Pak file.\n");
            printf("\n");
            printf("Options:\n");
            printf("  -r, --repair            Attempt to fix any issues found\n");
            printf("  --level <level>         Set report level: INFO, WARNING, ERROR (default: WARNING)\n");
            break;
        case CMD_FORMAT:
            printf("Usage: %s format [OPTIONS] <pak_file>\n", prog);
            printf("\n");
            printf("Create and format a new Controller Pak file.\n");
            printf("\n");
            printf("Options:\n");
            printf("  -s, --size <KB>         Size of the new pak file in kilobytes (default: 32)\n");
            printf("  -b, --banks <num>       Number of banks for the new pak file (1 bank = 32 KB)\n");
            printf("  -f, --force             Overwrite the file if it already exists\n");
            break;
        default:
            printf("No help available for this command.\n");
            break;
    }
}

static int execute_command(command_t cmd, int argc, char *argv[], int start_idx) {
    // Count non-option arguments
    int arg_count = 0;
    for (int i = start_idx; i < argc; i++) {
        if (argv[i][0] != '-') {
            arg_count++;
        }
    }
    
    // Collect non-option arguments
    char **args = (char**)malloc(arg_count * sizeof(char*));
    int arg_idx = 0;
    for (int i = start_idx; i < argc; i++) {
        if (argv[i][0] != '-') {
            args[arg_idx++] = argv[i];
        }
    }
    
    int result = 0;
    
    switch (cmd) {
        case CMD_LIST:
            if (arg_count < 1) {
                fatal_error("list command requires at least one argument (pak file)");
            }
            result = cmd_list(args[0], &args[1], arg_count - 1);
            break;
            
        case CMD_EXTRACT:
            if (arg_count < 1) {
                fatal_error("extract command requires at least one argument (pak file)");
            }
            result = cmd_extract(args[0], &args[1], arg_count - 1);
            break;
            
        case CMD_ADD:
            if (arg_count < 2) {
                fatal_error("add command requires at least two arguments (pak file and file to add)");
            }
            result = cmd_add(args[0], &args[1], arg_count - 1);
            break;
            
        case CMD_DELETE:
            if (arg_count < 2) {
                fatal_error("delete command requires at least two arguments (pak file and pattern)");
            }
            result = cmd_delete(args[0], &args[1], arg_count - 1);
            break;
            
        case CMD_INFO:
            if (arg_count != 1) {
                fatal_error("info command requires exactly one argument (pak file)");
            }
            result = cmd_info(args[0]);
            break;
            
        case CMD_TEST:
            if (arg_count != 1) {
                fatal_error("test command requires exactly one argument (pak file)");
            }
            result = cmd_test(args[0]);
            break;
            
        case CMD_FORMAT:
            if (arg_count != 1) {
                fatal_error("format command requires exactly one argument (pak file)");
            }
            result = cmd_format(args[0]);
            break;
            
        default:
            fatal_error("Unknown command");
            break;
    }
    
    free(args);
    cleanup_command_options();  // Free allocated memory
    return result;
}

//
// UTILITY FUNCTIONS
//

void fatal_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

void warning(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Warning: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void verbose_log(const char *fmt, ...) {
    if (!g_global_opts.verbose) return;
    
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}
