#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include "../common/binout.h"
#include "exoquant.h"

#define LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS    // No need to parse PNG extra fields
#define LODEPNG_NO_COMPILE_CPP                 // No need to use C++ API
#include "lodepng.h"
#include "lodepng.c"

// Quantization library
#include "exoquant.h"
#include "exoquant.c"

// Compression library
#include "../common/assetcomp.h"
#include "../common/assetcomp.c"

// Bring in tex_format_t definition
#include "surface.h"
#include "sprite.h"

#define ROUND_UP(n, d) ({ \
	typeof(n) _n = n; typeof(d) _d = d; \
	(((_n) + (_d) - 1) / (_d) * (_d)); \
})

const char* tex_format_name(tex_format_t fmt) {
    switch (fmt) {
    case FMT_NONE: return "AUTO";
    case FMT_RGBA32: return "RGBA32";
    case FMT_RGBA16: return "RGBA16";
    case FMT_CI8: return "CI8";
    case FMT_CI4: return "CI4";
    case FMT_I8: return "I8";
    case FMT_I4: return "I4";
    case FMT_IA16: return "IA16";
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

#define MIPMAP_ALGO_NONE  0
#define MIPMAP_ALGO_BOX   1

const char *mipmap_algo_name(int algo) {
    switch (algo) {
    case MIPMAP_ALGO_NONE: return "NONE";
    case MIPMAP_ALGO_BOX: return "BOX";
    default: assert(0); return "";
    }
}

#define DITHER_ALGO_NONE     0
#define DITHER_ALGO_RANDOM   1
#define DITHER_ALGO_ORDERED  2

const char *dither_algo_name(int algo) {
    switch (algo) {
    case DITHER_ALGO_NONE: return "NONE";
    case DITHER_ALGO_RANDOM: return "RANDOM";
    case DITHER_ALGO_ORDERED: return "ORDERED";
    default: assert(0); return "";
    }
}


typedef struct {
    tex_format_t outfmt;
    int hslices;
    int vslices;
    int tilew;
    int tileh;
    int mipmap_algo;
    int dither_algo;
} parms_t;


bool flag_verbose = false;
bool flag_debug = false;

void print_supported_formats(void) {
    fprintf(stderr, "Supported formats: AUTO, RGBA32, RGBA16, IA16, CI8, I8, IA8, CI4, I4, IA4\n");
}

void print_supported_mipmap(void) {
    fprintf(stderr, "Supported mipmap algorithms: NONE (disable), BOX\n");
}

void print_supported_dithers(void) {
    fprintf(stderr, "Supported dithering algorithms: NONE (disable), RANDOM, ORDERED. \nNote that dithering is only applied while quantizing an image.\n");
}

void print_args( char * name )
{
    fprintf(stderr, "Usage: %s [flags] <input files...>\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -v/--verbose          Verbose output\n");
    fprintf(stderr, "   -o/--output <dir>     Specify output directory (default: .)\n");
    fprintf(stderr, "   -f/--format <fmt>     Specify output format (default: AUTO)\n");
    fprintf(stderr, "   -t/--tiles  <w,h>     Specify single tile size (default: auto)\n");
    fprintf(stderr, "   -m/--mipmap <algo>    Calculate mipmap levels using the specified algorithm (default: NONE)\n");
    fprintf(stderr, "   -D/--dither <dither>  Dithering algorithm (default: NONE)\n");
    fprintf(stderr, "   -c/--compress         Compress output files (using mksasset)\n");
    fprintf(stderr, "   -d/--debug            Dump computed images (eg: mipmaps) as PNG files in output directory\n");
    fprintf(stderr, "\n");
    print_supported_formats();
    print_supported_mipmap();
    print_supported_dithers();
}

uint16_t conv_rgb5551(uint8_t r8, uint8_t g8, uint8_t b8, uint8_t a8) {
    uint16_t r=r8>>3, g=g8>>3, b=b8>>3, a=a8?1:0;
    return (r<<11) | (g<<6) | (b<<1) | a;
}

int calc_tmem_usage(tex_format_t fmt, int width, int height)
{
    int pitch = ROUND_UP(TEX_FORMAT_PIX2BYTES(fmt, width), 8);
    int usage = pitch*height;

    // Palettized images can use only half of the TMEM, so double the TMEM usage
    if (fmt == FMT_CI4 || fmt == FMT_CI8)
        usage *= 2;

    return usage;
}

const char *colortype_to_string(LodePNGColorType ct) {
    switch (ct) {
    case LCT_GREY: return "LCT_GREY";
    case LCT_RGB: return "LCT_RGB";
    case LCT_PALETTE: return "LCT_PALETTE";
    case LCT_GREY_ALPHA: return "LCT_GREY_ALPHA";
    case LCT_RGBA: return "LCT_RGBA";
    default: assert(0); return "";
    }
}

typedef struct {
    uint8_t *image;
    int width, height;
    LodePNGColorType ct;
} image_t;

typedef struct {
    const char *infn;       // Input file
    const char *outfn;      // Output file
    image_t images[8];      // Pixel images (one per lod level)
    int num_images;         // Number of images
    uint8_t colors[256][4]; // Color palette
    int num_colors;         // Number of colors in palette
    int used_colors;        // Number of colors actually used in palette
    tex_format_t outfmt;    // Output format of the sprite
    int vslices;            // Number of vertical slices (deprecated API for old rdp.c)
    int hslices;            // Number of horizontal slices (deprecated API for old rdp.c)
} spritemaker_t;


bool spritemaker_load_png(spritemaker_t *spr, tex_format_t outfmt)
{
    LodePNGState state;
    bool autofmt = (outfmt == FMT_NONE);
    unsigned char* png = 0;
    size_t pngsize;
    unsigned char* image = 0;
    unsigned width, height;
    bool inspected = false;

    // Initialize lodepng and load the input file into memory (without decoding).
    lodepng_state_init(&state);

    int error = lodepng_load_file(&png, &pngsize, spr->infn);
    if(error) {
        fprintf(stderr, "%s: PNG reading error: %u: %s\n", spr->infn, error, lodepng_error_text(error));
        goto error;
    }

    // Check if we're asked to autodetect the best possible texformat for output
    if (autofmt) {
        // Parse the PNG header to get some metadata
        error = lodepng_inspect(&width, &height, &state, png, pngsize);
        if(error) {
            fprintf(stderr, "%s: PNG reading error: %u: %s\n", spr->infn, error, lodepng_error_text(error));
            goto error;
        }
        inspected = true;

        // Autodetect the best output format depending on the input format
        // The rule of thumb is that we want to preserve the information on the
        // input image as much as possible.
        switch (state.info_png.color.colortype) {
        case LCT_GREY:
            outfmt = (state.info_png.color.bitdepth > 4) ? FMT_I8 : FMT_I4;
            break;
        case LCT_GREY_ALPHA:
            if (state.info_png.color.bitdepth < 4) outfmt = FMT_IA4;
            else if (state.info_png.color.bitdepth < 8) outfmt = FMT_IA8;
            else outfmt = FMT_IA16;
            break;
        case LCT_PALETTE:
            outfmt = FMT_CI8; // Will check if CI4 (<= 16 colors) later
            break;
        case LCT_RGB: case LCT_RGBA:
            // Usage of 32-bit sprites/textures is extremely rare because of the
            // limited TMEM size. Default to 16-bit here, even though this might
            // cause some banding to appear.
            outfmt = FMT_RGBA16;
            break;
        default:
            fprintf(stderr, "%s: unknown PNG color type: %d\n", spr->infn, state.info_png.color.colortype);
            goto error;
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
    case FMT_CI8: case FMT_CI4: {
        // Inspect the PNG if we haven't already
        if (!inspected) {
            error = lodepng_inspect(&width, &height, &state, png, pngsize);
            if(error) {
                fprintf(stderr, "%s: PNG reading error: %u: %s\n", spr->infn, error, lodepng_error_text(error));
                goto error;
            }
            inspected = true;
        }
        if (state.info_png.color.colortype != LCT_PALETTE) {
            // If the original is not a palettized format, we need to run our quantization engine.
            // Expand to RGBA for now.
            state.info_raw.colortype = LCT_RGBA;
            state.info_raw.bitdepth = 8;
        } else {
            // Keep the current palette so that we respect the existing colormap.
            // Notice lodepng does not encode to 4bit palettized, so for now just force 8bit,
            // and will later change it back to CI4 if needed/possible.
            state.info_raw.colortype = LCT_PALETTE;
            state.info_raw.bitdepth = 8;
        }
    }   break;
    case FMT_I8: case FMT_I4:
        state.info_raw.colortype = LCT_GREY;
        state.info_raw.bitdepth = 8;
        break;
    case FMT_IA16: case FMT_IA8: case FMT_IA4:
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
        goto error;
    }

    // Copy the image into the output
    spr->images[0] = (image_t){
        .image = image,
        .width = width,
        .height = height,
        .ct = state.info_raw.colortype,
    };
    spr->num_images++;

    if(flag_verbose)
        printf("loaded %s (%dx%d, %s)\n", spr->infn, width, height, colortype_to_string(state.info_png.color.colortype));

    // For a palettized image, copy the palette and also count the number of actually
    // used colors (aka, the highest index used in the image). This is useful later for
    // some heuristics.
    if (state.info_raw.colortype == LCT_PALETTE) {
        memcpy(spr->colors, state.info_png.color.palette, state.info_png.color.palettesize * 4);
        spr->num_colors = state.info_png.color.palettesize;
        spr->used_colors = 0;
        for (int i=0; i < width*height; i++) {
            if (image[i] > spr->used_colors)
                spr->used_colors = image[i];
        }
        if (flag_verbose)
            printf("palette: %d colors (used: %d)\n", spr->num_colors, spr->used_colors);
    }

    // In case we'autodetecting the output format and the PNG had a palette, and only
    // indices 0-15 are used, we can use a FMT_CI4.
    if (autofmt && state.info_raw.colortype == LCT_PALETTE && spr->used_colors <= 16)
        outfmt = FMT_CI4;

    // Autodetection complete, log it.
    if (flag_verbose && autofmt)
        printf("auto selected format: %s\n", tex_format_name(outfmt));
    spr->outfmt = outfmt;
    
    return true;

error:
    lodepng_state_cleanup(&state);
    if (png) lodepng_free(png);
    return false;
}

void spritemaker_calc_lods(spritemaker_t *spr, int algo) {
    // Calculate mipmap levels
    assert(algo == MIPMAP_ALGO_BOX);

    // Calculate TMEM size for the image
    int tmem_usage = calc_tmem_usage(spr->outfmt, spr->images[0].width, spr->images[0].height);
    if (tmem_usage > 4096) {
        fprintf(stderr, "WARNING: image does not fit in TMEM; are you sure you want to have mipmaps for this?");
    }
    bool done = false;
    image_t *prev = &spr->images[0];
    for (int i=1;i<8 && !done;i++) {
        int mw = prev->width / 2, mh = prev->height / 2;
        if (mw < 4) break;
        tmem_usage += calc_tmem_usage(spr->outfmt, mw, mh);
        if (tmem_usage > 4096) {
            if (flag_verbose)
                printf("mipmap: stopping because TMEM full (%d)", tmem_usage);
            break;
        }
        uint8_t *mipmap = NULL;
        switch (prev->ct) {
        case LCT_RGBA:
            mipmap = malloc(mw * mh * 4);
            for (int y=0;y<mh;y++) {
                uint8_t *src1 = prev->image + y*prev->width*4*2;
                uint8_t *src2 = src1 + prev->width*4;
                uint8_t *dst = mipmap + y*mw*4;
                for (int x=0;x<mw;x++) {
                    dst[0] = (src1[0] + src1[4] + src2[0] + src2[4]) / 4;
                    dst[1] = (src1[1] + src1[5] + src2[1] + src2[5]) / 4;
                    dst[2] = (src1[2] + src1[6] + src2[2] + src2[6]) / 4;
                    dst[3] = (src1[3] + src1[7] + src2[3] + src2[7]) / 4;
                    dst += 4; src1 += 8; src2 += 8;
                }
            }
            break;
        default:
            fprintf(stderr, "WARNING: mipmap calculation for format %s not implemented yet", tex_format_name(spr->outfmt));
            done = true;
            break;
        }
        if(!done) {
            if (flag_verbose)
                printf("mipmap: generated %dx%d\n", mw, mh);
            spr->images[spr->num_images++] = (image_t){
                .image = mipmap,
                .width = mw,
                .height = mh,
                .ct = prev->ct,
            };
            prev = &spr->images[spr->num_images-1];
        }
    }
}

bool spritemaker_expand_rgba(spritemaker_t *spr) {
    for (int i=0; i<spr->num_images; i++) {
        image_t *img = &spr->images[i];
        if (img->ct == LCT_RGBA)
            continue;
        if (flag_verbose)
            printf("expanding image %d to RGBA\n", i);
        uint8_t *rgba = malloc(img->width * img->height * 4);
        switch (img->ct) {
        case LCT_PALETTE:
            for (int y=0; y<img->height; y++) {
                for (int x=0; x<img->width; x++) {
                    uint8_t *src = img->image + y*img->width + x;
                    uint8_t *dst = rgba + (y*img->width + x) * 4;
                    uint8_t *pal = spr->colors[*src];
                    dst[0] = pal[0];
                    dst[1] = pal[1];
                    dst[2] = pal[2];
                    dst[3] = pal[3];
                }
            }
            break;
        default:
            fprintf(stderr, "ERROR: unsupported color type %d\n", img->ct);
            return false;
        }
        free(img->image);
        img->image = rgba;
        img->ct = LCT_RGBA;
    }
    // Clear the palette data as it's not used anymore
    memset(spr->colors, 0, sizeof(spr->colors));
    spr->num_colors = 0;
    spr->used_colors = 0;
    return true;
}

bool spritemaker_quantize(spritemaker_t *spr, int num_colors, int dither) {
    if (flag_verbose)
        printf("quantizing image(s) to %d colors\n", num_colors);

    // Initialize the quantizer engine
    exq_data *exq = exq_init();
    exq->numBitsPerChannel = 5;   // force calculations using rgb555

    // Feed the input images, so that all of them will be quantized at once
    // using the same palette.
    for (int i=0; i<spr->num_images; i++) {
        if (spr->images[i].ct != LCT_RGBA) {
            fprintf(stderr, "ERROR: image %d is not RGBA\n", i);
            goto error;
        }
        exq_feed(exq, spr->images[i].image, spr->images[i].width * spr->images[i].height);
    }

    // Run quantization (high quality mode)
    exq_quantize_hq(exq, num_colors);

    // Extract the palette
    exq_get_palette(exq, spr->colors[0], num_colors);
    spr->num_colors = num_colors;
    spr->used_colors = num_colors;

    // Remap the images to the new palette
    for (int i=0; i<spr->num_images; i++) {
        image_t *img = &spr->images[i];
        uint8_t* ci_image = malloc(img->width * img->height);
        switch (dither) {
        case DITHER_ALGO_NONE:
            exq_map_image(exq, img->width * img->height, img->image, ci_image);
            break;
        case DITHER_ALGO_RANDOM:
            exq_map_image_random(exq, img->width * img->height, img->image, ci_image);
            break;
        case DITHER_ALGO_ORDERED:
            exq_map_image_ordered(exq, img->width, img->height, img->image, ci_image);
            break;
        default:
            fprintf(stderr, "ERROR: invalid dithering mode %d\n", dither);
            goto error;
        }
        free(img->image);
        img->image = ci_image;
        img->ct = LCT_PALETTE;
    }

    exq_free(exq);
    return true;

error:
    exq_free(exq);
    return false;
}

bool spritemaker_write(spritemaker_t *spr) {
    FILE *out = fopen(spr->outfn, "wb");
    if (!out) {
        fprintf(stderr, "ERROR: cannot open output file %s\n", spr->outfn);
        return false;
    }

    // Write the sprite header
    int bpp = tex_format_bytes_per_pixel(spr->outfmt);
    w16(out, spr->images[0].width);
    w16(out, spr->images[0].height);
    w8(out, 0); // deprecated field
    w8(out, (uint8_t)(spr->outfmt | SPRITE_FLAGS_EXT));
    w8(out, spr->hslices);
    w8(out, spr->vslices);

    uint32_t w_palpos = 0;
    uint32_t w_lodpos[7] = {0};

    // Process the images (the first always exists)
    for (int m=0; m<spr->num_images; m++) {
        image_t *image = &spr->images[m];

        if (m > 0) {
            assert(w_lodpos[m-1] != 0); // we should have left a placeholder for this LOD
            w32_at(out, w_lodpos[m-1], ftell(out));
        }

        switch (spr->outfmt) {
        case FMT_RGBA16: {
            assert(image->ct == LCT_RGBA);
            // Convert to 16-bit RGB5551 format.
            uint8_t *img = image->image;
            for (int i=0;i<image->width*image->height;i++) {
                w16(out, conv_rgb5551(img[0], img[1], img[2], img[3]));
                img += 4;
            }
            break;
        }

        case FMT_CI4: {
            assert(image->ct == LCT_PALETTE);
            assert(spr->used_colors <= 16);
            // Convert image to 4 bit.
            uint8_t *img = image->image;
            for (int i=0; i<image->width*image->height; i+=2) {
                uint8_t ix0 = *img++;
                uint8_t ix1 = *img++;
                assert(ix0 < 16 && ix1 < 16);
                w8(out, (uint8_t)((ix0 << 4) | ix1));
            }
            break;
        }

        case FMT_IA8: {
            assert(image->ct == LCT_GREY_ALPHA);
            uint8_t *img = image->image;
            for (int i=0; i<image->width*image->height; i++) {
                uint8_t I = *img++; uint8_t A = *img++;
                w8(out, (uint8_t)((I & 0xF0) | (A >> 4)));
            }
            break;
        }

        case FMT_I4: {
            assert(image->ct == LCT_GREY);
            uint8_t *img = image->image;
            for (int i=0; i<image->width*image->height; i+=2) {
                uint8_t I0 = *img++; uint8_t I1 = *img++;
                w8(out, (uint8_t)((I0 & 0xF0) | (I1 >> 4)));
            }
            break;
        }

        case FMT_IA4: {
            assert(image->ct == LCT_GREY_ALPHA);
            // IA4 is 3 bit intensity and 1 bit alpha. Pack it
            uint8_t *img = image->image;
            for (int i=0; i<image->width*image->height; i+=2) {
                uint8_t I0 = *img++; uint8_t A0 = *img++ ? 1 : 0;
                uint8_t I1 = *img++; uint8_t A1 = *img++ ? 1 : 0;
                w8(out, (uint8_t)((I0 & 0xE0) | (A0 << 4) | ((I1 & 0xE0) >> 4) | A1));
            }
            break;
        }

        default:
            // No further conversion needed. Used for: RGBA32, IA16, CI8, I8.
            fwrite(image->image, 1, image->width*image->height*bpp, out);
            break;
        }

        // Padding to force alignment of every image
        walign(out, 8);
        
        // Write extended sprite header after first image
        // See sprite_ext_t (sprite_internal.h)
        if (m == 0) { 
            w16(out, 64);  // sizeof(sprite_ext_t)
            w16(out, 1);   // version
            w_palpos = w32_placeholder(out); // placeholder for position of palette
            for (int i=0; i<7; i++) {
                if (i+1 < spr->num_images) {
                    w16(out, spr->images[i+1].width);
                    w16(out, spr->images[i+1].height);
                    w_lodpos[i] = w32_placeholder(out); // placeholder for position of LOD
                } else {
                    w16(out, 0);
                    w16(out, 0);
                    w32(out, 0);
                }
            }
            walign(out, 8);
        }
    }

    // Finally, write the palette if needed
    if (spr->num_colors > 0) {
        assert(spr->outfmt == FMT_CI8 || spr->outfmt == FMT_CI4);
        w32_at(out, w_palpos, ftell(out));

        // Convert the palette into RGB5551 format. The number of colors can differ
        // from the target, for instanc a PNG with LCT_PALETTE of 64 colors but only
        // actually using the first 16. We handle this without quantization, but still
        // saves the full 64 color palette as it might contain useful colors for effects.
        // FIXME: add the palette size to the sprite_ext_format and sprite API.
        for (int i=0; i<spr->num_colors; i++) {
            uint8_t *pal = spr->colors[i];
            w16(out, conv_rgb5551(pal[0], pal[1], pal[2], pal[3]));
        }
        walign(out, 8);
    }

    fclose(out);
    return true;
}

void spritemaker_write_pngs(spritemaker_t *spr) {
    for (int i=0; i<spr->num_images; i++) {
        char lodext[16]; sprintf(lodext, ".%d.png", i);
        char debugfn[2048];
        strcpy(debugfn, spr->outfn);
        strcpy(strrchr(debugfn, '.'), lodext);

        image_t *img = &spr->images[i];
        if (flag_verbose)
            printf("writing debug file: %s\n", debugfn);

        // Write the PNG file respecting the colortype. Notice that we can't use
        // the simple lodepng_encode_file as it doesn't support a palette, so we need
        // to use the lower level API.
        LodePNGState state;
        lodepng_state_init(&state);
        state.encoder.auto_convert = false; // avoid automatic remapping of palette colors
        state.info_raw = lodepng_color_mode_make(img->ct, 8);
        state.info_png.color = lodepng_color_mode_make(img->ct, 8);
        if (img->ct == LCT_PALETTE) {
            for (int i=0; i<spr->num_colors; i++) {
                lodepng_palette_add(&state.info_raw,       spr->colors[i][0], spr->colors[i][1], spr->colors[i][2], spr->colors[i][3]);
                lodepng_palette_add(&state.info_png.color, spr->colors[i][0], spr->colors[i][1], spr->colors[i][2], spr->colors[i][3]);
            }
        }
        uint8_t *out = NULL; size_t outsize;
        unsigned error = lodepng_encode(&out, &outsize, img->image, img->width, img->height, &state);
        if (!error) error = lodepng_save_file(out, outsize, debugfn);
        lodepng_state_cleanup(&state);
        if (out) lodepng_free(out);
        if (error) {
            fprintf(stderr, "ERROR: writing debug file %s: %s\n", debugfn, lodepng_error_text(error));
        }
    }
}

void spritemaker_free(spritemaker_t *spr) {
    for (int i=0; i<spr->num_images; i++)
        if (spr->images[i].image)
            free(spr->images[i].image);
    memset(spr, 0, sizeof(*spr));
}

int convert(const char *infn, const char *outfn, parms_t *pm) {
    spritemaker_t spr = {0};

    spr.infn = infn;
    spr.outfn = outfn;

    // Load the PNG, passing the desired output format (or FMT_NONE if autodetect).
    if (!spritemaker_load_png(&spr, pm->outfmt))
        goto error;

    // Calculate mipmap levels, if requested
    if (pm->mipmap_algo != MIPMAP_ALGO_NONE)
        spritemaker_calc_lods(&spr, pm->mipmap_algo);

    // Run quantization if needed
    if (spr.outfmt == FMT_CI8 || spr.outfmt == FMT_CI4) {
        int expected_colors = spr.outfmt == FMT_CI8 ? 256 : 16;

        switch (spr.images[0].ct) {
        case LCT_RGBA:
            if (!spritemaker_quantize(&spr, expected_colors, pm->dither_algo))
                goto error;
            break;
        case LCT_PALETTE:
            if (expected_colors < spr.used_colors) {
                if (!spritemaker_expand_rgba(&spr) || 
                    !spritemaker_quantize(&spr, expected_colors, pm->dither_algo))
                    goto error;
            }
            break;
        default:
            assert(0); // should not get here
        }
    }

    // Autodetection of optimal slice size. TODO: this could be improved
    // by calculating actual memory occupation of each slice, to minimize the
    // number of TMEM loads.
    if (pm->tilew) pm->hslices = spr.images[0].width / pm->tilew;
    if (pm->tileh) pm->vslices = spr.images[0].height / pm->tileh;
    if (!pm->hslices) {
        pm->hslices = spr.images[0].width / 16;
        if (flag_verbose)
            printf("auto detected hslices: %d (w=%d/%d)\n", pm->hslices, spr.images[0].width, spr.images[0].width/pm->hslices);
    }
    if (!pm->vslices) {
        pm->vslices = spr.images[0].height / 16;
        if (flag_verbose)
            printf("auto detected vslices: %d (w=%d/%d)\n", pm->vslices, spr.images[0].height, spr.images[0].height/pm->vslices);
    }
    spr.hslices = pm->hslices;
    spr.vslices = pm->vslices;

    // Write the sprite
    if (!spritemaker_write(&spr))
        goto error;

    // Write debug files
    if (flag_debug)
        spritemaker_write_pngs(&spr);

    spritemaker_free(&spr);
    return 0;

error:
    spritemaker_free(&spr);
    return 1;
}


int main(int argc, char *argv[])
{
    char *infn = NULL, *outdir = ".", *outfn = NULL;
    parms_t pm = {0}; bool compression = false;

    if (argc < 2) {
        print_args(argv[0]);
        return 1;
    }

    // We still support (but not document) the old mksprite command line
    // syntax: mksprite <bitdepth> [hslices vslices] input output
    if ((argc == 4 || argc == 6) && (!strcmp(argv[1], "16") || !strcmp(argv[1], "32"))) {
        int i = 1;
        pm.outfmt = !strcmp(argv[i++], "16") ? FMT_RGBA16 : FMT_RGBA32;
        if (argc == 6) {
            pm.hslices = atoi(argv[i++]);
            pm.vslices = atoi(argv[i++]);
        }
        infn = argv[i++];
        outfn = argv[i++];
        printf("WARNING: deprecated command-line syntax was used, please switch to new syntax\n");
        return convert(infn, outfn, &pm);
    }

    bool error = false;
     
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                print_args(argv[0]);
                return 0;
            } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
                flag_verbose = true;
            } else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
                flag_debug = true;
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
                if (!strcmp(argv[i], "RGBA32")) pm.outfmt = FMT_RGBA32;
                else if (!strcmp(argv[i], "RGBA16")) pm.outfmt = FMT_RGBA16;
                else if (!strcmp(argv[i], "IA16")) pm.outfmt = FMT_IA16;
                else if (!strcmp(argv[i], "CI8")) pm.outfmt = FMT_CI8;
                else if (!strcmp(argv[i], "I8")) pm.outfmt = FMT_I8;
                else if (!strcmp(argv[i], "IA8")) pm.outfmt = FMT_IA8;
                else if (!strcmp(argv[i], "CI4")) pm.outfmt = FMT_CI4;
                else if (!strcmp(argv[i], "I4")) pm.outfmt = FMT_I4;
                else if (!strcmp(argv[i], "IA4")) pm.outfmt = FMT_IA4;
                else if (!strcmp(argv[i], "AUTO")) pm.outfmt = FMT_NONE;
                else {
                    fprintf(stderr, "invalid argument for --format: %s\n", argv[i]);
                    print_supported_formats();
                    return 1;
                }
            } else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--tiles")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                char extra;
                if (sscanf(argv[i], "%d,%d%c", &pm.tilew, &pm.tileh, &extra) != 2) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
            } else if (!strcmp(argv[i], "-m") || !strcmp(argv[i], "--mipmap")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                if (!strcmp(argv[i], "NONE")) pm.mipmap_algo = MIPMAP_ALGO_NONE;
                else if (!strcmp(argv[i], "BOX")) pm.mipmap_algo = MIPMAP_ALGO_BOX;
                else {
                    fprintf(stderr, "invalid mipmap algorithm: %s\n", argv[i]);
                    print_supported_mipmap();
                    return 1;
                }
            } else if (!strcmp(argv[i], "-D") || !strcmp(argv[i], "--dither")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                if (!strcmp(argv[i], "NONE")) pm.dither_algo = DITHER_ALGO_NONE;
                else if (!strcmp(argv[i], "RANDOM")) pm.dither_algo = DITHER_ALGO_RANDOM;
                else if (!strcmp(argv[i], "ORDERED")) pm.dither_algo = DITHER_ALGO_ORDERED;
                else {
                    fprintf(stderr, "invalid dithering algorithm: %s\n", argv[i]);
                    print_supported_dithers();
                    return 1;
                }
            } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--compress")) {
                compression = true;
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
            printf("Converting: %s -> %s [fmt=%s tiles=%d,%d mipmap=%s dither=%s]\n",
                infn, outfn, tex_format_name(pm.outfmt), pm.tilew, pm.tileh, mipmap_algo_name(pm.mipmap_algo), dither_algo_name(pm.dither_algo));

        if (convert(infn, outfn, &pm) != 0) {
            error = true;
        } else {
            if (compression) {
                struct stat st_decomp = {0}, st_comp = {0};
                stat(outfn, &st_decomp);
                asset_compress(outfn, outfn, DEFAULT_COMPRESSION);
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
