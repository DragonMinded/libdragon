#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "../common/subprocess.h"
#include "../common/polyfill.h"
#include "../common/binout.c"
#include "../common/binout.h"

//DSO Symbol Table Internals
#include "../../src/dso_format.h"

struct { char *key; int64_t value; } *imports_hash = NULL;

dso_sym_t *export_syms = NULL;

bool export_all = false;
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
    fprintf(stderr, "%s - Generate main executable symbol table\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: %s [flags] input_elf output_file\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -v/--verbose            Verbose output\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "This program requires a libdragon toolchain installed in $N64_INST.\n");
}

void add_export_sym(const char *name, uint32_t value, uint32_t size)
{
    dso_sym_t sym;
    sym.name = strdup(name);
    sym.value = value;
    sym.info = size & 0x3FFFFFFF;
    stbds_arrput(export_syms, sym);
}

int dso_sym_compare(const void *a, const void *b)
{
    //Sort in lexicographical order (standard strcmp uses)
    dso_sym_t *symbol_1 = (dso_sym_t *)a;
    dso_sym_t *symbol_2 = (dso_sym_t *)b;
    return strcmp(symbol_1->name, symbol_2->name);
}

void get_export_syms(char *infn)
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
    getline(&line_buf, &line_buf_size, readelf_stdout); //Blank line unless if error
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
    verbose("Grabbing exported symbols from ELF\n");
    while(getline(&line_buf, &line_buf_size, readelf_stdout) != -1) {
        char *bind_ptr = strstr(line_buf, "GLOBAL ");
        //Try searching for weak in output if symbol is not global
        if(!bind_ptr) {
            bind_ptr = strstr(line_buf, "WEAK   ");
        }
        //Include defined GLOBAL/WEAK symbols
        if(bind_ptr && strncmp(&bind_ptr[15], " UND", 4) != 0) {
            //Remove line terminator
            size_t linebuf_len = strlen(line_buf);
            line_buf[linebuf_len-1] = 0;
            if(line_buf[linebuf_len-2] == '\r') {
                line_buf[linebuf_len-2] = 0;
            }
            char *sym_name = &bind_ptr[20]; //Get symbol name pointer
            size_t sym_value = strtoull(&line_buf[8], NULL, 16); //Read symbol value
            //Read symbol size
            size_t sym_size = strtoull(&line_buf[17], NULL, 0); //Read symbol size
            add_export_sym(sym_name, sym_value, sym_size);
        }
    }
    //Free resources
    free(line_buf);
    free(readelf_bin);
    subprocess_terminate(&subp);
}

void dso_write_symbols(dso_sym_t *syms, size_t num_syms, FILE *out_file)
{
    for(size_t i=0; i<num_syms; i++) {
        w32_placeholderf(out_file, "symbol%zu", i);
        w32(out_file, syms[i].value);
        w32(out_file, syms[i].info);
    }
    for(size_t i=0; i<num_syms; i++) {
        placeholder_set_offset(out_file, ftell(out_file)-sizeof(mainexe_sym_info_t), "symbol%zu", i);
        fwrite(syms[i].name, 1, strlen(syms[i].name)+1, out_file);
    }
}

void write_mainexe_sym_info(mainexe_sym_info_t *header, FILE *out_file)
{
    fseek(out_file, 0, SEEK_SET);
    w32(out_file, header->magic);
    w32(out_file, header->size);
    w32(out_file, header->num_syms);
}

void write_msym(char *outfn)
{
    FILE *out_file = fopen(outfn, "wb");
    if(!out_file) {
        fprintf(stderr, "Cannot create file: %s\n", outfn);
        exit(1);
    }
    //Initialize main executable symbol table info
    mainexe_sym_info_t sym_info;
    sym_info.magic = DSO_MAINEXE_SYM_DATA_MAGIC;
    sym_info.size = 0;
    sym_info.num_syms =  stbds_arrlenu(export_syms);
    write_mainexe_sym_info(&sym_info, out_file);
    //Write symbol table
    dso_write_symbols(export_syms, sym_info.num_syms, out_file);
    walign(out_file, 2);
    //Correct output size
    sym_info.size = ftell(out_file)-sizeof(mainexe_sym_info_t);
    write_mainexe_sym_info(&sym_info, out_file);
    fclose(out_file);
}

void process(char *infn, char *outfn)
{
    get_export_syms(infn);
    verbose("Sorting exported symbols from ELF");
    qsort(export_syms, stbds_arrlenu(export_syms), sizeof(dso_sym_t), dso_sym_compare);
    verbose("Writing output file %s\n", outfn);
    write_msym(outfn);
}

int main(int argc, char **argv)
{
    char *infn;
    char *outfn;
    int i;
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
    for(i=1; i<argc && argv[i][0] == '-'; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            //Print help
            print_args(argv[0]);
            return 0;
        } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
            //Specify verbose flag
            verbose_flag = true;
        } else {
            //Output invalid flag warning
            fprintf(stderr, "invalid flag: %s\n", argv[i]);
            return 1;
        }
    }
    if(argc-2 > i) {
        fprintf(stderr, "Extraneous arguments present\n");
        return 1;
    }
    infn = argv[i++];
    if(i == argc) {
        fprintf(stderr, "Missing output filename\n");
        return 1;
    } else {
        outfn = argv[i++];
    }
    process(infn, outfn);
    return 0;
}