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
float flag_ttf_outline = 0;
bool flag_ttf_monochrome = false;
float flag_ttf_char_spacing = 0;
tex_format_t flag_bmfont_format = FMT_RGBA16;

std::vector<uint32_t> unicode_ranges{
    0x0000, 0x0020, 0x0080, 0x0100, 0x180, 0x250, 0x2b0, 0x300, 0x370, 0x400,
    0x500, 0x530, 0x590, 0x600, 0x700, 0x780, 0x900, 0x980, 0xa00, 0xa80,
    0xb00, 0xb80, 0xc00, 0xc80, 0xd00, 0xd80, 0xe00, 0xe80,
    0xf00, 0x1000, 0x10A0, 0x1100, 0x1200, 0x13A0, 0x1400, 0x1680, 0x16A0,
    0x1700, 0x1720, 0x1740, 0x1760, 0x1780, 0x1800, 0x1900, 0x1950, 0x19E0,
    0x1D00, 0x1E00, 0x1F00, 0x2000, 0x2070, 0x20A0, 0x20D0, 0x2100, 0x2150,
    0x2190, 0x2200, 0x2300, 0x2400, 0x2440, 0x2460, 0x2500, 0x2580, 0x25A0,
    0x2600, 0x2700, 0x27C0, 0x27F0, 0x2800, 0x2900, 0x2980, 0x2A00, 0x2B00,
    0x2E80, 0x2F00, 0x2FF0, 0x3000, 0x3040, 0x30A0, 0x3100, 0x3130, 0x3190,
    0x31A0, 0x31F0, 0x3200, 0x3300, 0x3400, 0x4DC0, 0x4E00, 0xA000, 0xA490,
    0xAC00, 0xD800, 0xDB80, 0xDC00, 0xE000, 0xF900, 0xFB00, 0xFB50, 0xFE00,
    0xFE20, 0xFE30, 0xFE50, 0xFE70, 0xFF00, 0xFFF0, 0x10000, 0x10080, 0x10100,
    0x10300, 0x10330, 0x10380, 0x10400, 0x10450, 0x10480, 0x10800, 0x1D000, 0x1D100,
    0x1D300, 0x1D400, 0x20000, 0x2F800, 0x2FA20
};

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
    fprintf(stderr, "                             Can be specified multiple times. Use \"--range all\" to extract all\n");
    fprintf(stderr, "                             glyphs in the font.\n");
    fprintf(stderr, "   --monochrome              Force monochrome output, with no aliasing (default: off)\n");
    fprintf(stderr, "   --outline <width>         Add outline to font, specifying its width in (fractional) pixels\n");
    fprintf(stderr, "   --char-spacing <width>    Add extra spacing between characters (default: 0)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "BMFont specific flags:\n");
    fprintf(stderr, "   --format <format>         Specify the output texture format. Valid options are:\n");
    fprintf(stderr, "                             RGBA16, RGBA32, CI4, CI8 (default: RGBA16)\n");
    fprintf(stderr, "\n");
}

#include "mkfont_out.cpp"
#include "mkfont_ttf.cpp"
#include "mkfont_bmfont.cpp"

int main(int argc, char *argv[])
{
    char *infn = NULL, *outfn = NULL; const char *outdir = ".";
    bool error = false;
    int compression = DEFAULT_COMPRESSION;
    bool range_all = false;

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
                if (strcmp(argv[i], "all") == 0) {
                    range_all = true;
                    continue;
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
            } else if (!strcmp(argv[i], "--outline")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                float outline;
                char extra;
                if (sscanf(argv[i], "%f%c", &outline, &extra) != 1) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
                flag_ttf_outline = outline;
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
            } else if (!strcmp(argv[i], "--char-spacing")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                float spacing;
                char extra;
                if (sscanf(argv[i], "%f%c", &spacing, &extra) != 1) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
                flag_ttf_char_spacing = spacing;
            } else if (!strcmp(argv[i], "--format")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                if (strcmp(argv[i], "RGBA16") == 0) {
                    flag_bmfont_format = FMT_RGBA16;
                } else if (strcmp(argv[i], "RGBA32") == 0) {
                    flag_bmfont_format = FMT_RGBA32;
                } else if (strcmp(argv[i], "CI4") == 0) {
                    flag_bmfont_format = FMT_CI4;
                } else if (strcmp(argv[i], "CI8") == 0) {
                    flag_bmfont_format = FMT_CI8;
                } else {
                    fprintf(stderr, "invalid format: %s\n", argv[i]);
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
        char* basename_noext = strdup(basename);
        char* ext = strrchr(basename_noext, '.');
        if (ext) *ext = '\0';

        if (range_all) {
            flag_ranges.clear();
        } else if (flag_ranges.empty()) {
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
            ret = convert_bmfont(infn, outfn);
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
            } else {
                if (flag_verbose) {
                    struct stat st = {0};
                    stat(outfn, &st);
                    printf("written: %s (%d bytes)\n", outfn, (int)st.st_size);
                }
            }
        }
        free(outfn);
    }

    return error ? 1 : 0;
}
