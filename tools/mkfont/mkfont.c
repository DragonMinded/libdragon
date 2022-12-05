#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "../../src/rdpq/rdpq_font_internal.h"
#include "../../include/surface.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int flag_verbose = 0;
bool flag_debug = false;
int flag_point_size = 12;

void print_args( char * name )
{
    fprintf(stderr, "Usage: %s [flags] <input files...>\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -s/--size <pt>        Point size of the font (default: 12)\n");
    fprintf(stderr, "   -o/--output <dir>     Specify output directory (default: .)\n");
    fprintf(stderr, "   -v/--verbose          Verbose output\n");
    fprintf(stderr, "   -d/--debug            Dump also debug images\n");
    fprintf(stderr, "\n");
}

void codepoint_range_add(int **arr, int *n, int first, int last)
{
    *arr = realloc(*arr, (*n + last - first + 1) * sizeof(int));
    for (int i = first; i <= last; i++)
    {
        (*arr)[(*n)++] = i;
    }
}

#define w32(out, v) ({ \
    _Static_assert(sizeof(v) == 4, "w32: v must be 4 bytes"); \
    fputc((v) >> 24, out); fputc((v) >> 16, out); fputc((v) >> 8, out); fputc((v), out); \
})

#define w16(out, v) ({ \
    _Static_assert(sizeof(v) == 2, "w16: v must be 2 bytes"); \
    fputc(v >> 8, out); fputc(v, out); \
})

#define w8(out, v) ({ \
    _Static_assert(sizeof(v) == 1, "w8: v must be 1 byte"); \
    fputc(v, out); \
})

void falign(FILE *out, int align)
{
    int pos = ftell(out);
    while (pos % align)
    {
        fputc(0, out);
        pos++;
    }
}

void n64font_write(rdpq_font_t *fnt, FILE *out)
{
    // Write header
    w32(out, fnt->magic);
    w32(out, fnt->num_ranges);
    w32(out, fnt->num_glyphs);
    w32(out, fnt->num_atlases);
    int off_placeholders = ftell(out);
    w32(out, (uint32_t)0); // placeholder
    w32(out, (uint32_t)0); // placeholder
    w32(out, (uint32_t)0); // placeholder

    // Write ranges
    uint32_t offset_ranges = ftell(out);
    for (int i=0; i<fnt->num_ranges; i++)
    {
        w32(out, fnt->ranges[i].first_codepoint);
        w32(out, fnt->ranges[i].num_codepoints);
        w32(out, fnt->ranges[i].first_glyph);
    }

    // Write glyphs, aligned to 16 bytes. This makes sure
    // they cover exactly one data cacheline in R4300, so that
    // they each drawn glyph dirties exactly one line.
    falign(out, 16);
    uint32_t offset_glypes = ftell(out);
    for (int i=0; i<fnt->num_glyphs; i++)
    {
        w16(out, fnt->glyphs[i].xadvance);
        w8(out, fnt->glyphs[i].xoff);
        w8(out, fnt->glyphs[i].yoff);
        w8(out, fnt->glyphs[i].xoff2);
        w8(out, fnt->glyphs[i].yoff2);
        w8(out, fnt->glyphs[i].s);
        w8(out, fnt->glyphs[i].t);
        w8(out, fnt->glyphs[i].natlas);
        for (int j=0;j<7;j++) w8(out, (uint8_t)0);
    }

    // Write atlases
    falign(out, 16);
    uint32_t offset_atlases = ftell(out);
    for (int i=0; i<fnt->num_atlases; i++)
    {
        w32(out, (uint32_t)0);
        w16(out, fnt->atlases[i].width);
        w16(out, fnt->atlases[i].height);
        w8(out, fnt->atlases[i].fmt);
        w8(out, fnt->atlases[i].__padding[0]);
        w8(out, fnt->atlases[i].__padding[1]);
        w8(out, fnt->atlases[i].__padding[2]);
    }

    // Write bytes
    uint32_t* offset_atlases_bytes = alloca(sizeof(uint32_t) * fnt->num_atlases);
    for (int i=0; i<fnt->num_atlases; i++)
    {
        falign(out, 8); // align texture data to 8 bytes (for RDP)
        offset_atlases_bytes[i] = ftell(out);
        fwrite(fnt->atlases[i].buf, fnt->atlases[i].width * fnt->atlases[i].height / 2, 1, out);
    }
    uint32_t offset_end = ftell(out);

    // Write offsets
    fseek(out, off_placeholders, SEEK_SET);
    w32(out, offset_ranges);
    w32(out, offset_glypes);
    w32(out, offset_atlases);
    for (int i=0;i<fnt->num_atlases;i++)
    {
        fseek(out, offset_atlases + i * 12, SEEK_SET);
        w32(out, offset_atlases_bytes[i]);
    }

    fseek(out, offset_end, SEEK_SET);
}

void n64font_addrange(rdpq_font_t *fnt, int first, int last)
{
    fnt->ranges = realloc(fnt->ranges, (fnt->num_ranges + 1) * sizeof(range_t));
    fnt->ranges[fnt->num_ranges].first_codepoint = first;
    fnt->ranges[fnt->num_ranges].num_codepoints = last - first + 1;
    fnt->ranges[fnt->num_ranges].first_glyph = fnt->num_glyphs;
    fnt->num_ranges++;
    fnt->glyphs = realloc(fnt->glyphs, (fnt->num_glyphs + last - first + 1) * sizeof(glyph_t));
    memset(fnt->glyphs + fnt->num_glyphs, 0, (last - first + 1) * sizeof(glyph_t));
    fnt->num_glyphs += last - first + 1;
}

glyph_t* n64font_glyph(rdpq_font_t *fnt, uint32_t cp)
{
    for (int i=0;i<fnt->num_ranges;i++)
    {
        if (cp >= fnt->ranges[i].first_codepoint && cp < fnt->ranges[i].first_codepoint + fnt->ranges[i].num_codepoints)
            return &fnt->glyphs[fnt->ranges[i].first_glyph + cp - fnt->ranges[i].first_codepoint];
    }
    assert(!"invalid codepoint"); // should never happen
}

void n64font_addatlas(rdpq_font_t *fnt, uint8_t *buf, int width, int height, int stride)
{
    int rwidth = (width + 15) / 16 * 16; // round up to 8 bytes (16 pixels)

    fnt->atlases = realloc(fnt->atlases, (fnt->num_atlases + 1) * sizeof(atlas_t));
    fnt->atlases[fnt->num_atlases].width = rwidth;
    fnt->atlases[fnt->num_atlases].height = height;
    fnt->atlases[fnt->num_atlases].fmt = FMT_I4;
    fnt->atlases[fnt->num_atlases].buf = calloc(1, rwidth * height / 2);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 2) {
            uint8_t px0 = buf[y * stride + x + 0] >> 4;
            uint8_t px1 = buf[y * stride + x + 1] >> 4;
            fnt->atlases[fnt->num_atlases].buf[y * rwidth / 2 + x / 2] = (px0 << 4) | px1;
        }
    }
    fnt->num_atlases++;
}

rdpq_font_t* n64font_alloc(void)
{
    rdpq_font_t *fnt = calloc(1, sizeof(rdpq_font_t));
    fnt->magic = FONT_MAGIC_V0;
    return fnt;
}

void n64font_free(rdpq_font_t *fnt)
{
    for (int i=0;i<fnt->num_atlases;i++)
        free(fnt->atlases[i].buf);
    free(fnt->atlases);
    free(fnt->glyphs);
    free(fnt->ranges);
    free(fnt);
}

void image_compact(uint8_t *pixels, int *w, int *h, int stride)
{
    // Compact trailing rows
    for (int y = *h -  1; y >= 0; y--) {
        int x;
        for (x = 0; x < *w; x++) {
            if (pixels[y * stride + x] != 0) {
                *h = y + 1;
                y = 0;
                break;
            }
        }
    }

    // Compact trailing columns
    for (int x = *w - 1; x >= 0; x--) {
        int y;
        for (y = 0; y < *h; y++) {
            if (pixels[y * stride + x] != 0) {
                *w = x + 1;
                x = 0;
                break;
            }
        }
    }
}

int convert(const char *infn, const char *outfn, int point_size)
{
    unsigned char *indata = NULL;
    {
        FILE *infile = fopen(infn, "rb");
        if (!infile) {
            fprintf(stderr, "Error: could not open input file: %s\n", infn);
            return false;
        }
        fseek(infile, 0, SEEK_END);
        int insize = ftell(infile);
        fseek(infile, 0, SEEK_SET);
        indata = (unsigned char *)malloc(insize);
        fread(indata, 1, insize, infile);
        fclose(infile);
    }

    int w = 128, h = 64;  // maximum size for a I4 texture
    unsigned char *pixels = malloc(w * h);

    int ranges[] = { 0x20, 0x7F, 0xA0, 0xFF, 0x100, 0x17F, 0x400, 0x4FF, 0x3040, 0x309F, 0,0 };

    rdpq_font_t *font = n64font_alloc();

    // Go through all the ranges
    int nimg = 0;
    for (int r=0; ranges[r]; r+=2) {
        if (flag_verbose)
            fprintf(stderr, "processing codepoint range: %04X - %04X\n", ranges[r], ranges[r+1]);
        n64font_addrange(font, ranges[r], ranges[r+1]);

        // Create an array with all the codepoints for this range
        int *cprange = NULL;
        for (int p=ranges[r]; p <= ranges[r+1]; p++)
            arrpush(cprange, p);

        // Go through all the codepoints in this range, until we process them all
        while (arrlen(cprange) > 0) {
            memset(pixels, 0, w*h);

            stbtt_pack_range range = {
                .font_size = STBTT_POINT_SIZE(point_size),
                .array_of_unicode_codepoints = cprange,
                .num_chars = arrlen(cprange),
                .h_oversample = 1, .v_oversample = 1,
            };
            range.chardata_for_range = malloc(sizeof(stbtt_packedchar) * range.num_chars);

            // Call stbtt to extract the glyphs into the bitmap.
            // Not all of them will fit, so we need to figure out which ones did.
            stbtt_pack_context spc;
            stbtt_PackBegin(&spc, pixels, w, h, 0, 1, NULL);
            stbtt_PackSetSkipMissingCodepoints(&spc, 0);
            stbtt_PackFontRanges(&spc, indata, 0, &range, 1);
            stbtt_PackEnd(&spc);

            bool at_least_one = false;
            int *newrange = NULL;
            for (int i=0;i<range.num_chars;i++) {
                // Check if this glyph was actually packed
                stbtt_packedchar *ch = &range.chardata_for_range[i];
                if (ch->x1 != 0) {
                    if (flag_verbose >= 2) {
                        fprintf(stderr, " codepoint: %d [%d,%d-%d,%d] %.3f,%.3f,%.3f,%.3f,%.3f\n", range.array_of_unicode_codepoints[i],
                            ch->x0, ch->y0, ch->x1, ch->y1,
                            ch->xoff, ch->yoff, ch->xoff2, ch->yoff2, ch->xadvance);
                    }
                    if(fabsf(ch->xoff) > 128 || fabsf(ch->yoff) > 128 || fabsf(ch->xoff2) > 128 || fabsf(ch->yoff2) > 128 ||
                       fabsf(ch->xadvance) > 32768/64)
                    {
                        fprintf(stderr, "ERROR: font too big, please reduce point size (%d)\n", point_size);
                        return 1;
                    }
                    at_least_one = true;
                    glyph_t *g = n64font_glyph(font, range.array_of_unicode_codepoints[i]);
                    g->natlas = nimg;
                    g->s = ch->x0; g->t = ch->y0;
                    g->xoff = ch->xoff; g->yoff = ch->yoff;
                    g->xoff2 = ch->xoff2; g->yoff2 = ch->yoff2;
                    g->xadvance = ch->xadvance * 64;
                } else {
                    // If the glyph wasn't packed, add it to an array of codepoints to process in the next image
                    arrpush(newrange, range.array_of_unicode_codepoints[i]);
                }
            }

            if (at_least_one) {
                if (flag_debug) {
                    char *outfn2 = NULL;
                    asprintf(&outfn2, "%s_%d.png", outfn, nimg);
                    stbi_write_png(outfn2, w, h, 1, pixels, w);
                    free(outfn2);
                }

                int rw = w, rh = h;
                image_compact(pixels, &rw, &rh, w);
                n64font_addatlas(font, pixels, rw, rh, w);
                if (flag_verbose)
                    fprintf(stderr, "created atlas %d: %d x %d pixels (%ld glyphs left)\n", nimg, rw, rh, arrlen(newrange));
                nimg++;
            } else {
                // No glyph were added even if the image is empty. It means
                // that all of these are not available in the current font, so let's
                // just skip them.
                arrfree(newrange);
            }

            arrfree(cprange);
            cprange = newrange;
        }
    }

    free(pixels);

    FILE *out = fopen(outfn, "wb");
    if (!out) {
        fprintf(stderr, "cannot open output file: %s\n", outfn);
        return 1;
    }
    n64font_write(font, out);
    fclose(out);

    n64font_free(font);
    return 0;
}

int main(int argc, char *argv[])
{
    char *infn = NULL, *outdir = ".", *outfn = NULL;
    bool error = false;

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
            } else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--size")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                char extra;
                if (sscanf(argv[i], "%d%c", &flag_point_size, &extra) != 1) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
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

        asprintf(&outfn, "%s/%s.font64", outdir, basename_noext);
        if (flag_verbose)
            printf("Converting: %s -> %s\n",
                infn, outfn);
        if (convert(infn, outfn, flag_point_size) != 0)
            error = true;
        free(outfn);
    }

    return error ? 1 : 0;
}
