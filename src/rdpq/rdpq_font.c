#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "rdpq.h"
#include "rdpq_rect.h"
#include "surface.h"
#include "rdpq_mode.h"
#include "rdpq_tex.h"
#include "rdpq_font.h"
#include "rdpq_font_internal.h"

_Static_assert(sizeof(glyph_t) == 16, "glyph_t size is wrong");
_Static_assert(sizeof(atlas_t) == 12, "atlas_t size is wrong");
_Static_assert(sizeof(kerning_t) == 3, "kerning_t size is wrong");

#define PTR_DECODE(font, ptr)    ((void*)(((uint8_t*)(font)) + (uint32_t)(ptr)))

/** @brief Drawing context */
static struct draw_ctx_s {
    atlas_t *last_atlas;
    rdpq_tile_t atlas_tile;
    float x;
    float y;
    float xscale, yscale;
} draw_ctx;

void *__file_load_all(const char *fn, int *sz);

static rdpq_tile_t atlas_activate(atlas_t *atlas)
{
    if (draw_ctx.last_atlas != atlas) {
        draw_ctx.atlas_tile = (draw_ctx.atlas_tile + 1) & 7;
        surface_t s = surface_make_linear(atlas->buf, atlas->fmt, atlas->width, atlas->height);
        rdpq_tex_load(draw_ctx.atlas_tile, &s, 0);
        draw_ctx.last_atlas = atlas;
    }
    return draw_ctx.atlas_tile;
}

rdpq_font_t* rdpq_font_load(const char *fn)
{
    int sz;
    rdpq_font_t *fnt = __file_load_all(fn, &sz);
    assertf(fnt->magic == FONT_MAGIC_V0, "invalid font file (magic: %08lx)", fnt->magic);

    fnt->ranges = PTR_DECODE(fnt, fnt->ranges);
    fnt->glyphs = PTR_DECODE(fnt, fnt->glyphs);
    fnt->atlases = PTR_DECODE(fnt, fnt->atlases);
    fnt->kerning = PTR_DECODE(fnt, fnt->kerning);
    for (int i = 0; i < fnt->num_atlases; i++) {
        fnt->atlases[i].buf = PTR_DECODE(fnt, fnt->atlases[i].buf);
    }

    return fnt;
}

void rdpq_font_free(rdpq_font_t *fnt)
{
    #ifndef NDEBUG
    // To help debugging, zero the font structure
    memset(fnt, 0, sizeof(rdpq_font_t));
    #endif

    free(fnt);
}


static uint32_t utf8_decode(const char **str)
{
    const uint8_t *s = (const uint8_t*)*str;
    uint32_t c = *s++;
    if (c < 0x80) {
        *str = (const char*)s;
        return c;
    }
    if (c < 0xC0) {
        *str = (const char*)s;
        return 0xFFFD;
    }
    if (c < 0xE0) {
        c = ((c & 0x1F) << 6) | (*s++ & 0x3F);
        *str = (const char*)s;
        return c;
    }
    if (c < 0xF0) {
        c = ((c & 0x0F) << 12); c |= ((*s++ & 0x3F) << 6); c |= (*s++ & 0x3F);
        *str = (const char*)s;
        return c;
    }
    if (c < 0xF8) {
        c = ((c & 0x07) << 18); c |= ((*s++ & 0x3F) << 12); c |= ((*s++ & 0x3F) << 6); c |= (*s++ & 0x3F);
        *str = (const char*)s;
        return c;
    }
    *str = (const char*)s;
    return 0xFFFD;
}

void rdpq_font_printn(rdpq_font_t *fnt, const char *text, int nch)
{
    int16_t *glyphs = alloca(nch * sizeof(int16_t));
    int n = 0;
    const char* text_end = text + nch;

    // Decode UTF-8 text into glyph indices. We do this in one pass
    // and store the glyph indices to avoid redoing the decoding for
    // multiple atlases.
    while (text < text_end) {
        // Decode one Unicode codepoint from UTF-8
        uint32_t codepoint = *text > 0 ? *text++ : utf8_decode(&text);

        // Search for the range that contains this codepoint (if any)
        for (int i = 0; i < fnt->num_ranges; i++) {
            range_t *r = &fnt->ranges[i];
            if (codepoint >= r->first_codepoint && codepoint < r->first_codepoint + r->num_codepoints) {
                glyphs[n++] = r->first_glyph + codepoint - r->first_codepoint;
                break;
            }
        }
    }

    // Allocate an array that will hold the X position of each glyph.
    // We will fill this lazily in the first pass in the loop below.
    float *xpos = alloca((n+1) * sizeof(float));
    xpos[0] = 0.5f;  // start at center of pixel so that all rounds are to nearest
    bool first_loop = true;

    float advance_scale = draw_ctx.xscale * (1.0f / 64.0f);
    float kerning_scale = draw_ctx.xscale * (fnt->point_size / 127.0f);

    // Go through all the glyphs multiple times, one per atlas. Each time,
    // start from the first undrawn glyph, activate its atlas, and then draw
    // all the glyphs in the same atlas. Repeat until all the glyphs are drawn.
    int j = 0;
    while (j >= 0) {
        // Activate the atlas of the first undrawn glyph
        int a = fnt->glyphs[glyphs[j]].natlas;
        atlas_t *atlas = &fnt->atlases[a];
        rdpq_tile_t tile = atlas_activate(atlas);

        // Go through all the glyphs till the end, and draw the ones that are
        // part of the current atlas
        int first_undrawn = -1;
        for (int i = j; i < n; i++) {
            // If this glyph was already drawn, skip it
            if (glyphs[i] < 0)
                continue;
            glyph_t *g = &fnt->glyphs[glyphs[i]];

            // If this is the first loop, compute the X position of the glyph
            if (first_loop) {
                xpos[i+1] = xpos[i] + g->xadvance * advance_scale;

                // Check if there is kerning information for this glyph
                if (g->kerning_lo && i < n-1) {
                    // Do a binary search in the kerning table to look for the next glyph
                    int l = g->kerning_lo, r = g->kerning_hi;
                    int next = glyphs[i+1];
                    while (l <= r) {
                        int m = (l + r) / 2;
                        if (fnt->kerning[m].glyph2 == next) {
                            // Found the kerning value: add it to the X position
                            xpos[i+1] += fnt->kerning[m].kerning * kerning_scale;
                            break;
                        }
                        if (fnt->kerning[m].glyph2 < next)
                            l = m + 1;
                        else
                            r = m - 1;
                    }
                }
            }

            // If this glyph is not part of the current atlas, skip it. If it's
            // the first undrawn glyph, remember it.
            if (g->natlas != a) {
                if (first_undrawn < 0) first_undrawn = i;
                continue;
            }

            // Draw the glyph
            int width = g->xoff2 - g->xoff;
            int height = g->yoff2 - g->yoff;
            rdpq_texture_rectangle_scaled(tile, 
                draw_ctx.x + g->xoff * draw_ctx.xscale + xpos[i],
                draw_ctx.y + g->yoff * draw_ctx.yscale,
                draw_ctx.x + g->xoff2 * draw_ctx.xscale + xpos[i],
                draw_ctx.y + g->yoff2 * draw_ctx.yscale,
                g->s, g->t, g->s + width, g->t + height);

            // Mark the glyph as drawn
            glyphs[i] = -1;
        }

        j = first_undrawn;
        first_loop = false;
    }
}

void rdpq_font_printf(rdpq_font_t *fnt, const char *fmt, ...)
{
    char buf[256];
    va_list va;
    va_start(va, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, va);
    va_end(va);
    rdpq_font_printn(fnt, buf, n);
}

void rdpq_font_position(float x, float y)
{
    draw_ctx.x = x;
    draw_ctx.y = y;
}

void rdpq_font_begin(color_t color)
{
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (0,0,0,TEX0)));
    rdpq_mode_alphacompare(1);
    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
    rdpq_set_prim_color(color);
    draw_ctx = (struct draw_ctx_s){ .xscale = 1, .yscale = 1 };
}

void rdpq_font_end(void)
{
}


extern inline void rdpq_font_print(rdpq_font_t *fnt, const char *text);
