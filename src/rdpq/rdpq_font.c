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
#include "utils.h"

// Include the built-in font data
#include "rdpq_font_builtin.c"

#define MAX_STYLES   256

_Static_assert(sizeof(glyph_t) == 16, "glyph_t size is wrong");
_Static_assert(sizeof(atlas_t) == 12, "atlas_t size is wrong");
_Static_assert(sizeof(kerning_t) == 3, "kerning_t size is wrong");

#define PTR_DECODE(font, ptr)    ((void*)(((uint8_t*)(font)) + (uint32_t)(ptr)))
#define PTR_ENCODE(font, ptr)    ((void*)(((uint8_t*)(ptr)) - (uint32_t)(font)))

static void recalc_style(int font_type, style_t *s)
{
    if (s->block)
        rdpq_call_deferred((void (*)(void*))rspq_block_free, s->block);

    rspq_block_begin();
            switch (font_type) {
            case FONT_TYPE_ALIASED:
                // Atlases are simple I4 textures, so we just need to colorize
                // them using the PRIM color. We also use PRIM alpha to control
                // additional transparency of the text.
                rdpq_mode_begin();
                    rdpq_set_mode_standard();
                    rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (TEX0,0,PRIM,0)));
                    rdpq_mode_alphacompare(1);
                    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
                rdpq_mode_end();
                rdpq_set_prim_color(s->color);
                break;
            case FONT_TYPE_MONO:
                // Monochrome fonts use CI4 textures with 4 1bpp layers, and special
                // palettes to isolate each layer. PRIM control the color and
                // the transparency of the text.
                rdpq_mode_begin();
                    rdpq_set_mode_standard();
                    rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (TEX0,0,PRIM,0)));
                    rdpq_mode_alphacompare(1);
                    rdpq_mode_tlut(TLUT_RGBA16);
                rdpq_mode_end();
                rdpq_set_prim_color(s->color);
                break;
            case FONT_TYPE_MONO_OUTLINE:
                // Mono-outline fonts are CI4 with IA16 palettes. Each texel is
                // a IA16 color as follows: 0x0000=transparent, 0xFFFF=fill, 0x00FF=outline
                // So TEX will become either 0x00 or 0xFF, and TEX_ALPHA will be 0x20 (or 0 for transparent)
                // We set a combiner that does PRIM*TEX + ENV*(1-TEX), so that we can
                // select between the fill and the outline color, in PRIM and ENV respectively.
                // Unfortunately, we can't use alpha compare with a two-stage combiner because of
                // a RDP bug; so we simulate it using SOM_BLALPHA_CVG_TIMES_CC which multiplies
                // the alpha by the coverage (which should be full on all pixels) before hitting
                // the blender, and this causes a similar transparent effect.
                // to turn on AA for this to work (for unknown reasons).
                rdpq_mode_begin();
                    rdpq_set_mode_standard();
                    rdpq_mode_combiner(RDPQ_COMBINER2(
                        (ONE,TEX1,ENV,0),       (0,0,0,TEX1),
                        (TEX1,0,PRIM,COMBINED), (0,0,0,COMBINED)
                    ));
                    rdpq_mode_antialias(AA_REDUCED);
                    rdpq_mode_tlut(TLUT_IA16);
                rdpq_mode_end();
                rdpq_change_other_modes_raw(SOM_BLALPHA_MASK, SOM_BLALPHA_CVG_TIMES_CC);
                rdpq_set_prim_color(s->color);
                rdpq_set_env_color(s->outline_color);
                break;
            case FONT_TYPE_ALIASED_OUTLINE:
                // Atlases are IA8, where I modulates between the fill color and the outline color.
                rdpq_mode_begin();
                    rdpq_set_mode_standard();
                    rdpq_mode_combiner(RDPQ_COMBINER2(
                        (ONE,TEX1,ENV,0),       (0,0,0,TEX1),
                        (TEX1,0,PRIM,COMBINED), (0,0,0,COMBINED)
                    ));
                    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
                rdpq_mode_end();
                rdpq_change_other_modes_raw(SOM_BLALPHA_MASK, SOM_BLALPHA_CVG_TIMES_CC);
                rdpq_set_prim_color(s->color);
                rdpq_set_env_color(s->outline_color);
                break;
            default:
                assert(0);
            }

    s->block = rspq_block_end();
}

rdpq_font_t* rdpq_font_load_buf(void *buf, int sz)
{
    assertf(__rdpq_inited, "rdpq_init() must be called before loading a font");
    rdpq_font_t *fnt = buf;
    assertf(sz >= sizeof(rdpq_font_t), "Font buffer too small (sz=%d)", sz);
    assertf(memcmp(fnt->magic, FONT_MAGIC_LOADED, 3), "Trying to load already loaded font data (buf=%p, sz=%08x)", buf, sz);
    assertf(!memcmp(fnt->magic, FONT_MAGIC, 3), "invalid font data (magic: %c%c%c)", fnt->magic[0], fnt->magic[1], fnt->magic[2]);
    assertf(fnt->version == 7, "unsupported font version: %d\nPlease regenerate fonts with an updated mkfont tool", fnt->version);
    fnt->ranges = PTR_DECODE(fnt, fnt->ranges);
    fnt->glyphs = PTR_DECODE(fnt, fnt->glyphs);
    fnt->atlases = PTR_DECODE(fnt, fnt->atlases);
    fnt->kerning = PTR_DECODE(fnt, fnt->kerning);
    fnt->styles = &fnt->builtin_style;
    fnt->num_styles = 1;
    for (int i = 0; i < fnt->num_atlases; i++) {
        void *buf = PTR_DECODE(fnt, fnt->atlases[i].sprite);
        fnt->atlases[i].sprite = sprite_load_buf(buf, fnt->atlases[i].size);
        rspq_block_begin();
            int font_type = fnt->flags & FONT_FLAG_TYPE_MASK;
            switch (font_type) {
            case FONT_TYPE_MONO: {
                surface_t surf = sprite_get_pixels(fnt->atlases[i].sprite);
                rdpq_tex_multi_begin();
                rdpq_tex_upload(TILE0, &surf, NULL);
                rdpq_tex_reuse(TILE1, &(rdpq_texparms_t){ .palette = 1 });
                rdpq_tex_reuse(TILE2, &(rdpq_texparms_t){ .palette = 2 });
                rdpq_tex_reuse(TILE3, &(rdpq_texparms_t){ .palette = 3 });
                rdpq_tex_multi_end();
                rdpq_tex_upload_tlut(sprite_get_palette(fnt->atlases[i].sprite), 0, font_type == FONT_TYPE_MONO ? 64 : 32);
                break;
            }
            case FONT_TYPE_MONO_OUTLINE: {
                surface_t surf = sprite_get_pixels(fnt->atlases[i].sprite);
                rdpq_tex_multi_begin();
                // Outline font uses only TILE1 and TILE2 because the combiner only uses
                // TEX1 and never TEX0 (see recalc_style).
                rdpq_tex_upload(TILE1, &surf, NULL);
                rdpq_tex_reuse(TILE2, &(rdpq_texparms_t){ .palette = 1 });
                rdpq_tex_multi_end();
                rdpq_tex_upload_tlut(sprite_get_palette(fnt->atlases[i].sprite), 0, font_type == FONT_TYPE_MONO ? 64 : 32);
                break;
            }
            case FONT_TYPE_ALIASED_OUTLINE:
                rdpq_sprite_upload(TILE1, fnt->atlases[i].sprite, NULL);
                break;
            case FONT_TYPE_ALIASED:
            default:
                rdpq_sprite_upload(TILE0, fnt->atlases[i].sprite, NULL);
                break;
            }

        fnt->atlases[i].up = rspq_block_end();
    }

    for (int i = 0; i < fnt->num_styles; i++)
        recalc_style(fnt->flags & FONT_FLAG_TYPE_MASK, &fnt->styles[i]);
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
    for (int i = 0; i < fnt->num_styles; i++) {
        if (fnt->styles[i].block) {
            rspq_block_free(fnt->styles[i].block);
            fnt->styles[i].block = NULL;
        }
    }
    if (fnt->num_styles > 1) {
        free(fnt->styles);
        fnt->styles = &fnt->builtin_style;
        fnt->num_styles = 1;
    }
    fnt->ranges = PTR_ENCODE(fnt, fnt->ranges);
    fnt->glyphs = PTR_ENCODE(fnt, fnt->glyphs);
    fnt->atlases = PTR_ENCODE(fnt, fnt->atlases);
    fnt->kerning = PTR_ENCODE(fnt, fnt->kerning);
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
    if (style_id >= fnt->num_styles) {
        assertf(style_id < MAX_STYLES, "style_id %d exceeds maximum %d", style_id, MAX_STYLES);

        if (fnt->num_styles == 1) {
            fnt->num_styles = 16;
            fnt->styles = calloc(16, sizeof(style_t));
            memcpy(&fnt->styles[0], &fnt->builtin_style, sizeof(style_t));
            fnt->builtin_style.block = NULL;
        } else {
            int old_styles = fnt->num_styles;
            fnt->num_styles = MAX(fnt->num_styles*2, MAX_STYLES);
            fnt->styles = realloc(fnt->styles, fnt->num_styles * sizeof(style_t));
            memset(&fnt->styles[old_styles], 0, (fnt->num_styles - old_styles) * sizeof(style_t));
        }
    }

    style_t *s = &fnt->styles[style_id];
    s->color = style->color;
    s->outline_color = style->outline_color;
    recalc_style(fnt->flags & FONT_FLAG_TYPE_MASK, s);
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
            assertf(ch->style_id < fnt->num_styles && fnt->styles[ch->style_id].block,
                 "style %d not defined in this font", ch->style_id);
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
        int width = g->xoff2 - g->xoff;
        int height = g->yoff2 - g->yoff;

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

bool rdpq_font_get_glyph_ranges(const rdpq_font_t *fnt, int idx, uint32_t *start, uint32_t *end)
{
    if (idx < 0 || idx >= fnt->num_ranges)
        return false;
    *start = fnt->ranges[idx].first_codepoint;
    *end = fnt->ranges[idx].first_codepoint + fnt->ranges[idx].num_codepoints - 1;
    return true;
}

bool rdpq_font_get_glyph_metrics(const rdpq_font_t *fnt,  uint32_t codepoint, rdpq_font_gmetrics_t *metrics)
{
    int16_t glyph = __rdpq_font_glyph(fnt, codepoint);
    if (glyph < 0)
        return false;

    glyph_t *g = &fnt->glyphs[glyph];
    metrics->xadvance = g->xadvance * (1.0f / 64.0f);
    metrics->x0 = g->xoff;
    metrics->y0 = g->yoff;
    metrics->x1 = g->xoff2;
    metrics->y1 = g->yoff2;
    return true;
}

rdpq_font_t *__rdpq_font_load_builtin_1(void)
{
    return rdpq_font_load_buf((void*)__fontdb_monogram, __fontdb_monogram_len);
}

rdpq_font_t *__rdpq_font_load_builtin_2(void)
{
    return rdpq_font_load_buf((void*)__fontdb_at01, __fontdb_at01_len);
}


extern inline void __rdpq_font_glyph_metrics(const rdpq_font_t *fnt, int16_t index, float *xadvance, int8_t *xoff, int8_t *xoff2, bool *has_kerning, uint8_t *sort_key);
extern inline rdpq_font_t* rdpq_font_load_builtin(rdpq_font_builtin_t font);

