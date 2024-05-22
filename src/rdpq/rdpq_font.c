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
#include "rdpq_text.h"
#include "rdpq_paragraph.h"
#include "rdpq_font_internal.h"
#include "rdpq_internal.h"
#include "asset.h"
#include "fmath.h"

#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define MAX_STYLES   256

_Static_assert(sizeof(glyph_t) == 16, "glyph_t size is wrong");
_Static_assert(sizeof(atlas_t) == 12, "atlas_t size is wrong");
_Static_assert(sizeof(kerning_t) == 3, "kerning_t size is wrong");

#define PTR_DECODE(font, ptr)    ((void*)(((uint8_t*)(font)) + (uint32_t)(ptr)))
#define PTR_ENCODE(font, ptr)    ((void*)(((uint8_t*)(ptr)) - (uint32_t)(font)))

static void recalc_style(style_t *s, tex_format_t fmt)
{
    if (s->block)
        rdpq_call_deferred((void (*)(void*))rspq_block_free, s->block);

    rspq_block_begin();
        rdpq_mode_begin();
            switch (fmt) {
            case FMT_I4: case FMT_I8:
                rdpq_set_mode_standard();
                rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (TEX0,0,PRIM,0)));
                rdpq_mode_alphacompare(1);
                rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
                rdpq_set_prim_color(s->color);
                break;
            case FMT_CI4: case FMT_CI8:
                rdpq_set_mode_standard();
                rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (TEX0,0,PRIM,0)));
                rdpq_mode_alphacompare(1);
                rdpq_mode_tlut(TLUT_RGBA16);
                rdpq_set_prim_color(s->color);
                break;
            default:
                assert(0);
            }
        rdpq_mode_end();
    s->block = rspq_block_end();
}

rdpq_font_t* rdpq_font_load_buf(void *buf, int sz)
{
    assertf(__rdpq_inited, "rdpq_init() must be called before loading a font");
    rdpq_font_t *fnt = buf;
    assertf(sz >= sizeof(rdpq_font_t), "Font buffer too small (sz=%d)", sz);
    assertf(memcmp(fnt->magic, FONT_MAGIC_LOADED, 3), "Trying to load already loaded font data (buf=%p, sz=%08x)", buf, sz);
    assertf(!memcmp(fnt->magic, FONT_MAGIC, 3), "invalid font data (magic: %c%c%c)", fnt->magic[0], fnt->magic[1], fnt->magic[2]);
    assertf(fnt->version == 4, "unsupported font version: %d\nPlease regenerate fonts with an updated mkfont tool", fnt->version);
    fnt->ranges = PTR_DECODE(fnt, fnt->ranges);
    fnt->glyphs = PTR_DECODE(fnt, fnt->glyphs);
    fnt->atlases = PTR_DECODE(fnt, fnt->atlases);
    fnt->kerning = PTR_DECODE(fnt, fnt->kerning);
    fnt->styles = PTR_DECODE(fnt, fnt->styles);
    for (int i = 0; i < fnt->num_atlases; i++) {
        void *buf = PTR_DECODE(fnt, fnt->atlases[i].sprite);
        fnt->atlases[i].sprite = sprite_load_buf(buf, fnt->atlases[i].size);
        rspq_block_begin();
            switch (sprite_get_format(fnt->atlases[i].sprite)) {
            case FMT_CI4:
                surface_t surf = sprite_get_pixels(fnt->atlases[i].sprite);
                rdpq_tex_multi_begin();
                rdpq_tex_upload(TILE0, &surf, NULL);
                rdpq_tex_reuse(TILE1, &(rdpq_texparms_t){ .palette = 1 });
                rdpq_tex_reuse(TILE2, &(rdpq_texparms_t){ .palette = 2 });
                rdpq_tex_reuse(TILE3, &(rdpq_texparms_t){ .palette = 3 });
                rdpq_tex_multi_end();
                rdpq_tex_upload_tlut(sprite_get_palette(fnt->atlases[i].sprite), 0, 64);
                break;

            default:
                rdpq_sprite_upload(TILE0, fnt->atlases[i].sprite, NULL);
                break;
            }

        fnt->atlases[i].up = rspq_block_end();
    }

    tex_format_t fmt = sprite_get_format(fnt->atlases[0].sprite);
    for (int i = 0; i < fnt->num_styles; i++)
        recalc_style(&fnt->styles[i], fmt);
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
    tex_format_t fmt = sprite_get_format(fnt->atlases[0].sprite);
    recalc_style(s, fmt);
}

int rdpq_font_render_paragraph(const rdpq_font_t *fnt, const rdpq_paragraph_char_t *chars, float x0, float y0)
{
    uint8_t font_id = chars[0].font_id;
    int cur_atlas = -1;
    int cur_style = -1;

    const rdpq_paragraph_char_t *ch = chars;
    while (ch->font_id == font_id) {
        const glyph_t *g = &fnt->glyphs[ch->glyph];
        if (UNLIKELY(ch->style_id != cur_style)) {
            assertf(fnt->styles[ch->style_id].block, "style %d not defined in this font", ch->style_id);
            rspq_block_run(fnt->styles[ch->style_id].block);
            cur_style = ch->style_id;
        }
        if (UNLIKELY(g->natlas != cur_atlas)) {
            rspq_block_run(fnt->atlases[g->natlas].up);
            cur_atlas = g->natlas;
        }

        // Draw the glyph
        float x = x0 + (ch->x + g->xoff);
        float y = y0 + (ch->y + g->yoff);
        int width = (g->xoff2 - g->xoff + 1);
        int height = (g->yoff2 - g->yoff + 1);

        rdpq_texture_rectangle(g->ntile,
            x, y, x+width, y+height,
            g->s, g->t);

        // rdpq_texture_rectangle_scaled(TILE0, 
        //     x, y, x + width * draw_ctx.xscale, y + height * draw_ctx.yscale,
        //     g->s, g->t, g->s + width, g->t + height);

        ch++;
    }

    return ch - chars;
}

// extern inline void rdpq_font_print(rdpq_font_t *fnt, const char *text, const rdpq_parparms_t *parms);
extern inline void __rdpq_font_glyph_metrics(const rdpq_font_t *fnt, int16_t index, float *xadvance, int8_t *xoff, int8_t *xoff2, bool *has_kerning, uint8_t *sort_key);
