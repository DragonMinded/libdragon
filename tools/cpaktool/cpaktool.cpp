#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <strings.h>

#include "cpaktool.h"

// Forward declarations
static void print_usage(const char *program_name);
static void print_command_usage(const char *program_name, command_t cmd);
static void print_version(void);
static command_t parse_command(const char *cmd_str);
static bool handle_global_option(const char *arg, global_options_t *opts, const char *program_name, command_t cmd, bool before_command);
static int parse_global_options(int argc, char *argv[], int *start_idx, global_options_t *opts);
static int parse_command_options(command_t cmd, int argc, char *argv[], int start_idx, global_options_t *global_opts, command_options_t *cmd_opts);
static int execute_command(command_t cmd, global_options_t *global_opts, command_options_t *cmd_opts, int argc, char *argv[], int start_idx);

//
// MAIN FUNCTION
//

int main(int argc, char *argv[]) {
    global_options_t global_opts = {0};
    command_options_t cmd_opts = {0};
    command_t cmd = CMD_NONE;
    int cmd_start_idx;
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Parse global options and find command
    if (parse_global_options(argc, argv, &cmd_start_idx, &global_opts) < 0) {
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
    int args_start_idx = parse_command_options(cmd, argc, argv, cmd_start_idx + 1, &global_opts, &cmd_opts);
    if (args_start_idx < 0) {
        return 1;
    }
    
    // Execute command
    return execute_command(cmd, &global_opts, &cmd_opts, argc, argv, args_start_idx);
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
    printf("\n");
    printf("Commands:\n");
    printf("  list    (l)     List contents of Controller Pak\n");
    printf("  extract (x)     Extract files from Controller Pak\n");
    printf("  add     (a)     Add files to Controller Pak\n");
    printf("  delete  (d)     Delete files from Controller Pak\n");
    printf("  info    (i)     Show Controller Pak information\n");
    printf("  test    (t)     Test Controller Pak integrity\n");
    printf("  format  (fmt)   Format new Controller Pak\n");
    printf("  convert (conv)  Convert between pak formats\n");
    printf("  compare (diff)  Compare two Controller Paks\n");
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
        {"format", CMD_FORMAT},
        {"fmt", CMD_FORMAT},
        {"convert", CMD_CONVERT},
        {"conv", CMD_CONVERT},
        {"compare", CMD_COMPARE},
        {"diff", CMD_COMPARE}
    };
    
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (!strcmp(cmd_str, commands[i].name)) {
            return commands[i].cmd;
        }
    }
    
    return CMD_NONE;
}

// Single global options handler usable before or after the command
static bool handle_global_option(const char *arg, global_options_t *opts, const char *program_name, command_t cmd, bool before_command) {
    if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
        if (before_command) {
            print_usage(program_name ? program_name : "cpaktool");
        } else {
            print_command_usage(NULL, cmd);
        }
        exit(0);
    }
    if (!strcmp(arg, "--version") || !strcmp(arg, "-V")) {
        print_version();
        exit(0);
    }
    if (!strcmp(arg, "--verbose") || !strcmp(arg, "-v")) {
        opts->verbose = true;
        return true;
    }
    if (!strcmp(arg, "--dry-run") || !strcmp(arg, "-n")) {
        opts->dry_run = true;
        return true;
    }
    // Accept -f as global force in both positions; resolve conflicts by renaming local flags
    if (!strcmp(arg, "--force") || !strcmp(arg, "-f")) {
        opts->force = true;
        return true;
    }
    return false;
}

static int parse_global_options(int argc, char *argv[], int *start_idx, global_options_t *opts) {
    *start_idx = 1;
    
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            // Found non-option argument (command)
            *start_idx = i;
            break;
        }
        
        char *arg = argv[i];
        
        // Check for --option=value format  
        char *eq = strchr(arg, '=');
        if (eq) {
            *eq = '\0';
        }
        
        if (handle_global_option(arg, opts, argv[0], CMD_NONE, true)) {
            // handled (or exited)
        } else {
            fatal_error("Unknown global option: %s", argv[i]);
        }
        
        // Restore the '=' if we modified it
        if (eq) {
            *eq = '=';
        }
        
        *start_idx = i + 1;
    }
    
    return 0;
}

static int parse_command_options(command_t cmd, int argc, char *argv[], int start_idx, global_options_t *global_opts, command_options_t *cmd_opts) {
    // Initialize with defaults
    memset(cmd_opts, 0, sizeof(*cmd_opts));
    cmd_opts->pak_size = 32;    // Default pak size in KB (32KB = 1 bank)
    cmd_opts->num_banks = 1;    // Default number of banks
    cmd_opts->debug_bufsize = 4096;  // Default buffer size for file operations
    // Default fsck report level: WARNING
    cmd_opts->report_level = 1;

    int i;
    for (i = start_idx; i < argc; i++) {
        if (argv[i][0] != '-') {
            // Found non-option argument, stop parsing options
            break;
        }
        
        char *arg = argv[i];
        char *value = NULL;
        
        // Check for --option=value format
        char *eq = strchr(arg, '=');
        if (eq) {
            *eq = '\0';
            value = eq + 1;
        } else if (i + 1 < argc && argv[i + 1][0] != '-') {
            // Next argument might be the value
            value = argv[i + 1];
        }
        
        // Try global options here as well (after command)
        if (handle_global_option(arg, global_opts, NULL, cmd, false)) {
            if (eq) *eq = '='; // restore
            continue; // proceed to next arg
        }
        
        // Parse command-specific options
        switch (cmd) {
            case CMD_LIST:
                if (!strcmp(arg, "-l") || !strcmp(arg, "--long")) {
                    cmd_opts->long_format = true;
                } else if (!strcmp(arg, "-H") || !strcmp(arg, "--human-readable")) {
                    cmd_opts->human_readable = true;
                } else if (!strcmp(arg, "-r") || !strcmp(arg, "--reverse")) {
                    cmd_opts->reverse_sort = true;
                } else if (!strcmp(arg, "-s") || !strcmp(arg, "--sort")) {
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    cmd_opts->sort_by = value;
                    if (!eq) i++; // Skip next argument as it's the value
                } else {
                    fatal_error("Unknown option for list command: %s", arg);
                }
                break;
            
            case CMD_EXTRACT:
                if (!strcmp(arg, "-o") || !strcmp(arg, "--overwrite")) {
                    cmd_opts->overwrite = true;
                } else {
                    fatal_error("Unknown option for extract command: %s", arg);
                }
                break;
                
            case CMD_ADD:
                if (!strcmp(arg, "-c") || !strcmp(arg, "--create")) {
                    cmd_opts->create_pak = true;
                } else if (!strcmp(arg, "-u") || !strcmp(arg, "--update")) {
                    cmd_opts->update_only = true;
                } else if (!strcmp(arg, "-s") || !strcmp(arg, "--size")) {
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    cmd_opts->pak_size = atoi(value);
                    if (!eq) i++; // Skip next argument as it's the value
                } else if (!strcmp(arg, "-g") || !strcmp(arg, "--gamecode")) {
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    cmd_opts->gamecode = value;
                    if (!eq) i++; // Skip next argument as it's the value
                } else if (!strcmp(arg, "--debug-bufsize")) {
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    cmd_opts->debug_bufsize = atoi(value);
                    if (cmd_opts->debug_bufsize <= 0) {
                        fatal_error("Buffer size must be positive: %d", cmd_opts->debug_bufsize);
                    }
                    if (!eq) i++; // Skip next argument as it's the value
                } else {
                    fatal_error("Unknown option for add command: %s", arg);
                }
                break;
                
            case CMD_DELETE:
                if (!strcmp(arg, "-i") || !strcmp(arg, "--interactive")) {
                    cmd_opts->interactive = true;
                } else {
                    fatal_error("Unknown option for delete command: %s", arg);
                }
                break;
                
            case CMD_INFO:
                if (!strcmp(arg, "-s") || !strcmp(arg, "--stats")) {
                    cmd_opts->show_stats = true;
                } else if (!strcmp(arg, "-b") || !strcmp(arg, "--banks")) {
                    cmd_opts->show_banks = true;
                } else if (!strcmp(arg, "-F") || !strcmp(arg, "--filesystem")) {
                    cmd_opts->show_filesystem = true;
                } else if (!strcmp(arg, "-H") || !strcmp(arg, "--header-only")) {
                    cmd_opts->header_only = true;
                } else {
                    fatal_error("Unknown option for info command: %s", arg);
                }
                break;
                
            case CMD_TEST:
                if (!strcmp(arg, "-r") || !strcmp(arg, "--repair")) {
                    cmd_opts->fix_errors = true;
                } else if (!strcmp(arg, "--level")) {
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    // Accept case-insensitive values: INFO, WARNING, ERROR
                    if (!strcasecmp(value, "INFO")) cmd_opts->report_level = 0;
                    else if (!strcasecmp(value, "WARNING")) cmd_opts->report_level = 1;
                    else if (!strcasecmp(value, "ERROR")) cmd_opts->report_level = 2;
                    else fatal_error("Invalid level '%s' (use INFO, WARNING, or ERROR)", value);
                    if (!eq) i++; // consume value
                } else {
                    fatal_error("Unknown option for test command: %s", arg);
                }
                break;
                
            case CMD_FORMAT:
                if (!strcmp(arg, "-s") || !strcmp(arg, "--size")) {
                    if (cmd_opts->banks_specified) {
                        fatal_error("Cannot specify both --size and --banks options");
                    }
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    cmd_opts->pak_size = atoi(value);
                    // Convert size to banks (32KB per bank)
                    cmd_opts->num_banks = (cmd_opts->pak_size + 31) / 32;
                    cmd_opts->size_specified = true;
                    if (!eq) i++; // Skip next argument as it's the value
                } else if (!strcmp(arg, "-b") || !strcmp(arg, "--banks")) {
                    if (cmd_opts->size_specified) {
                        fatal_error("Cannot specify both --size and --banks options");
                    }
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    cmd_opts->num_banks = atoi(value);
                    // Update pak_size to match banks
                    cmd_opts->pak_size = cmd_opts->num_banks * 32;
                    cmd_opts->banks_specified = true;
                    if (!eq) i++; // Skip next argument as it's the value
                } else {
                    fatal_error("Unknown option for format command: %s", arg);
                }
                break;
                
            case CMD_CONVERT:
                if (!strcmp(arg, "-F") || !strcmp(arg, "--from")) {
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    cmd_opts->from_format = value;
                    if (!eq) i++; // Skip next argument as it's the value
                } else if (!strcmp(arg, "-t") || !strcmp(arg, "--to")) {
                    if (!value) {
                        fatal_error("Option %s requires a value", arg);
                    }
                    cmd_opts->to_format = value;
                    if (!eq) i++; // Skip next argument as it's the value
                } else {
                    fatal_error("Unknown option for convert command: %s", arg);
                }
                break;
                
            case CMD_COMPARE:
                if (!strcmp(arg, "-b") || !strcmp(arg, "--brief")) {
                    cmd_opts->brief = true;
                } else if (!strcmp(arg, "-s") || !strcmp(arg, "--summary")) {
                    cmd_opts->summary = true;
                } else {
                    fatal_error("Unknown option for compare command: %s", arg);
                }
                break;
                
            default:
                fatal_error("Unknown command");
                break;
        }
        
        // Restore the '=' if we modified it
        if (eq) {
            *eq = '=';
        }
    }
    
    return i;  // Return the index where arguments start
}

static void print_command_usage(const char *program_name, command_t cmd) {
    const char *prog = program_name ? program_name : "cpaktool";
    
    switch (cmd) {
        case CMD_LIST:
            printf("Usage: %s list [OPTIONS] <pak-file> [pattern...]\n", prog);
            printf("List files in Controller Pak\n");
            printf("Options:\n");
            printf("  -l, --long              Long format (show details)\n");
            printf("  -H, --human-readable    Human-readable sizes\n");
            printf("  -s, --sort FIELD        Sort by field (name, size, date)\n");
            printf("  -r, --reverse           Reverse sort order\n");
            break;
            
        case CMD_EXTRACT:
            printf("Usage: %s extract [OPTIONS] <pak-file> [pattern...]\n", prog);
            printf("Extract files from Controller Pak\n");
            printf("Options:\n");
            printf("  -o, --overwrite         Overwrite existing files\n");
            break;
            
        case CMD_ADD:
            printf("Usage: %s add [OPTIONS] <pak-file> <file...>\n", prog);
            printf("Add files to Controller Pak\n");
            printf("Options:\n");
            printf("  -c, --create            Create pak if it doesn't exist\n");
            printf("  -u, --update            Update existing files only\n");
            printf("  -s, --size SIZE         Pak size in KB (default: 32)\n");
            printf("  -g, --gamecode CODE     Game code for files (format: ABCD.EF, default: DRAG.ON)\n");
            break;
            
        case CMD_DELETE:
            printf("Usage: %s delete [OPTIONS] <pak-file> <pattern...>\n", prog);
            printf("Delete files from Controller Pak\n");
            printf("Options:\n");
            printf("  -i, --interactive       Ask before deleting each file\n");
            break;
            
        case CMD_INFO:
            printf("Usage: %s info [OPTIONS] <pak-file>\n", prog);
            printf("Show Controller Pak information\n");
            printf("Options:\n");
            printf("  -s, --stats             Show statistics\n");
            printf("  -b, --banks             Show bank information\n");
            printf("  -F, --filesystem        Show filesystem details\n");
            printf("  -H, --header-only       Show header only\n");
            break;
            
        case CMD_TEST:
            printf("Usage: %s test [OPTIONS] <pak-file>\n", prog);
            printf("Test Controller Pak integrity\n");
            printf("Options:\n");
            printf("  -r, --repair            Attempt to repair errors\n");
            printf("      --level LEVEL       Minimum messages to show: INFO, WARNING, ERROR (default: WARNING)\n");
            break;
            
        case CMD_FORMAT:
            printf("Usage: %s format [OPTIONS] <pak-file>\n", prog);
            printf("Format Controller Pak\n");
            printf("Options:\n");
            printf("  -s, --size SIZE         Pak size in KB (default: 32)\n");
            printf("  -b, --banks NUM         Number of banks (default: 1)\n");
            printf("Note: --size and --banks are mutually exclusive\n");
            break;
            
        case CMD_CONVERT:
            printf("Usage: %s convert [OPTIONS] <input-file> <output-file>\n", prog);
            printf("Convert between Controller Pak formats\n");
            printf("Options:\n");
            printf("  -F, --from FORMAT       Input format (auto-detect if not specified)\n");
            printf("  -t, --to FORMAT         Output format (auto-detect if not specified)\n");
            break;
            
        case CMD_COMPARE:
            printf("Usage: %s compare [OPTIONS] <pak-file1> <pak-file2>\n", prog);
            printf("Compare two Controller Paks\n");
            printf("Options:\n");
            printf("  -b, --brief             Show only differences\n");
            printf("  -s, --summary           Show summary only\n");
            break;
            
        default:
            printf("Unknown command\n");
            break;
    }
}

static int execute_command(command_t cmd, global_options_t *global_opts, command_options_t *cmd_opts, int argc, char *argv[], int start_idx) {
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
            result = cmd_list(global_opts, cmd_opts, args[0], &args[1], arg_count - 1);
            break;
            
        case CMD_EXTRACT:
            if (arg_count < 1) {
                fatal_error("extract command requires at least one argument (pak file)");
            }
            result = cmd_extract(global_opts, cmd_opts, args[0], &args[1], arg_count - 1);
            break;
            
        case CMD_ADD:
            if (arg_count < 2) {
                fatal_error("add command requires at least two arguments (pak file and file to add)");
            }
            result = cmd_add(global_opts, cmd_opts, args[0], &args[1], arg_count - 1);
            break;
            
        case CMD_DELETE:
            if (arg_count < 2) {
                fatal_error("delete command requires at least two arguments (pak file and pattern)");
            }
            result = cmd_delete(global_opts, cmd_opts, args[0], &args[1], arg_count - 1);
            break;
            
        case CMD_INFO:
            if (arg_count != 1) {
                fatal_error("info command requires exactly one argument (pak file)");
            }
            result = cmd_info(global_opts, cmd_opts, args[0]);
            break;
            
        case CMD_TEST:
            if (arg_count != 1) {
                fatal_error("test command requires exactly one argument (pak file)");
            }
            result = cmd_test(global_opts, cmd_opts, args[0]);
            break;
            
        case CMD_FORMAT:
            if (arg_count != 1) {
                fatal_error("format command requires exactly one argument (pak file)");
            }
            result = cmd_format(global_opts, cmd_opts, args[0]);
            break;
            
        case CMD_CONVERT:
            if (arg_count != 2) {
                fatal_error("convert command requires exactly two arguments (input and output files)");
            }
            result = cmd_convert(global_opts, cmd_opts, args[0], args[1]);
            break;
            
        case CMD_COMPARE:
            if (arg_count != 2) {
                fatal_error("compare command requires exactly two arguments (two pak files)");
            }
            result = cmd_compare(global_opts, cmd_opts, args[0], args[1]);
            break;
            
        default:
            fatal_error("Unknown command");
            break;
    }
    
    free(args);
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

void verbose_log(global_options_t *opts, const char *fmt, ...) {
    if (!opts->verbose) return;
    
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}
