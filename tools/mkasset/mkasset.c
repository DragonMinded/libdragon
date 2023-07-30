#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../common/assetcomp.h"

bool flag_verbose = false;

void print_args(char * name)
{
    fprintf(stderr, "%s -- Libdragon asset compression tool\n\n", name);
    fprintf(stderr, "This tool can be used to compress/decompress arbitrary asset files in a format\n");
    fprintf(stderr, "that can be loaded by the libdragon library. To open the compressed\n");
    fprintf(stderr, "files, use asset_open() or asset_load().\n\n");
    fprintf(stderr, "Usage: %s [flags] <input files...>\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -v/--verbose          Verbose output\n");
    fprintf(stderr, "   -o/--output <dir>     Specify output directory (default: .)\n");
    fprintf(stderr, "   -c/--compress <algo>  Compression: 0=none, 1=lha, 2=lzh5 (default: %d)\n", DEFAULT_COMPRESSION);
    fprintf(stderr, "\n");
}

int main(int argc, char *argv[])
{
    char *infn = NULL, *outdir = ".", *outfn = NULL;
    int compression = DEFAULT_COMPRESSION;

    if (argc < 2) {
        print_args(argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                print_args(argv[0]);
                return 0;
            } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
                flag_verbose = true;
            } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                outdir = argv[i];
            } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--compress")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                char extra;
                if (sscanf(argv[i], "%d%c", &compression, &extra) != 1) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
                if (compression < 0 || compression > 2) {
                    fprintf(stderr, "invalid compression algorithm: %d\n", compression);
                    return 1;
                }
            } else {
                fprintf(stderr, "invalid flag: %s\n", argv[i]);
                return 1;
            }
            continue;
        }

        infn = argv[i];
        char *basename = strrchr(infn, '/');
        if (!basename) basename = infn; else basename += 1;

        asprintf(&outfn, "%s/%s", outdir, basename);

        if (flag_verbose)
            printf("Compressing: %s => %s [algo=%d]\n", infn, outfn, compression);

        asset_compress(infn, outfn, compression);

        free(outfn);
    }
}
