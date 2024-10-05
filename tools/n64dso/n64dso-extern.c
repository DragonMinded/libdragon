#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

//Asset decompression
#include "../../src/asset.c"
#include "../../src/compress/aplib_dec.c"
#include "../../src/compress/lz4_dec.c"
#include "../../src/compress/ringbuf.c"
#include "../../src/compress/shrinkler_dec.c"

//DSO Format Internals
#include "../../src/dso_format.h"

bool verbose_flag = false;

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
    fprintf(stderr, "%s - Output list of undefined symbols in all DSOs\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: %s [flags] [<input_dsos>]\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -v/--verbose            Verbose output\n");
    fprintf(stderr, "   -o/--output <file>      Specify output file (default stdout)\n");
    fprintf(stderr, "\n");
}

uint32_t read_buf_u32(void *buf)
{
	uint8_t *temp = buf;
	//Read 4 bytes from buffer as big-endian 32-bit integer
	return (temp[0] << 24)|(temp[1] << 16)|(temp[2] << 8)|temp[3];
}

void write_externs(uint8_t *dso_sym_table, uint8_t *name_base, uint32_t num_externs, FILE *out_file)
{
    dso_sym_table += DSO_SYM_SIZE;
	//Iterate through each external symbol and output their name to out_file
	for(uint32_t i=0; i<num_externs; i++) {
		fprintf(out_file, "EXTERN(%s)\n", name_base+read_buf_u32(dso_sym_table));
        dso_sym_table += DSO_SYM_SIZE;
	}
}

void process(const char *infn, FILE *out_file)
{
	int sz;
    verbose("Processing DSO %s\n", infn);
	//Load DSO file
	uint8_t *data = asset_load(infn, &sz);
	uint8_t *orig_data = data;
	//Do basic sanity checks on DSO file
	if(sz < 84 || read_buf_u32(data) != DSO_MAGIC) {
		fprintf(stderr, "File is not a valid DSO file\n");
		exit(1);
	}
	//Write data externs
	verbose("Writing external symbols in DSO to output file");
	write_externs(data+read_buf_u32(data+DSO_SYMS_OFS), data, read_buf_u32(data+DSO_NUM_IMPORT_SYMS_OFS), out_file);
	//Free DSO file data
	free(orig_data);
}

int main(int argc, char **argv)
{
    FILE *out_file = stdout;
    if(argc < 2) {
        //Print usage if too few arguments are passed
        print_args(argv[0]);
        return 1;
    }
    asset_init_compression(2);
    asset_init_compression(3);
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
                out_file = fopen(argv[i], "w");
                if(!out_file) {
                    //Output error if file cannot be opened
                    fprintf(stderr, "Cannot create file: %s\n", argv[i]);
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