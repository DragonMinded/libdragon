#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "../../src/rdpq/rdpq_font_internal.h"
#include "../../include/surface.h"

#define STB_DS_IMPLEMENTATION
#include "../common/stb_ds.h"
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int flag_verbose = 0;
bool flag_debug = false;
bool flag_kerning = true;
int flag_point_size = 12;
int *flag_ranges = NULL;

void print_args( char * name )
{
    fprintf(stderr, "mkfont -- Convert TTF/OTF fonts into the font64 format for libdragon\n\n");
    fprintf(stderr, "Usage: %s [flags] <input files...>\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -s/--size <pt>            Point size of the font (default: 12)\n");
    fprintf(stderr, "   -r/--range <start-stop>   Range of unicode codepoints to convert, as hex values (default: 20-7F)\n");
    fprintf(stderr, "   -o/--output <dir>         Specify output directory (default: .)\n");
    fprintf(stderr, "   -v/--verbose              Verbose output\n");
    fprintf(stderr, "   --no-kerning              Do not export kerning information\n");
    fprintf(stderr, "   -d/--debug                Dump also debug images\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "It is possible to convert multiple ranges of codepoints, by specifying\n");
    fprintf(stderr, "--range more than one time.\n");
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
    w32(out, fnt->point_size);
    w32(out, fnt->num_ranges);
    w32(out, fnt->num_glyphs);
    w32(out, fnt->num_atlases);
    w32(out, fnt->num_kerning);
    int off_placeholders = ftell(out);
    w32(out, (uint32_t)0); // placeholder
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
        for (int j=0;j<3;j++) w8(out, (uint8_t)0);
        w16(out, fnt->glyphs[i].kerning_lo);
        w16(out, fnt->glyphs[i].kerning_hi);
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

    // Write kernings
    falign(out, 16);
    uint32_t offset_kernings = ftell(out);
    for (int i=0; i<fnt->num_kerning; i++)
    {
        w16(out, fnt->kerning[i].glyph2);
        w8(out, fnt->kerning[i].kerning);
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
    w32(out, offset_kernings);
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

int n64font_glyph(rdpq_font_t *fnt, uint32_t cp)
{
    for (int i=0;i<fnt->num_ranges;i++)
    {
        if (cp >= fnt->ranges[i].first_codepoint && cp < fnt->ranges[i].first_codepoint + fnt->ranges[i].num_codepoints)
            return fnt->ranges[i].first_glyph + cp - fnt->ranges[i].first_codepoint;
    }
    return -1;
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

void n64font_addkerning(rdpq_font_t *fnt, int g1, int g2, int kerning)
{
    fnt->kerning = realloc(fnt->kerning, (fnt->num_kerning + 1) * sizeof(kerning_t));
    fnt->kerning[fnt->num_kerning].glyph2 = g2;
    assert(kerning >= -128 && kerning <= 127);
    fnt->kerning[fnt->num_kerning].kerning = kerning;
    fnt->num_kerning++;
}

rdpq_font_t* n64font_alloc(int point_size)
{
    rdpq_font_t *fnt = calloc(1, sizeof(rdpq_font_t));
    fnt->magic = FONT_MAGIC_V0;
    fnt->point_size = point_size;
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

// qsort compare function to sort arrays of kerning_t by glyph2
int kerning_cmp(const void *a, const void *b)
{
    const kerning_t *ka = a;
    const kerning_t *kb = b;
    if (ka->glyph2 < kb->glyph2)
        return -1;
    if (ka->glyph2 > kb->glyph2)
        return 1;
    return 0;
}

int convert(const char *infn, const char *outfn, int point_size, int *ranges)
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

    // Initialize the font
    stbtt_fontinfo info;
    stbtt_InitFont(&info, indata, 0);
    float font_scale = stbtt_ScaleForMappingEmToPixels(&info, point_size);

    int w = 128, h = 64;  // maximum size for a I4 texture
    unsigned char *pixels = malloc(w * h);

    rdpq_font_t *font = n64font_alloc(point_size);

    // Map from N64 glyph index to TTF glyph index
    typedef struct { int key; int value; } glyphmap_t;
    glyphmap_t *glyph_indices = NULL;
    hmdefault(glyph_indices, -1);

    // Go through all the ranges
    int nimg = 0;
    for (int r=0; r<arrlen(ranges); r+=2) {
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
                    int gidx = n64font_glyph(font, range.array_of_unicode_codepoints[i]);
                    assert(gidx >= 0);
                    glyph_t *g = &font->glyphs[gidx];
                    g->natlas = nimg;
                    g->s = ch->x0; g->t = ch->y0;
                    g->xoff = ch->xoff; g->yoff = ch->yoff;
                    g->xoff2 = ch->xoff2; g->yoff2 = ch->yoff2;
                    g->xadvance = ch->xadvance * 64;

                    // Update the glyph index map
                    int ttf_gidx = stbtt_FindGlyphIndex(&info, range.array_of_unicode_codepoints[i]);
                    assert(ttf_gidx >= 0);
                    hmput(glyph_indices, gidx, ttf_gidx);
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

    // Add kerning information, if enabled on command line and available in the font
    if (flag_kerning && (info.kern || info.gpos)) {
        const int ascii_range_start = 0x20;
        const int ascii_range_len = 0x80 - 0x20;

        // Add first empty entry for kerning. This allows to store "0" in glyphs to mean "no kerning"
        n64font_addkerning(font, 0, 0, 0);

        // Prepare the kerning table. Go through all ranges, and within each range, construct a N*N table
        // for all the pairs [glyph1, glyph2] for all glyphs in that range. This means that we don't
        // collect kerning for pairs of glyphs in different ranges, but that shouldn't really matter in real
        // use cases (eg: kerning between a cyrillic and a greek letter is probably not very useful).
        // In addition to this, always collect kerning against all ASCII characters, because those are common
        // enough to be useful with all the ranges.
        for (int r=0;r<font->num_ranges; r++) {
            range_t *range = &font->ranges[r];

            // Number of codepoints to iterate twice (N^2). These are the glyphs in the range
            // plus the ASCII range (unless the range *is* ASCII itself).
            int num_codepoints = range->num_codepoints;
            if (range->first_codepoint != ascii_range_start)
                num_codepoints += ascii_range_len;

            for (int i=0;i<num_codepoints;i++) {
                // First glyph index
                int gidx1 = (i >= range->num_codepoints) ? ascii_range_start+i-range->num_codepoints : range->first_glyph + i;
                int ttf_idx1 = hmget(glyph_indices, gidx1);
                if (ttf_idx1 < 0) continue;
                glyph_t *g = &font->glyphs[gidx1];

                int kerning_start = font->num_kerning;

                for (int j=0; j<num_codepoints; j++) {
                    // Second glyph index
                    int gidx2 = (j >= range->num_codepoints) ? ascii_range_start+j-range->num_codepoints : range->first_glyph + j;
                    int ttf_idx2 = hmget(glyph_indices, gidx2);
                    if (ttf_idx2 < 0) continue;

                    // Extract kerning between the two glyphs from the TTF file
                    int kerning = stbtt_GetGlyphKernAdvance(&info, ttf_idx1, ttf_idx2);
                    if (kerning != 0) {
                        // Calculate the kerning in pixels
                        float advance = kerning * font_scale;

                        // Skip very small kerning values. These are possibly useless with our
                        // small resolutions and font sizes, so we save a bit of runtime space
                        // in RAM and a bit of CPU (smaller tables => faster lookups).
                        if (fabsf(advance) < 0.5f)
                            continue;

                        // Add the kerning entry. Scale the advance to fit 8 bit, assuming
                        // the kerning will never be bigger than the point size (and usually much
                        // smaller). This makes good use of the available precision.
                        n64font_addkerning(font, gidx1, gidx2, advance * 127.0f / point_size);
                    }
                }

                if (font->num_kerning != kerning_start) {
                    // If at least one kerning entry was added for this glyph, sort the kerning table
                    // by second glyph index (to speeed up runtime lookups) and then store the range
                    // within the first glyph.
                    g->kerning_lo = kerning_start;
                    g->kerning_hi = font->num_kerning - 1;

                    qsort(font->kerning + g->kerning_lo, g->kerning_hi - g->kerning_lo + 1, sizeof(kerning_t), kerning_cmp);
                }
            }
        }

        if (flag_verbose)
            fprintf(stderr, "built kerning table (%d entries)\n", font->num_kerning);
    }

    // Write output file
    FILE *out = fopen(outfn, "wb");
    if (!out) {
        fprintf(stderr, "cannot open output file: %s\n", outfn);
        return 1;
    }
    n64font_write(font, out);
    fclose(out);

    n64font_free(font);
    free(indata);
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
            } else if (!strcmp(argv[i], "--no-kerning")) {
                flag_kerning = false;
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
                arrpush(flag_ranges, r0);
                arrpush(flag_ranges, r1);
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

        if (!flag_ranges) {
            // Default range (ASCII)
            arrpush(flag_ranges, 0x20);
            arrpush(flag_ranges, 0x7F);
        }

        asprintf(&outfn, "%s/%s.font64", outdir, basename_noext);
        if (flag_verbose)
            printf("Converting: %s -> %s\n",
                infn, outfn);
        if (convert(infn, outfn, flag_point_size, flag_ranges) != 0)
            error = true;
        free(outfn);
    }

    return error ? 1 : 0;
}
