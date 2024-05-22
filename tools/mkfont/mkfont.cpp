#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <assert.h>
#include <vector>

#include "../../include/surface.h"
#include "../../src/rdpq/rdpq_font_internal.h"

// LodePNG
#define LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS    // No need to parse PNG extra fields
#define LODEPNG_NO_COMPILE_CPP                 // No need to use C++ API
#include "../common/lodepng.h"
#include "../common/lodepng.c"

// Rect packing
#include "rect_pack.cpp"

// Compression library
#include "../common/assetcomp.h"

#include "../common/binout.c"
#include "../common/binout.h"
#include "../common/subprocess.h"
#include "../common/utils.h"
#include "../common/polyfill.h"

int flag_verbose = 0;
bool flag_debug = false;
bool flag_kerning = true;
int flag_ttf_point_size = 0;
std::vector<int> flag_ranges;
const char *n64_inst = NULL;
int flag_ellipsis_cp = 0x002E;
int flag_ellipsis_repeats = 3;
bool flag_ttf_outline = false;
bool flag_ttf_monochrome = false;

void print_args( char * name )
{
    fprintf(stderr, "mkfont -- Convert TTF/OTF/BMFont fonts into the font64 format for libdragon\n\n");
    fprintf(stderr, "Usage: %s [flags] <input files...>\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -o/--output <dir>         Specify output directory (default: .)\n");
    fprintf(stderr, "   -v/--verbose              Verbose output\n");
    fprintf(stderr, "   --no-kerning              Do not export kerning information\n");
    fprintf(stderr, "   --ellipsis <cp>,<reps>    Select glyph and repetitions to use for ellipsis (default: 2E,3) \n");
    fprintf(stderr, "   -c/--compress <level>     Compress output files (default: %d)\n", DEFAULT_COMPRESSION);
    fprintf(stderr, "   -d/--debug                Dump also debug images\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "TTF/OTF specific flags:\n");
    fprintf(stderr, "   -s/--size <pt>            Point size of the font (default: whatever the font defaults to)\n");
    fprintf(stderr, "   -r/--range <start-stop>   Range of unicode codepoints to convert, as hex values (default: 20-7F)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "BMFont specific flags:\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "It is possible to convert multiple ranges of codepoints, by specifying\n");
    fprintf(stderr, "--range more than one time.\n");
}

#include "mkfont_out.cpp"
#include "mkfont_ttf.cpp"
// #include "mkfont_bmfont.cpp"

int main(int argc, char *argv[])
{
    char *infn = NULL, *outfn = NULL; const char *outdir = ".";
    bool error = false;
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
                flag_verbose++;
            } else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
                flag_debug = true;
            } else if (!strcmp(argv[i], "--no-kerning")) {
                flag_kerning = false;
            } else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--size")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                char extra;
                if (sscanf(argv[i], "%d%c", &flag_ttf_point_size, &extra) != 1) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
            } else if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--range")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                int r0, r1;
                char extra;
                if (sscanf(argv[i], "%x-%x%c", &r0, &r1, &extra) != 2) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
                flag_ranges.push_back(r0);
                flag_ranges.push_back(r1);
            } else if (!strcmp(argv[i], "--monochrome")) {
                flag_ttf_monochrome = true;
            } else if (!strcmp(argv[i], "--ellipsis")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                int cp, repeats;
                char extra;
                if (sscanf(argv[i], "%x,%d%c", &cp, &repeats, &extra) != 2) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
                flag_ellipsis_cp = cp;
                flag_ellipsis_repeats = repeats;
            } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--compress")) {
                // Optional compression level
                if (i+1 < argc && argv[i+1][1] == 0) {
                    int level = argv[i+1][0] - '0';
                    if (level >= 0 && level <= 3) {
                        compression = level;
                        i++;
                    }
                    else {
                        fprintf(stderr, "invalid compression level: %s\n", argv[i+1]);
                        return 1;
                    }
                }
            } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                outdir = argv[i];
            } else {
                fprintf(stderr, "invalid flag: %s\n", argv[i]);
                return 1;
            }
            continue;
        }

        infn = argv[i];
        char *basename = strrchr(infn, '/');
        if (!basename) basename = infn; else basename += 1;
        char* basename_noext = strdup(basename);
        char* ext = strrchr(basename_noext, '.');
        if (ext) *ext = '\0';

        if (flag_ranges.empty()) {
            // Default range (ASCII)
            flag_ranges.push_back(0x20);
            flag_ranges.push_back(0x7F);
        }

        // Find n64 tool directory
        if (!n64_inst) {
            n64_inst = n64_tools_dir();
            if (!n64_inst) {
                fprintf(stderr, "Error: N64_INST environment variable not set\n");
                return 1;
            }
        }

        asprintf(&outfn, "%s/%s.font64", outdir, basename_noext);
        if (flag_verbose)
            printf("Converting: %s -> %s\n",
                infn, outfn);
        
        int ret;
        if (strcasestr(infn, ".ttf") || strcasestr(infn, ".otf")) {
            ret = convert_ttf(infn, outfn, flag_ranges);
        } else if (strcasestr(infn, ".fnt")) {
            // ret = convert_bmfont(infn, outfn);
            assert(!"BMFont support is not implemented yet");
        } else {
            fprintf(stderr, "Error: unknown input file type: %s\n", infn);
            ret = 1;
        }

        if (ret != 0) {
            error = true;
        } else {
            if (compression) {
                struct stat st_decomp = {0}, st_comp = {0};
                stat(outfn, &st_decomp);
                asset_compress(outfn, outfn, compression, 0);
                stat(outfn, &st_comp);
                if (flag_verbose)
                    printf("compressed: %s (%d -> %d, ratio %.1f%%)\n", outfn,
                    (int)st_decomp.st_size, (int)st_comp.st_size, 100.0 * (float)st_comp.st_size / (float)(st_decomp.st_size == 0 ? 1 :st_decomp.st_size));
            }
        }
        free(outfn);
    }

    return error ? 1 : 0;
}
