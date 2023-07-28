#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "n64sys.h"
#include "rspq.h"
#include "rdpq.h"
#include "rdpq_rect.h"
#include "surface.h"
#include "sprite.h"
#include "rdpq_mode.h"
#include "rdpq_tex.h"
#include "rdpq_sprite.h"
#include "rdpq_font.h"
#include "rdpq_paragraph.h"
#include "rdpq_font_internal.h"
#include "asset.h"

#define MAX_STYLES   256

#define GLYPH_SPECIAL_NEWLINE  -2
#define GLYPH_SPECIAL_SPACE    -3

_Static_assert(sizeof(glyph_t) == 16, "glyph_t size is wrong");
_Static_assert(sizeof(atlas_t) == 12, "atlas_t size is wrong");
_Static_assert(sizeof(kerning_t) == 3, "kerning_t size is wrong");

#define PTR_DECODE(font, ptr)    ((void*)(((uint8_t*)(font)) + (uint32_t)(ptr)))
#define PTR_ENCODE(font, ptr)    ((void*)(((uint8_t*)(ptr)) - (uint32_t)(font)))

/** @brief Drawing context */
static struct draw_ctx_s {
    atlas_t *last_atlas;
    float x;
    float y;
    float xscale, yscale;
} draw_ctx;

static void recalc_style(style_t *s)
{
    if (s->block)
        rdpq_call_deferred((void (*)(void*))rspq_block_free, s->block);

    rspq_block_begin();
        rdpq_mode_begin();
            rdpq_set_mode_standard();
            rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (0,0,0,TEX0)));
            rdpq_mode_alphacompare(1);
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
            rdpq_set_prim_color(s->color);
        rdpq_mode_end();
    s->block = rspq_block_end();
}

rdpq_font_t* rdpq_font_load_buf(void *buf, int sz)
{
    rdpq_font_t *fnt = buf;
    assertf(sz >= sizeof(rdpq_font_t), "Font buffer too small (sz=%d)", sz);
    assertf(memcmp(fnt->magic, FONT_MAGIC_LOADED, 3), "Trying to load already loaded font data (buf=%p, sz=%08x)", buf, sz);
    assertf(!memcmp(fnt->magic, FONT_MAGIC, 3), "invalid font data (magic: %c%c%c)", fnt->magic[0], fnt->magic[1], fnt->magic[2]);
    assertf(fnt->version == 3, "unsupported font version: %d\nPlease regenerate fonts with an updated mkfont tool", fnt->version);
    fnt->ranges = PTR_DECODE(fnt, fnt->ranges);
    fnt->glyphs = PTR_DECODE(fnt, fnt->glyphs);
    fnt->atlases = PTR_DECODE(fnt, fnt->atlases);
    fnt->kerning = PTR_DECODE(fnt, fnt->kerning);
    fnt->styles = PTR_DECODE(fnt, fnt->styles);
    for (int i = 0; i < fnt->num_atlases; i++) {
        void *buf = PTR_DECODE(fnt, fnt->atlases[i].sprite);
        fnt->atlases[i].sprite = sprite_load_buf(buf, fnt->atlases[i].size);
        rspq_block_begin();
            rdpq_sprite_upload(TILE0, fnt->atlases[i].sprite, NULL);
        fnt->atlases[i].up = rspq_block_end();
        debugf("Loaded atlas %d: %dx%d %s\n", i, fnt->atlases[i].sprite->width, fnt->atlases[i].sprite->height, tex_format_name(sprite_get_format(fnt->atlases[i].sprite)));
    }
    for (int i = 0; i < fnt->num_styles; i++)
        recalc_style(&fnt->styles[i]);
    memcpy(fnt->magic, FONT_MAGIC_LOADED, 3);
    data_cache_hit_writeback(fnt, sz);
    return fnt;
}

rdpq_font_t* rdpq_font_load(const char *fn)
{
    int sz;
    void *buf = asset_load(fn, &sz);
    rdpq_font_t *fnt = rdpq_font_load_buf(buf, sz);
    memcpy(fnt->magic, FONT_MAGIC_OWNED, 3);
    return fnt;
}

static void font_unload(rdpq_font_t *fnt)
{
    for (int i = 0; i < fnt->num_atlases; i++) {
        sprite_free(fnt->atlases[i].sprite);
        rspq_block_free(fnt->atlases[i].up); fnt->atlases[i].up = NULL;
        fnt->atlases[i].sprite = PTR_ENCODE(fnt, fnt->atlases[i].sprite);
    }
    for (int i = 0; i < MAX_STYLES; i++) {
        if (fnt->styles[i].block) {
            rspq_block_free(fnt->styles[i].block);
            fnt->styles[i].block = NULL;
        }
    }
    fnt->ranges = PTR_ENCODE(fnt, fnt->ranges);
    fnt->glyphs = PTR_ENCODE(fnt, fnt->glyphs);
    fnt->atlases = PTR_ENCODE(fnt, fnt->atlases);
    fnt->kerning = PTR_ENCODE(fnt, fnt->kerning);
    fnt->styles = PTR_ENCODE(fnt, fnt->styles);
    memcpy(fnt->magic, FONT_MAGIC, 3);
}

void rdpq_font_free(rdpq_font_t *fnt)
{
    bool owned = memcmp(fnt->magic, FONT_MAGIC_OWNED, 3) == 0;
    font_unload(fnt);

    if (owned) {
        #ifndef NDEBUG
        // To help debugging, zero the font structure
        memset(fnt, 0, sizeof(rdpq_font_t));
        #endif

        free(fnt);
    }
}

int16_t __rdpq_font_glyph(const rdpq_font_t *fnt, uint32_t codepoint)
{
    // Search for the range that contains this codepoint (if any)
    for (int i = 0; i < fnt->num_ranges; i++) {
        range_t *r = &fnt->ranges[i];
        if (codepoint >= r->first_codepoint && codepoint < r->first_codepoint + r->num_codepoints) {
            return r->first_glyph + codepoint - r->first_codepoint;
        }
    }
    return -1;
}

float __rdpq_font_kerning(const rdpq_font_t *fnt, int16_t glyph1, int16_t glyph2)
{
    glyph_t *g = &fnt->glyphs[glyph1];
    float kerning_scale = fnt->point_size / 127.0f;

    // Do a binary search in the kerning table to look for the next glyph
    int l = g->kerning_lo, r = g->kerning_hi;
    while (l <= r) {
        int m = (l + r) / 2;
        if (fnt->kerning[m].glyph2 == glyph2) {
            // Found the kerning value
            return fnt->kerning[m].kerning * kerning_scale;
        }
        if (fnt->kerning[m].glyph2 < glyph2)
            l = m + 1;
        else
            r = m - 1;
    }

    return 0;
}

void rdpq_font_style(rdpq_font_t *fnt, uint8_t style_id, const rdpq_fontstyle_t *style)
{
    // NOTE: fnt->num_styles refer to how many styles have been defined at
    // mkfont time. The font always contain room for 256 styles (all zeroed).
    style_t *s = &fnt->styles[style_id];
    s->color = style->color;
    recalc_style(s);
}
#include "rdpq_debug.h"
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

int rdpq_font_render_paragraph(const rdpq_font_t *fnt, const rdpq_paragraph_char_t *chars, float x0, float y0)
{
    uint8_t font_id = chars[0].font_id;
    int cur_atlas = -1;
    int cur_style = -1;

    draw_ctx.x = 0;
    draw_ctx.y = 0;
    draw_ctx.xscale = 1.0f;
    draw_ctx.yscale = 1.0f;
#if 1
    const rdpq_paragraph_char_t *ch = chars;
    while (ch->font_id == font_id) {
        const glyph_t *g = &fnt->glyphs[ch->glyph];
        if (UNLIKELY(g->natlas != cur_atlas)) {
            rspq_block_run(fnt->atlases[g->natlas].up);
            cur_atlas = g->natlas;
        }

        if (UNLIKELY(ch->style_id != cur_style)) {
            assertf(fnt->styles[ch->style_id].block, "style %d not defined in this font", ch->style_id);
            rspq_block_run(fnt->styles[ch->style_id].block);
            cur_style = ch->style_id;
        }

        // Draw the glyph
        float x = x0 + ch->x * 0.25f;
        float y = y0 + ch->y * 0.25f;
        int width = g->xoff2 - g->xoff;
        int height = g->yoff2 - g->yoff;

        float r0 = draw_ctx.x + g->xoff * draw_ctx.xscale + x;
        float r1 = draw_ctx.y + g->yoff * draw_ctx.yscale + y;
        rdpq_texture_rectangle_raw(TILE0,
            r0, r1, r0+width, r1+height,
            g->s, g->t, 1, 1);

        // rdpq_texture_rectangle_scaled(TILE0, 
        //     draw_ctx.x + g->xoff * draw_ctx.xscale + x,
        //     draw_ctx.y + g->yoff * draw_ctx.yscale + y,
        //     draw_ctx.x + g->xoff2 * draw_ctx.xscale + x,
        //     draw_ctx.y + g->yoff2 * draw_ctx.yscale + y,
        //     g->s, g->t, g->s + width, g->t + height);

        ch++;
    }

    return ch - chars;

#else
    bool first_loop = true;
    rdpq_paragraph_char_t *ch_last;
    rdpq_paragraph_char_t *ch_next = (rdpq_paragraph_char_t *)&chars[0];
    while (ch_next) {
        // Activate the atlas if it's changed
        const glyph_t *g = &fnt->glyphs[ch_next->glyph];
        if (g->natlas != cur_atlas) {
            rspq_block_run(fnt->atlases[g->natlas].up);
            cur_atlas = g->natlas;
        }
        // Set the style if it's changed
        if (ch_next->style_id != cur_style) {
            assertf(fnt->styles[ch_next->style_id].block, "style %d not defined in this font", ch_next->style_id);
            rspq_block_run(fnt->styles[ch_next->style_id].block);
            cur_style = ch_next->style_id;
        }

        rdpq_paragraph_char_t *ch_next_atlas = NULL;
        rdpq_paragraph_char_t *ch_next_style = NULL;
        rdpq_paragraph_char_t *ch = ch_next; ch_next = NULL;
        for (; ch->font_id == font_id; ch++) {
            if (!first_loop && ch->drawn) continue;
            if (first_loop) ch->drawn = false;

            // Fetch the glyph. Draw only glyphs in the same atlas and style
            const glyph_t *g = &fnt->glyphs[ch->glyph];
            if (UNLIKELY(g->natlas != cur_atlas)) {
                if (!ch_next_atlas) ch_next_atlas = ch;
                continue;
            }
            if (UNLIKELY(ch->style_id != cur_style)) {
                if (!ch_next_style) ch_next_style = ch;
                continue;
            }

            // Draw the glyph
            float x = x0 + ch->x * 0.25f;
            float y = y0 + ch->y * 0.25f;
            int width = g->xoff2 - g->xoff;
            int height = g->yoff2 - g->yoff;

            float r0 = draw_ctx.x + g->xoff * draw_ctx.xscale + x;
            float r1 = draw_ctx.y + g->yoff * draw_ctx.yscale + y;
            rdpq_texture_rectangle_raw(TILE0,
                r0, r1, r0+width, r1+height,
                g->s, g->t, 1, 1);

            // rdpq_texture_rectangle_scaled(TILE0, 
            //     draw_ctx.x + g->xoff * draw_ctx.xscale + x,
            //     draw_ctx.y + g->yoff * draw_ctx.yscale + y,
            //     draw_ctx.x + g->xoff2 * draw_ctx.xscale + x,
            //     draw_ctx.y + g->yoff2 * draw_ctx.yscale + y,
            //     g->s, g->t, g->s + width, g->t + height);

            ch->drawn = true;
        }

        // Go to the first character with a different style in the same
        // atlas, or if none, to the first character with a different atlas
        ch_next = ch_next_style ? ch_next_style : ch_next_atlas;

        ch_last = ch;
        first_loop = false;
    }

    return ch_last - chars;
#endif
}



#if 0

static void atlas_activate(atlas_t *atlas)
{
    if (draw_ctx.last_atlas != atlas) {
        rspq_block_run(atlas->up);
        draw_ctx.last_atlas = atlas;
    }
}

static void layout_finish_line(float *xystart, float *xyend, const rdpq_parparms_t *parms)
{
    // Adjust the line horizontal position according to the alignment
    float width = xyend[0] - xystart[0];

    int valign = 0;
    int align = 0;
    switch (parms->align) {
        case 0: // left
            break;
        case 1: // center
            align = (parms->width - width) / 2;
            break;
        case 2: // right
            align = parms->width - width;
            break;
    }

    // Apply alignment to the row
    for (float *xy = xystart; xy < xyend; xy += 2) {
        xy[0] += align;
        xy[1] += valign;
    }
}


static int layout_text(float *xypos, rdpq_font_t *fnt, int16_t *glyphs, int count, const rdpq_parparms_t *parms)
{
    static const rdpq_parparms_t empty_parms = {0};
    if (!parms) parms = (rdpq_parparms_t*)&empty_parms;

    float advance_scale = draw_ctx.xscale * (1.0f / 64.0f);
    float kerning_scale = draw_ctx.xscale * (fnt->point_size / 127.0f);
    float line_height = (fnt->ascent - fnt->descent + fnt->line_gap + parms->line_spacing) * draw_ctx.yscale;
    debugf("line_height=%f metrics:%ld,%ld,%ld\n", line_height, fnt->ascent, fnt->descent, fnt->line_gap);

    bool skip_until_newline = false;
    bool force_newline = false;
    int idx_last_space = -1;
    float *xyline = xypos;
    float *xy = xyline;
    xy[0] = 0.5f + parms->indent;  // start at center of pixel so that all rounds are to nearest
    xy[1] = 0.5f + parms->height ? fnt->ascent : 0;

    for (int i = 0; i < count; i++, xy += 2) {
        // Check for newline
        if (glyphs[i] == GLYPH_SPECIAL_NEWLINE || force_newline) {
            // The line is finished
            layout_finish_line(xypos, xy, parms);

            // Advance to next line
            xy[2] = 0.5f;
            xy[3] = xyline[1] + line_height;
            if (parms->height && xy[3] - fnt->descent > parms->height)
                return i;
            xyline = xy + 2;
            skip_until_newline = false;
            force_newline = false;
            continue;
        }
        if (skip_until_newline) {
            // Skip this glyph.
            glyphs[i] = -1;
            xy[2] = 0; xy[3] = 0;
            continue;
        }

        if (glyphs[i] == GLYPH_SPECIAL_SPACE) {
            idx_last_space = i;
            xy[2] = xy[0] + fnt->space_width;
            xy[3] = xy[1];
            continue;
        }

        glyph_t *g = &fnt->glyphs[glyphs[i]];
        
        // Advance glyph on this same line
        xy[2] = xy[0] + g->xadvance * advance_scale;
        xy[3] = xy[1];

        // Check if there is kerning information for this glyph
        if (g->kerning_lo && i < count-1) {
            // Do a binary search in the kerning table to look for the next glyph
            int l = g->kerning_lo, r = g->kerning_hi;
            int next = glyphs[i+1];
            while (l <= r) {
                int m = (l + r) / 2;
                if (fnt->kerning[m].glyph2 == next) {
                    // Found the kerning value: add it to the X position
                    xy[2] += fnt->kerning[m].kerning * kerning_scale;
                    break;
                }
                if (fnt->kerning[m].glyph2 < next)
                    l = m + 1;
                else
                    r = m - 1;
            }
        }
    
        // Check if we are limited in width
        float last_pixel = xy[0] + g->xoff2 * draw_ctx.xscale;
        if (parms->width && last_pixel > parms->width) {
            // Check if we are allowed to wrap
            switch (parms->wrap) {
                case WRAP_NONE:
                    // The text doesn't fit on this line anymore.
                    // Skip the rest of the line.
                    glyphs[i] = -1;
                    skip_until_newline = true;
                    break;

                // Check if we are wrapping by word
                case WRAP_WORD:
                    // Find the last space in the line
                    if (idx_last_space < 0) break;

                    // Wrap at the last space
                    xy -= 2 * (i - idx_last_space);
                    i = idx_last_space;
                    // fallthrough
                case WRAP_CHAR:
                    i--; xy -= 2;
                    force_newline = true;
                    break;
            }
        }
    }

    return count;
}

void rdpq_font_printn(rdpq_font_t *fnt, const char *text, int nch, const rdpq_parparms_t *parms)
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

        // Handle newline. These are not encoded in the font proper as it's
        // just whitespaces but we need to handle them for layout.
        if (codepoint == '\n') {
            glyphs[n++] = GLYPH_SPECIAL_NEWLINE;
            continue;
        }
        if (codepoint == ' ') {
            glyphs[n++] = GLYPH_SPECIAL_SPACE;
            continue;
        }

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
    float *xypos = alloca((n+1) * sizeof(float) * 2);

    // Do the layout
    n = layout_text(xypos, fnt, glyphs, n, parms);

    // Go through all the glyphs multiple times, one per atlas. Each time,
    // start from the first undrawn glyph, activate its atlas, and then draw
    // all the glyphs in the same atlas. Repeat until all the glyphs are drawn.
    int j = 0;
    while (j >= 0) {
        // Activate the atlas of the first undrawn glyph
        int a = fnt->glyphs[glyphs[j]].natlas;
        atlas_t *atlas = &fnt->atlases[a];
        atlas_activate(atlas);

        // Go through all the glyphs till the end, and draw the ones that are
        // part of the current atlas
        int first_undrawn = -1;
        for (int i = j; i < n; i++) {
            // If this glyph was already drawn, skip it
            if (glyphs[i] < 0)
                continue;
            glyph_t *g = &fnt->glyphs[glyphs[i]];

            // If this glyph is not part of the current atlas, skip it. If it's
            // the first undrawn glyph, remember it.
            if (g->natlas != a) {
                if (first_undrawn < 0) first_undrawn = i;
                continue;
            }

            // Draw the glyph
            int width = g->xoff2 - g->xoff;
            int height = g->yoff2 - g->yoff;
            rdpq_texture_rectangle_scaled(TILE0, 
                draw_ctx.x + g->xoff * draw_ctx.xscale + xypos[i*2+0],
                draw_ctx.y + g->yoff * draw_ctx.yscale + xypos[i*2+1],
                draw_ctx.x + g->xoff2 * draw_ctx.xscale + xypos[i*2+0],
                draw_ctx.y + g->yoff2 * draw_ctx.yscale + xypos[i*2+1],
                g->s, g->t, g->s + width, g->t + height);

            // Mark the glyph as drawn
            glyphs[i] = -1;
        }

        j = first_undrawn;
    }
}

void rdpq_font_printf(rdpq_font_t *fnt, const rdpq_parparms_t *parms, const char *fmt, ...)
{
    char buf[256];
    va_list va;
    va_start(va, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, va);
    va_end(va);
    rdpq_font_printn(fnt, buf, n, parms);
}
#endif

void rdpq_font_position(float x, float y)
{
    draw_ctx.x = x;
    draw_ctx.y = y;
}

void rdpq_font_begin(color_t color)
{
    rdpq_mode_begin();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (0,0,0,TEX0)));
        rdpq_mode_alphacompare(1);
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_set_prim_color(color);
    rdpq_mode_end();
    draw_ctx = (struct draw_ctx_s){ .xscale = 1, .yscale = 1 };
}

void rdpq_font_scale(float xscale, float yscale)
{
    draw_ctx.xscale = xscale;
    draw_ctx.yscale = yscale;
}

void rdpq_font_end(void)
{
}


// extern inline void rdpq_font_print(rdpq_font_t *fnt, const char *text, const rdpq_parparms_t *parms);
extern inline void __rdpq_font_glyph_metrics(const rdpq_font_t *fnt, int16_t index, float *xadvance, int8_t *xoff, int8_t *xoff2, bool *has_kerning, uint8_t *sort_key);
