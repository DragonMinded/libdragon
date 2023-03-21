#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include "../common/subprocess.h"
#include "../common/polyfill.h"

bool verbose_flag = false;
char *n64_inst = NULL;

// Printf to stderr if verbose
void verbose(const char *fmt, ...) {
    if (verbose_flag) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

void print_args(const char *name)
{
    fprintf(stderr, "%s - Output list of undefined symbols in all ELFs\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: %s [flags] [<input_elfs>]\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -v/--verbose            Verbose output\n");
    fprintf(stderr, "   -o/--output <file>      Specify output file (default stdout)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "This program requires a libdragon toolchain installed in $N64_INST.\n");
}

void dump_elf_undef(const char *infn, FILE *out_file)
{
    //Readelf parameters
    struct subprocess_s subp;
    char *readelf_bin = NULL;
    const char *args[5] = {0};
    //Readelf output
    FILE *readelf_stdout = NULL;
    char *line_buf = NULL;
    size_t line_buf_size = 0;
    asprintf(&readelf_bin, "%s/bin/mips64-elf-readelf", n64_inst);
    args[0] = readelf_bin;
    args[1] = "-s"; //Output symbol table
    args[2] = "-W"; //Wide output
    args[3] = infn; //Input filename
    if (subprocess_create(args, subprocess_option_no_window, &subp) != 0) {
        fprintf(stderr, "Error: cannot run: %s\n", readelf_bin);
        free(readelf_bin);
        exit(1);
    }
    readelf_stdout = subprocess_stdout(&subp);
    //Skip first 3 lines of stdout from readelf
    getline(&line_buf, &line_buf_size, readelf_stdout); //Blank line
    getline(&line_buf, &line_buf_size, readelf_stdout); //Symbol table description
    //Check if program actually worked
    if(!strcmp(line_buf, "")) {
        fprintf(stderr, "Error running readelf\n");
        //Cleanup and exit program
        free(line_buf);
        free(readelf_bin);
        subprocess_terminate(&subp);
        exit(1);
    }
    getline(&line_buf, &line_buf_size, readelf_stdout); //Symbol table format
    //Read symbol table output from readelf
    verbose("Outputting undefined symbols from ELF\n");
    while(getline(&line_buf, &line_buf_size, readelf_stdout) != -1) {
        size_t line_len = strlen(line_buf);
        char *und_section_title = strstr(line_buf, " UND ");
        //Output non-empty undefined symbols
        if(und_section_title && strlen(&und_section_title[5]) > 1) {
            line_buf[line_len-1] = 0; //Remove extraneous newline
            //Output symbol
            fprintf(out_file, "EXTERN(%s)\n", &und_section_title[5]);
        }
    }
    //Free resources
    free(line_buf);
    free(readelf_bin);
    subprocess_terminate(&subp);
}

void process(const char *infn, FILE *out_file)
{
    verbose("Processing ELF %s\n", infn);
    dump_elf_undef(infn, out_file);
}

int main(int argc, char **argv)
{
    FILE *out_file = stdout;
    if(argc < 2) {
        //Print usage if too few arguments are passed
        print_args(argv[0]);
        return 1;
    }
    //Get libdragon install directory
    if (!n64_inst) {
        // n64.mk supports having a separate installation for the toolchain and
        // libdragon. So first check if N64_GCCPREFIX is set; if so the toolchain
        // is there. Otherwise, fallback to N64_INST which is where we expect
        // the toolchain to reside.
        n64_inst = getenv("N64_GCCPREFIX");
        if (!n64_inst)
            n64_inst = getenv("N64_INST");
        if (!n64_inst) {
            // Do not mention N64_GCCPREFIX in the error message, since it is
            // a seldom used configuration.
            fprintf(stderr, "Error: N64_INST environment variable not set.\n");
            return 1;
        }
        // Remove the trailing backslash if any. On some system, running
        // popen with a path containing double backslashes will fail, so
        // we normalize it here.
        n64_inst = strdup(n64_inst);
        int n = strlen(n64_inst);
        if (n64_inst[n-1] == '/' || n64_inst[n-1] == '\\')
            n64_inst[n-1] = 0;
    }
    for(int i=1; i<argc; i++) {
        if(argv[i][0] == '-') {
            if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                //Print help
                print_args(argv[0]);
                return 0;
            } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
                //Specify verbose flag
                verbose_flag = true;
            } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
                //Specify output file
                if(++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                //Open specified output file
                out_file = fopen(argv[i], "a");
                if(!out_file) {
                    //Output error if file cannot be opened
                    fprintf(stderr, "Cannot create file: %s\n", argv[i-1]);
                    return 1;
                }
            } else {
                //Output invalid flag warning
                fprintf(stderr, "invalid flag: %s\n", argv[i]);
                return 1;
            }
            continue;
        }
        process(argv[i], out_file);
    }
    fclose(out_file);
    return 0;
}