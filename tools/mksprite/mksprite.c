#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define LODEPNG_NO_COMPILE_ENCODER             // No need to save PNGs
#define LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS    // No need to parse PNG extra fields
#define LODEPNG_NO_COMPILE_CPP                 // No need to use C++ API
#include "lodepng.h"
#include "lodepng.c"

// Bring in sprite_t and tex_format_t definition
#include "sprite.h"
#include "surface.h"

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define LE32_TO_HOST(i) __builtin_bswap32(i)
    #define HOST_TO_LE32(i) __builtin_bswap32(i)
    #define LE16_TO_HOST(i) __builtin_bswap16(i)
    #define HOST_TO_LE16(i) __builtin_bswap16(i)

    #define BE32_TO_HOST(i) (i)
    #define HOST_TO_BE32(i) (i)
    #define LE16_TO_HOST(i) (i)
    #define HOST_TO_BE16(i) (i)
#else
    #define BE32_TO_HOST(i) __builtin_bswap32(i)
    #define HOST_TO_BE32(i) __builtin_bswap32(i)
    #define BE16_TO_HOST(i) __builtin_bswap16(i)
    #define HOST_TO_BE16(i) __builtin_bswap16(i)

    #define LE32_TO_HOST(i) (i)
    #define HOST_TO_LE32(i) (i)
    #define HOST_TO_LE16(i) (i)
    #define LE16_TO_HOST(i) (i)
#endif

const char* tex_format_name(tex_format_t fmt) {
    switch (fmt) {
    case FMT_NONE: return "AUTO";
    case FMT_RGBA32: return "RGBA32";
    case FMT_RGBA16: return "RGBA16";
    case FMT_CI8: return "CI8";
    case FMT_CI4: return "CI4";
    case FMT_I8: return "I8";
    case FMT_I4: return "I4";
    case FMT_IA8: return "IA8";
    case FMT_IA4: return "IA4";
    default: assert(0); return ""; // should not happen
    }
}

int tex_format_bytes_per_pixel(tex_format_t fmt) {
    switch (fmt) {
    case FMT_NONE: assert(0); return -1; // should not happen
    case FMT_RGBA32: return 4;
    case FMT_RGBA16: return 2;
    default: return 1;
    }
}

bool flag_verbose = false;

void print_args( char * name )
{
    fprintf(stderr, "Usage: %s [flags] <input files...>\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -v/--verbose        Verbose output\n");
    fprintf(stderr, "   -o/--output <dir>   Specify output directory (default: .)\n");
    fprintf(stderr, "   -f/--format <fmt>   Specify output format (default: AUTO)\n");
    fprintf(stderr, "   -t/--tiles  <w,h>   Specify single tile size (default: auto)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Supported formats: AUTO, RGBA32, RGBA16, CI8, I8, IA8, CI4, I4, IA4\n\n");
    fprintf(stderr, "NOTE: this tool will not quantize the input image. Make sure the input PNG\n");
    fprintf(stderr, "has the correct number of colors for the selected output format.\n");
}

uint16_t conv_rgb5551(uint8_t r8, uint8_t g8, uint8_t b8, uint8_t a8) {
    uint16_t r=r8>>3, g=g8>>3, b=b8>>3, a=a8?1:0;
    return (r<<11) | (g<<6) | (b<<1) | a;
}

int convert(const char *infn, const char *outfn, tex_format_t outfmt, int hslices, int vslices, int tilew, int tileh) {

    unsigned char* png = 0;
    size_t pngsize;
    unsigned char* image = 0;
    unsigned width, height;
    LodePNGState state;
    bool autofmt = (outfmt == FMT_NONE);
    bool inspected = false;

    // Initialize lodepng and load the input file into memory (without decoding).
    lodepng_state_init(&state);
    int error = lodepng_load_file(&png, &pngsize, infn);
    if(error) {
        fprintf(stderr, "%s: PNG reading error: %u: %s\n", infn, error, lodepng_error_text(error));
        return 1;
    }

    // Check if we're asked to autodetect the best possible texformat for output
    if (autofmt) {
        // Parse the PNG header to get some metadata
        error = lodepng_inspect(&width, &height, &state, png, pngsize);
        if(error) {
            fprintf(stderr, "%s: PNG reading error: %u: %s\n", infn, error, lodepng_error_text(error));
            return 1;
        }
        inspected = true;

        // Autodetect the best output format depending on the input format
        // The rule of thumb is that we want to preserve the information on the
        // input image as much as possible.
        switch (state.info_png.color.colortype) {
        case LCT_GREY:
            outfmt = (state.info_png.color.bitdepth >= 8) ? FMT_I8 : FMT_I4;
            break;
        case LCT_GREY_ALPHA:
            outfmt = (state.info_png.color.bitdepth >= 4) ? FMT_IA8 : FMT_IA4;
            break;
        case LCT_PALETTE:
            outfmt = FMT_CI8; // Will check if CI4 (<= 16 colors) later
            break;
        case LCT_RGB: case LCT_RGBA:
            outfmt = FMT_RGBA32;
            break;
        default:
            fprintf(stderr, "%s: unknown PNG color type: %d\n", infn, state.info_png.color.colortype);
            return 1;
        }
    }

    // Setup the info_raw structure with the desired pixel conversion,
    // depending on the output format.
    switch (outfmt) {
    case FMT_RGBA32: case FMT_RGBA16:
        // PNG does not support RGBA555 (aka RGBA16), so just convert
        // to 32-bit version we will downscale later.
        state.info_raw.colortype = LCT_RGBA;
        state.info_raw.bitdepth = 8;
        break;
    case FMT_CI8: case FMT_CI4:
        // Inspect the PNG if we haven't already
        if (!inspected) {
            error = lodepng_inspect(&width, &height, &state, png, pngsize);
            if(error) {
                fprintf(stderr, "%s: PNG reading error: %u: %s\n", infn, error, lodepng_error_text(error));
                return 1;
            }
            inspected = true;
        }
        if (state.info_png.color.colortype != LCT_PALETTE) {
            // lodepng does not support creating a palette from a non-palettized image, even
            // if the number of colors is very little
            fprintf(stderr, "%s: PNG has no palette, cannot convert to %s\n", infn, tex_format_name(outfmt));
            return 1;
        }
        // lodepng does not encode to 4bit palettized, so for now just force 8bit
        state.info_raw.colortype = LCT_PALETTE;
        state.info_raw.bitdepth = 8;
        break;
    case FMT_I8: case FMT_I4:
        state.info_raw.colortype = LCT_GREY;
        state.info_raw.bitdepth = 8;
        break;
    case FMT_IA8: case FMT_IA4:
        state.info_raw.colortype = LCT_GREY_ALPHA;
        state.info_raw.bitdepth = 8;
        break;
    default:
        assert(0); // should not happen
    }

    // Decode the PNG and do the color conversion as requested.
    // This will error out if the conversion requires downsampling / quantization,
    // as this is not supported by lodepng.
    // TODO: maybe provide quantization algorithms here?
    error = lodepng_decode(&image, &width, &height, &state, png, pngsize);
    if(error) {
        fprintf(stderr, "PNG decoding error: %u: %s\n", error, lodepng_error_text(error));
        return 1;
    }
    free(png);

    if (outfmt == FMT_CI4) {
        LodePNGColorMode newmode = lodepng_color_mode_make(LCT_PALETTE, 8);
        uint16_t outcolors[256];

        // Remove duplicated colors from the palette (or rather: colors that become
        // unique after conversion to RGBA5551). These are common when converting
        // from RGBA16/RGBA32 using tools like ImageMagick. Doing so will hopefully
        // help fitting the requested CI4 format.
        newmode.palette = malloc(state.info_png.color.palettesize * 4);
        newmode.palettesize = 0;
        for (int i=0;i<state.info_png.color.palettesize;i++) {
            uint8_t *cin = state.info_png.color.palette + i*4;
            uint16_t cin16 = conv_rgb5551(cin[0], cin[1], cin[2], cin[3]);

            bool found = false;
            for (int j=0;j<newmode.palettesize;j++) {
                if (cin16 == outcolors[j]) {
                    found = true;
                    // Remap color index in image
                    for (int x=0;x<width*height;x++)
                        if (image[x] == i)
                            image[x] = j;
                    break;
                }
            }
            if (!found) {
                uint8_t *cout = newmode.palette + newmode.palettesize*4;
                memcpy(cout, cin, 4);
                outcolors[newmode.palettesize] = cin16;
                newmode.palettesize++;
            }
        }
        if (flag_verbose)
            printf("unique palette colors: %zu (original: %zu)\n", newmode.palettesize, state.info_png.color.palettesize);
        state.info_raw = newmode;
    }

    // If we're autodetecting the output format and the PNG had a palette, go
    // through the pixels and count the colors to see if it fits CI4. 
    // We do the same also if the user explicitly selected CI4, to be able to
    // error out if the PNG has more than 16 colors.
    // We need this because lodepng doesn't support CI4 / 4-bit packing.
    if ((autofmt && outfmt == FMT_CI8) || outfmt == FMT_CI4) {
        // Check if the image fits 4bit indices
        bool is4bit = true;
        for (int i=0; i < width*height; i++) {
            if (image[i] >= 16) {
                is4bit = false;
                break;
            }
        }

        if (autofmt) {
            // In case this was an auto-format, select the correct texture format
            outfmt = is4bit ? FMT_CI4 : FMT_CI8;
        } else if (!is4bit) {
            fprintf(stderr, "PNG decoding error: image has more than 16 colors\n");
            return 1;
        }
    }

    // Autodetection complete, log it.
    if (flag_verbose && autofmt)
        printf("auto selected format: %s\n", tex_format_name(outfmt));

    // Autodetection of optimal slice size. TODO: this could be improved
    // by calculating actual memory occupation of each slice, to miminize the
    // number of TMEM loads.
    if (tilew) hslices = width / tilew;
    if (tileh) vslices = height / tileh;
    if (!hslices) {
        hslices = width / 16;
        if (flag_verbose)
            printf("auto detected hslices: %d (w=%d/%d)\n", hslices, width, width/hslices);
    }
    if (!vslices) {
        vslices = height / 16;
        if (flag_verbose)
            printf("auto detected vslices: %d (w=%d/%d)\n", vslices, height, height/vslices);
    }

    // Now we have the raw image / palette available. Prepare the sprite structure
    int bpp = tex_format_bytes_per_pixel(outfmt);
    sprite_t sprite = {0};
    sprite.width = HOST_TO_BE16(width);
    sprite.height = HOST_TO_BE16(height);
    sprite.format = outfmt;
    sprite.hslices = hslices;
    sprite.vslices = vslices;

    // Open the output file
    FILE *out = fopen(outfn, "wb");
    if (!out) {
        fprintf(stderr, "cannot create file: %s\n", outfn);
        return 1;
    }

    // Write the header
    fwrite(&sprite, 1, sizeof(sprite_t), out);

    // Write the data
    uint8_t *img = image;
    switch (outfmt) {
    case FMT_RGBA16: {
        // Convert to 16-bit RGB5551 format.
        for (int i=0;i<width*height;i++) {
            uint16_t px = conv_rgb5551(img[0], img[1], img[2], img[3]);
            fputc(px>>8, out); fputc(px, out);
            img += 4;
        }
        break;
    }

    case FMT_CI8: case FMT_CI4: {
        // Convert the palette into RGB5551 format. Notice that the original
        // PNG palette could contain less colors than we need, so we might need
        // to pad the palette with zeros.
        int fmt_colors = (outfmt == FMT_CI8) ? 256 : 16;
        LodePNGColorMode *color = &state.info_png.color;
        uint8_t black[4] = {0};
        uint8_t *pal = color->palette;
        for (int i=0; i<fmt_colors; i++) {
            uint16_t c = conv_rgb5551(pal[0], pal[1], pal[2], pal[3]);
            fputc(c>>8, out); fputc(c, out);
            pal = (i < color->palettesize) ? pal+4 : black;
        }

        if (outfmt == FMT_CI8) {
            // For 8-bit palettized, the image is already in the right format.
            fwrite(img, 1, width*height*bpp, out);
        } else {
            // Convert image to 4 bit.
            for (int i=0; i<width*height; i+=2) {
                uint8_t ix0 = *img++;
                uint8_t ix1 = *img++;
                assert(ix0 < 16 && ix1 < 16);
                fputc((ix0 << 4) | ix1, out);
            }
        }
        break;
    }

    case FMT_IA8: case FMT_I4: {
        // I4 is 4 bit intensity. IA8 is 4 bit intensity and 4 bit alpha.
        // The packing code is the same: we need to read two consecutive
        // bytes and compress them into 4 bit by keeping only the highest nibble.
        for (int i=0; i<width*height; i++) {
            uint8_t I = *img++; uint8_t A = *img++;
            fputc((I & 0xF0) | (A >> 4), out);
        }
        break;
    }

    case FMT_IA4: {
        // IA4 is 3 bit intensity and 1 bit alpha. Pack it
        for (int i=0; i<width*height; i+=2) {
            uint8_t I0 = *img++; uint8_t A0 = *img++ ? 1 : 0;
            uint8_t I1 = *img++; uint8_t A1 = *img++ ? 1 : 0;
            fputc((I0 & 0xE0) | (A0 << 4) | ((I1 & 0xE0) >> 4) | A1, out);
        }
        break;    
    }

    default:
        // No further conversion needed
        fwrite(img, 1, width*height*bpp, out);
        break;
    }

    fclose(out);
    free(image);
    lodepng_state_cleanup(&state);
    return 0;
}


int main(int argc, char *argv[])
{
    char *infn = NULL, *outdir = ".", *outfn = NULL;
    int hslices = 0, vslices = 0, tilew = 0, tileh = 0;
    tex_format_t outfmt = FMT_NONE;

    if (argc < 2) {
        print_args(argv[0]);
        return 1;
    }

    // We still support (but not document) the old mksprite command line
    // syntax: mksprite <bitdepth> [hslices vslices] input output
    if ((argc == 4 || argc == 6) && (!strcmp(argv[1], "16") || !strcmp(argv[1], "32"))) {
        int i = 1;
        outfmt = !strcmp(argv[i++], "16") ? FMT_RGBA16 : FMT_RGBA32;
        if (argc == 6) {
            hslices = atoi(argv[i++]);
            vslices = atoi(argv[i++]);
        }
        infn = argv[i++];
        outfn = argv[i++];
        printf("WARNING: deprecated command-line syntax was used, please switch to new syntax\n");
        return convert(infn, outfn, outfmt, hslices, vslices, 0, 0);
    }

    bool error = false;
     
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
                flag_verbose = true;
            } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                outdir = argv[i];
            } else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--format")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                if (!strcmp(argv[i], "RGBA32")) outfmt = FMT_RGBA32;
                else if (!strcmp(argv[i], "RGBA16")) outfmt = FMT_RGBA16;
                else if (!strcmp(argv[i], "CI8")) outfmt = FMT_CI8;
                else if (!strcmp(argv[i], "I8")) outfmt = FMT_I8;
                else if (!strcmp(argv[i], "IA8")) outfmt = FMT_IA8;
                else if (!strcmp(argv[i], "CI4")) outfmt = FMT_CI4;
                else if (!strcmp(argv[i], "I4")) outfmt = FMT_I4;
                else if (!strcmp(argv[i], "IA4")) outfmt = FMT_IA4;
                else if (!strcmp(argv[i], "AUTO")) outfmt = FMT_NONE;
                else {
                    fprintf(stderr, "invalid argument for --format: %s\n", argv[i]);
                    return 1;
                }
            } else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--tiles")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                char extra;
                if (sscanf(argv[i], "%d,%d%c", &tilew, &tileh, &extra) != 2) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
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

        asprintf(&outfn, "%s/%s.sprite", outdir, basename_noext);
        if (flag_verbose)
            printf("Converting: %s -> %s [fmt=%s tiles=%d,%d]\n",
                infn, outfn, tex_format_name(outfmt), tilew, tileh);
        if (convert(infn, outfn, outfmt, 0, 0, tilew, tileh) != 0)
            error = true;
        free(outfn);
    }

    return error ? 1 : 0;
}
