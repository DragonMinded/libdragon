/*
    mksprite: convert a PNG image into a sprite file
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
#ifndef MKSPRITE_H
#define MKSPRITE_H

#include <stdbool.h>
#include <stdint.h>

#include "../common/lodepng.h"
#include "../../include/surface.h"

typedef struct {
    uint8_t *image;         // Pointer to image data (pixels)
    int width, height;      // Image dimensions
    tex_format_t fmt;       // Texture format
    LodePNGColorType ct;    // PNG color type
} image_t;

typedef struct {
    int num_colors;         // Number of colors in palette
    int used_colors;        // Number of colors actually used in palette
    uint8_t colors[256][4]; // Color palette (if num_colors != 0)
} palette_t;

typedef struct {
    struct {
        float translate;
        int scale;
        float repeats;
        int mirror;
    } s, t;
    bool defined;
} texparms_t;

typedef struct parms_s {
    tex_format_t outfmt;
    int hslices;
    int vslices;
    int tilew;
    int tileh;
    int mipmap_algo;
    int dither_algo;
    int gamma_correct;
    bool ignore_tmem;
    int lossy_quality;
    texparms_t texparms;
    struct{
        const char   *infn;       // Input file for detail texture
        texparms_t   texparms;
        tex_format_t outfmt;
        float        blend_factor;
        bool         use_main_tex;
        bool         enabled;
    } detail;
} parms_t;

#ifdef __cplusplus
extern "C" {
#endif

bool load_png_image(const char *infn, tex_format_t fmt, image_t *imgout, palette_t *palout);
int mksprite_convert_lossy(
    const char *infn, const char *outfn, const parms_t *pm,
    int compress
);

extern bool flag_verbose;
extern bool flag_debug;

#ifdef __cplusplus
}
#endif

#endif

