#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>

#include "../../include/surface.h"
#include "../../src/rdpq/rdpq_font_internal.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS    // No need to parse PNG extra fields
#define LODEPNG_NO_COMPILE_CPP                 // No need to use C++ API
#include "../common/lodepng.h"
#include "../common/lodepng.c"

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
int flag_point_size = 12;
int *flag_ranges = NULL;
const char *n64_inst = NULL;
int flag_ellipsis_cp = 0x002E;
int flag_ellipsis_repeats = 3;

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
    // fprintf(stderr, "BMFont specific flags:\n");
    // fprintf(stderr, "\n");
    fprintf(stderr, "TTF/OTF specific flags:\n");
    fprintf(stderr, "   -s/--size <pt>            Point size of the font (default: 12)\n");
    fprintf(stderr, "   -r/--range <start-stop>   Range of unicode codepoints to convert, as hex values (default: 20-7F)\n");
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

void n64font_write(rdpq_font_t *fnt, FILE *out)
{
    // Write header
    w8(out, fnt->magic[0]);
    w8(out, fnt->magic[1]);
    w8(out, fnt->magic[2]);
    w8(out, fnt->version);
    w32(out, fnt->point_size);
    w32(out, fnt->ascent);
    w32(out, fnt->descent);
    w32(out, fnt->line_gap);
    w32(out, fnt->space_width);
    w16(out, fnt->ellipsis_width);
    w16(out, fnt->ellipsis_glyph);
    w16(out, fnt->ellipsis_reps);
    w16(out, fnt->ellipsis_advance);
    w32(out, fnt->num_ranges);
    w32(out, fnt->num_glyphs);
    w32(out, fnt->num_atlases);
    w32(out, fnt->num_kerning);
    w32(out, 1);   // num styles (not supported by mkfont yet)
    int off_placeholders = ftell(out);
    w32(out, (uint32_t)0); // placeholder
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
    walign(out, 16);
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
    walign(out, 16);
    uint32_t offset_atlases = ftell(out);
    int* offset_atlases_sprites = alloca(sizeof(int) * fnt->num_atlases);
    for (int i=0; i<fnt->num_atlases; i++)
    {
        offset_atlases_sprites[i] = w32_placeholder(out);
        w32(out, fnt->atlases[i].size);
        w32(out, 0);
    }

    // Write kernings
    walign(out, 16);
    uint32_t offset_kernings = ftell(out);
    for (int i=0; i<fnt->num_kerning; i++)
    {
        w16(out, fnt->kerning[i].glyph2);
        w8(out, fnt->kerning[i].kerning);
    }

    // Write bytes
    for (int i=0; i<fnt->num_atlases; i++)
    {
        walign(out, 16); // align sprites to 16 bytes
        w32_at(out, offset_atlases_sprites[i], ftell(out));
        fwrite(fnt->atlases[i].sprite, fnt->atlases[i].size, 1, out);
    }
    uint32_t offset_end = ftell(out);

    // Write styles
    walign(out, 16);
    uint32_t offset_styles = ftell(out);
    w32(out, 0xFFFFFFFF); // color
    w32(out, 0); // runtime pointer
    for (int i=0; i<255; i++)
    {
        w32(out, 0); // color
        w32(out, 0); // runtime pointer
    }

    // Write offsets
    fseek(out, off_placeholders, SEEK_SET);
    w32(out, offset_ranges);
    w32(out, offset_glypes);
    w32(out, offset_atlases);
    w32(out, offset_kernings);
    w32(out, offset_styles);

    fseek(out, offset_end, SEEK_SET);
}

void n64font_addrange(rdpq_font_t *fnt, int first, int last)
{
    // Check that the range does not overlap an existing one
    for (int i=0;i<fnt->num_ranges;i++)
    {
        if (first >= fnt->ranges[i].first_codepoint && first < fnt->ranges[i].first_codepoint + fnt->ranges[i].num_codepoints)
        {
            fprintf(stderr, "Error: range 0x%04x-0x%04x overlaps with existing range 0x%04x-0x%04x\n", first, last, fnt->ranges[i].first_codepoint, fnt->ranges[i].first_codepoint + fnt->ranges[i].num_codepoints - 1);
            exit(1);
        }
        if (last >= fnt->ranges[i].first_codepoint && last < fnt->ranges[i].first_codepoint + fnt->ranges[i].num_codepoints)
        {
            fprintf(stderr, "Error: range 0x%04x-0x%04x overlaps with existing range 0x%04x-0x%04x\n", first, last, fnt->ranges[i].first_codepoint, fnt->ranges[i].first_codepoint + fnt->ranges[i].num_codepoints - 1);
            exit(1);
        }
    }
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

static void png_write_func(void *context, void *data, int size)
{
    FILE *f = context;
    fwrite(data, 1, size, f);
}

void n64font_addatlas(rdpq_font_t *fnt, uint8_t *buf, int width, int height, int stride)
{
    static char *mksprite = NULL;
    if (!mksprite) asprintf(&mksprite, "%s/bin/mksprite", n64_inst);

    // Prepare mksprite command line
    struct subprocess_s subp;
    const char *cmd_addr[16] = {0}; int i = 0;
    cmd_addr[i++] = mksprite;
    cmd_addr[i++] = "--format";
    cmd_addr[i++] = "I4";
    cmd_addr[i++] = "--compress";  // don't compress the individual sprite (the font itself will be compressed)
    cmd_addr[i++] = "0";
    
    // Start mksprite
    if (subprocess_create(cmd_addr, subprocess_option_no_window|subprocess_option_inherit_environment, &subp) != 0) {
        fprintf(stderr, "Error: cannot run: %s\n", mksprite);
        exit(1);
    }

    // Write PNG to standard input of mksprite
    FILE *mksprite_in = subprocess_stdin(&subp);
    stbi_write_png_to_func(png_write_func, mksprite_in, width, height, 1, buf, stride);
    fclose(mksprite_in); subp.stdin_file = SUBPROCESS_NULL;

    // Read sprite from stdout into memory
    FILE *mksprite_out = subprocess_stdout(&subp);
    uint8_t *sprite = NULL;
    int sprite_size = 0;
    while (1) {
        uint8_t buf[4096];
        int n = fread(buf, 1, sizeof(buf), mksprite_out);
        if (n == 0) break;
        sprite = realloc(sprite, sprite_size + n);
        memcpy(sprite + sprite_size, buf, n);
        sprite_size += n;
    }

    // Dump mksprite's stderr. Whatever is printed there (if anything) is useful to see
    FILE *mksprite_err = subprocess_stderr(&subp);
    while (1) {
        char buf[4096];
        int n = fread(buf, 1, sizeof(buf), mksprite_err);
        if (n == 0) break;
        fwrite(buf, 1, n, stderr);
    }

    // mksprite should be finished. Extract the return code and abort if failed
    int retcode;
    subprocess_join(&subp, &retcode);
    if (retcode != 0) {
        fprintf(stderr, "Error: mksprite failed with return code %d\n", retcode);
        exit(1);
    }
    subprocess_destroy(&subp);

    fnt->atlases = realloc(fnt->atlases, (fnt->num_atlases + 1) * sizeof(atlas_t));
    fnt->atlases[fnt->num_atlases].sprite = (void*)sprite;
    fnt->atlases[fnt->num_atlases].size = sprite_size;
    fnt->atlases[fnt->num_atlases].up = NULL;
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

void n64font_add_ellipsis(rdpq_font_t *fnt, int ellipsis_cp, int ellipsis_repeats)
{
    int ellipsis_glyph = n64font_glyph(fnt, ellipsis_cp);
    if (ellipsis_glyph < 0) {
        fprintf(stderr, "Error: ellipsis codepoint 0x%04x not found in font\n", ellipsis_cp);
        exit(1);
    }

    // Calculate length of ellipsis string
    glyph_t *g = &fnt->glyphs[ellipsis_glyph];
    float ellipsis_width = g->xadvance * (1.0f / 64.0f);
    
    if (g->kerning_lo) {
        for (int i = g->kerning_lo; i <= g->kerning_hi; i++) {
            if (fnt->kerning[i].glyph2 == ellipsis_glyph) {
                ellipsis_width += fnt->kerning[i].kerning * fnt->point_size / 127.0f;
                break;
            }
        }
    }

    fnt->ellipsis_advance = ellipsis_width + 0.5f;
    ellipsis_width *= 2;
    ellipsis_width += g->xoff2;
    
    fnt->ellipsis_width = ellipsis_width + 0.5f;
    fnt->ellipsis_reps = ellipsis_repeats;
    fnt->ellipsis_glyph = ellipsis_glyph;
}

rdpq_font_t* n64font_alloc(int point_size, int ascent, int descent, int line_gap, int space_width)
{
    rdpq_font_t *fnt = calloc(1, sizeof(rdpq_font_t));
    memcpy(fnt->magic, FONT_MAGIC, 3);
    fnt->version = 4;
    fnt->point_size = point_size;
    fnt->ascent = ascent;
    fnt->descent = descent;
    fnt->line_gap = line_gap;
    fnt->space_width = space_width;
    return fnt;
}

void n64font_free(rdpq_font_t *fnt)
{
    for (int i=0;i<fnt->num_atlases;i++)
        free(fnt->atlases[i].sprite);
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

    // Add one black column and row to avoid bilinear filtering artifacts
    *w = *w+1;
    *h = *h+1;
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

int convert_ttf(const char *infn, const char *outfn, int point_size, int *ranges)
{
    unsigned char *indata = NULL;
    {
        FILE *infile = fopen(infn, "rb");
        if (!infile) {
            fprintf(stderr, "Error: could not open input file: %s\n", infn);
            return 1;
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
    int ascent, descent, line_gap; int space_width;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
    stbtt_GetCodepointHMetrics(&info, ' ', &space_width, NULL);

    int w = 128, h = 64;  // maximum size for a I4 texture
    unsigned char *pixels = malloc(w * h);

    rdpq_font_t *font = n64font_alloc(point_size, ascent * font_scale + .5, descent * font_scale + .5, line_gap * font_scale + .5, space_width * font_scale + .5);

    // Map from N64 glyph index to TTF glyph index
    typedef struct { int key; int value; } glyphmap_t;
    glyphmap_t *glyph_indices = NULL;
    stbds_hmdefault(glyph_indices, -1);

    // Go through all the ranges
    int nimg = 0;
    for (int r=0; r<stbds_arrlen(ranges); r+=2) {
        if (flag_verbose)
            fprintf(stderr, "processing codepoint range: %04X - %04X\n", ranges[r], ranges[r+1]);
        n64font_addrange(font, ranges[r], ranges[r+1]);

        // Create an array with all the codepoints for this range
        int *cprange = NULL;
        for (int p=ranges[r]; p <= ranges[r+1]; p++)
            stbds_arrpush(cprange, p);

        // Go through all the codepoints in this range, until we process them all
        while (stbds_arrlen(cprange) > 0) {
            memset(pixels, 0, w*h);

            stbtt_pack_range range = {
                .font_size = STBTT_POINT_SIZE(point_size),
                .array_of_unicode_codepoints = cprange,
                .num_chars = stbds_arrlen(cprange),
                .h_oversample = 1, .v_oversample = 1,
            };
            range.chardata_for_range = malloc(sizeof(stbtt_packedchar) * range.num_chars);

            // Call stbtt to extract the glyphs into the bitmap.
            // Not all of them will fit, so we need to figure out which ones did.
            stbtt_pack_context spc;
            stbtt_PackBegin(&spc, pixels, w, h, 0, 1, NULL);
            stbtt_PackSetSkipMissingCodepoints(&spc, 1);
            stbtt_PackFontRanges(&spc, indata, 0, &range, 1);
            stbtt_PackEnd(&spc);

            bool at_least_one = false;
            int *newrange = NULL;
            for (int i=0;i<range.num_chars;i++) {
                // if (range.array_of_unicode_codepoints[i] == ' ')
                //     continue;   // skip space (we already recorded its size in the header)
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
                    stbds_hmput(glyph_indices, gidx, ttf_gidx);
                } else {
                    // If the glyph wasn't packed, add it to an array of codepoints to process in the next image
                    stbds_arrpush(newrange, range.array_of_unicode_codepoints[i]);
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
                    fprintf(stderr, "created atlas %d: %d x %d pixels (%d glyphs left)\n", nimg, rw, rh, (int)stbds_arrlen(newrange));
                nimg++;
            } else {
                // No glyph were added even if the image is empty. It means
                // that all of these are not available in the current font, so let's
                // just skip them.
                stbds_arrfree(newrange);
            }

            stbds_arrfree(cprange);
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
                int ttf_idx1 = stbds_hmget(glyph_indices, gidx1);
                if (ttf_idx1 < 0) continue;
                glyph_t *g = &font->glyphs[gidx1];

                int kerning_start = font->num_kerning;

                for (int j=0; j<num_codepoints; j++) {
                    // Second glyph index
                    int gidx2 = (j >= range->num_codepoints) ? ascii_range_start+j-range->num_codepoints : range->first_glyph + j;
                    int ttf_idx2 = stbds_hmget(glyph_indices, gidx2);
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

    if (flag_ellipsis_repeats > 0)
        n64font_add_ellipsis(font, flag_ellipsis_cp, flag_ellipsis_repeats);

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

#include "mkfont_bmfont.c"

int main(int argc, char *argv[])
{
    char *infn = NULL, *outdir = ".", *outfn = NULL;
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
                stbds_arrpush(flag_ranges, r0);
                stbds_arrpush(flag_ranges, r1);
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

        if (!flag_ranges) {
            // Default range (ASCII)
            stbds_arrpush(flag_ranges, 0x20);
            stbds_arrpush(flag_ranges, 0x7F);
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
            ret = convert_ttf(infn, outfn, flag_point_size, flag_ranges);
        } else if (strcasestr(infn, ".fnt")) {
            fprintf(stderr, "Error: BMFont support is incomplete.\n"); exit(1);
            compression = 0;
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
            }
        }
        free(outfn);
    }

    return error ? 1 : 0;
}
