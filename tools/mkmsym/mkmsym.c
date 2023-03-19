#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <malloc.h>
#include "../common/subprocess.h"
#include "../common/polyfill.h"
#include "../common/binout.h"

#define STBDS_NO_SHORT_NAMES
#define STB_DS_IMPLEMENTATION
#include "../common/stb_ds.h"

//USO Symbol Table Internals
#include "../../src/uso_format.h"

struct { char *key; size_t value; } *imports_hash = NULL;

uso_sym_t *export_syms = NULL;

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
    fprintf(stderr, "   -a/--all                Export all global symbols from input ELF\n");
    fprintf(stderr, "   -i/--imports <file>     Specify list of imported symbols\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "This program requires a libdragon toolchain installed in $N64_INST.\n");
}

bool import_exists(const char *name)
{
    if(!imports_hash) {
        return false;
    }
    return stbds_shget(imports_hash, name) >= 0;
}

void add_import(const char *name)
{
    if(!imports_hash) {
        stbds_sh_new_arena(imports_hash);
        stbds_shdefault(imports_hash, -1);
    }
    if(!import_exists(name)) {
        stbds_shput(imports_hash, name, stbds_shlenu(imports_hash));
    }
}

void parse_imports(const char *filename)
{
    char *line_buf = NULL;
    size_t line_buf_size = 0;
    //Try opening file
    FILE *file = fopen(filename, "r");
    if(!file) {
        fprintf(stderr, "Cannot open file: %s\n", filename);
        return;
    }
    while(getline(&line_buf, &line_buf_size, file) != -1) {
        //Find start and end of relevant parts of line
        char *extern_start = strstr(line_buf, "EXTERN(");
        char *close_brace = strrchr(line_buf, ')');
        if(extern_start && close_brace) {
            *close_brace = 0; //Terminate symbol name before closing brace
            add_import(&extern_start[7]); //Symbol name starts after EXTERN(
        }
    }
    //Close imports file
    fclose(file);
}

size_t parse_hex(char *buf, size_t length)
{
    char temp_buf[17];
    if(length > 16) {
        strncpy(temp_buf, buf, 16);
        length = 16;
    } else {
        strncpy(temp_buf, buf, length);
    }
    temp_buf[length] = 0;
    return strtoul(temp_buf, NULL, 16);
}

size_t parse_decimal(char *buf, size_t length)
{
    char temp_buf[21];
    if(length > 20) {
        strncpy(temp_buf, buf, 20);
        length = 20;
    } else {
        strncpy(temp_buf, buf, length);
    }
    temp_buf[length] = 0;
    return strtoul(temp_buf, NULL, 10);
}

void cleanup_imports()
{
    if(!imports_hash) {
        return;
    }
    for(size_t i=0; i<stbds_shlenu(imports_hash); i++) {
        free(imports_hash[i].key);
    }
    stbds_shfree(imports_hash);
}

void add_export_sym(const char *name, uint32_t value, uint32_t size)
{
    uso_sym_t sym;
    sym.name = strdup(name);
    sym.value = value;
    sym.info = size & 0x7FFFFF;
    stbds_arrput(export_syms, sym);
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
    getline(&line_buf, &line_buf_size, readelf_stdout); //Symbol table format
    //Read symbol table output from readelf
    verbose("Grabbing exported symbols from ELF\n");
    while(getline(&line_buf, &line_buf_size, readelf_stdout) != -1) {
        char *global_ptr = strstr(line_buf, "GLOBAL");
        if(global_ptr) {
            //Remove line terminator
            size_t linebuf_len = strlen(line_buf);
            line_buf[linebuf_len-1] = 0;
            char *sym_name = &global_ptr[20]; //Get symbol name pointer
            size_t sym_value = parse_hex(&line_buf[8], 8); //Read symbol value
            //Read symbol size
            size_t sym_size;
            if(!strncmp(&line_buf[17], "0x", 2)) {
                verbose("Found symbol with size bigger than 99999\n");
                //Symbol size in hex prefixed by 0x
                char *space = strchr(&line_buf[17], ' ');
                sym_size = parse_hex(&line_buf[19], space-line_buf-19);
            } else {
                //Symbol size specified by 5 decimal digits
                sym_size = parse_decimal(&line_buf[17], 5);
            }
            if(export_all || import_exists(sym_name)) {
                add_export_sym(sym_name, sym_value, sym_size);
            }
        }
    }
    //Free resources
    free(line_buf);
    subprocess_terminate(&subp);
}

uso_file_sym_t uso_generate_file_sym(uso_sym_t *sym)
{
    uso_file_sym_t temp;
    temp.name_ofs = 0; //Placeholder
    temp.value = sym->value;
    temp.info = sym->info;
    return temp;
}

void uso_write_file_sym(uso_file_sym_t *file_sym, uint32_t offset, FILE *out)
{
    fseek(out, offset, SEEK_SET);
    w32(out, file_sym->name_ofs);
    w32(out, file_sym->value);
    w32(out, file_sym->info);
}

void uso_write_file_sym_table(uso_file_sym_table_t *file_sym_table, uint32_t offset, FILE *out)
{
    fseek(out, offset, SEEK_SET);
    w32(out, file_sym_table->size);
    w32(out, file_sym_table->data_ofs);
}

uint32_t uso_write_syms(uso_sym_t *sym_list, uint32_t num_syms, uint32_t offset, FILE *out)
{
    uint32_t name_ofs = num_syms*sizeof(uso_file_sym_t);
    for(uint32_t i=0; i<num_syms; i++) {
        uso_file_sym_t file_sym = uso_generate_file_sym(&sym_list[i]);
        size_t name_len = strlen(sym_list[i].name);
        file_sym.name_ofs = name_ofs;
        uso_write_file_sym(&file_sym, offset+(i*sizeof(uso_file_sym_t)), out);
        //Write name and null terminator
        fseek(out, offset+name_ofs, SEEK_SET);
        fwrite(sym_list[i].name, name_len, 1, out);
        w8(out, 0);
        //Allocate room for next string
        name_ofs += name_len+1;
    }
    //Pad to 2-byte boundary
    if(name_ofs % 2 != 0) {
        fseek(out, offset+name_ofs, SEEK_SET);
        w8(out, 0);
        name_ofs++;
    }
    return offset+name_ofs;
}

void write_mainexe_sym_header(mainexe_sym_info_t *header, uint32_t offset, FILE *out)
{
    fseek(out, offset, SEEK_SET);
    w32(out, header->magic);
    w32(out, header->size);
}

void write_msym(char *outfn)
{
    FILE *out = fopen(outfn, "wb");
    mainexe_sym_info_t sym_header;
    uso_file_sym_table_t file_sym_table;
    if(!out) {
        fprintf(stderr, "Cannot create file: %s\n", outfn);
        exit(1);
    }
    //Initialize main symbol table header
    sym_header.magic = USO_MAINEXE_SYM_DATA_MAGIC;
    sym_header.size = 0;
    write_mainexe_sym_header(&sym_header, 0, out);
    //Initialize symbol table parameters
    file_sym_table.size = stbds_arrlenu(export_syms);
    file_sym_table.data_ofs = sizeof(uso_file_sym_table_t);
    uso_write_file_sym_table(&file_sym_table, sizeof(mainexe_sym_info_t), out);
    //Write symbol table
    sym_header.size = uso_write_syms(export_syms, file_sym_table.size, sizeof(mainexe_sym_info_t)+file_sym_table.data_ofs, out);
    //Correct output size
    sym_header.size -= sizeof(mainexe_sym_info_t);
    write_mainexe_sym_header(&sym_header, 0, out);
    fclose(out);
}
void process(char *infn, char *outfn)
{
    get_export_syms(infn);
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
        } else if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--all")) {
            export_all = true;
        } else if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--imports")) {
            //Specify output file
            if(++i == argc) {
                fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                return 1;
            }
            parse_imports(argv[i]);
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
    cleanup_imports();
    return 0;
}