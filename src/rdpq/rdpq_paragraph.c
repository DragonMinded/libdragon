#include "rdpq_paragraph.h"
#include "rdpq_text.h"
#include "rdpq_font.h"
#include "rdpq_font_internal.h"
#include "debug.h"
#include "fmath.h"
#include <stdlib.h>
#include <assert.h>

#define UNLIKELY(x) __builtin_expect(!!(x), 0)

void __rdpq_paragraph_builder_newline(int ch_newline);

static struct {
    rdpq_paragraph_t *layout;
    const rdpq_textparms_t *parms;
    const rdpq_font_t *font;
    uint8_t font_id;
    uint8_t style_id;
    float xscale, yscale; // FIXME: this should be a drawing context
    float x, y;
    int ch_line_start;
    int ch_last_space;
    bool skip_current_line;
    bool must_sort;
} builder;

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

bool rdpq_paragraph_builder_full(void)
{
    return builder.parms->height && builder.y - builder.font->descent >= builder.parms->height;
}

void rdpq_paragraph_builder_begin(const rdpq_textparms_t *parms, uint8_t initial_font_id, rdpq_paragraph_t *layout)
{
    assertf(initial_font_id > 0, "invalid usage of font ID 0 (reserved)");

    memset(&builder, 0, sizeof(builder));

    if (parms) {
        if (parms->wrap) assertf(parms->width, "wrapping modes require a width");
        assertf(parms->width >= 0, "width must be positive");
        assertf(parms->height >= 0, "height must be positive");
    }
    static const rdpq_textparms_t empty_parms = {0};
    builder.parms = parms ? parms : &empty_parms;

    if (!layout) {
        const int initial_chars = 256;
        layout = malloc(sizeof(rdpq_paragraph_t) + sizeof(rdpq_paragraph_char_t) * initial_chars);
        memset(layout, 0, sizeof(*layout));
        layout->capacity = initial_chars;
    }
    builder.layout = layout;

    builder.xscale = 1.0f;
    builder.yscale = 1.0f;
    rdpq_paragraph_builder_font(initial_font_id);
    // start at center of pixel so that all rounds are to nearest
    builder.x = builder.parms->indent;
    builder.y = (builder.parms->height ? builder.font->ascent : 0);
    builder.skip_current_line = rdpq_paragraph_builder_full();
    builder.ch_last_space = -1;
}

void rdpq_paragraph_builder_font(uint8_t font_id)
{
    builder.must_sort |= builder.font_id > font_id;
    builder.font_id = font_id;
    builder.font = rdpq_text_get_font(font_id);
    assertf(builder.font, "font %d not registered", font_id);
    builder.style_id = 0;

    if (builder.parms->wrap == WRAP_ELLIPSES)
        assertf(builder.font->ellipsis_glyph && builder.font->ellipsis_reps, 
            "ellipses wrap mode requires an ellipses glyph to be specified in the font");
}

void rdpq_paragraph_builder_style(uint8_t style_id)
{
    builder.must_sort |= builder.style_id > style_id;
    builder.style_id = style_id;
}

static bool paragraph_wrap(int wrapchar, float *xcur, float *ycur)
{
    // Force a newline at wrapchar. If the newline doesn't fit vertically,
    // there's nothing more to do and we can return false.
    builder.x = *xcur;
    builder.y = *ycur;
    __rdpq_paragraph_builder_newline(wrapchar);
    if (builder.skip_current_line) {
        builder.layout->nchars = wrapchar;
        return false;
    }

    // If the wrapchar is the last char, we're done
    if (wrapchar == builder.layout->nchars) {
        *xcur = builder.x;
        *ycur = builder.y;
        return true;
    }

    rdpq_paragraph_char_t *ch = &builder.layout->chars[wrapchar];
    rdpq_paragraph_char_t *end = &builder.layout->chars[builder.layout->nchars];

    // Calculate wrap translation
    float offx = builder.x - ch[0].x;
    float offy = builder.y - ch[0].y;

    // Translate all the characters between wrapchar and the end
    while (ch < end) {
        ch->x += offx;
        ch->y += offy;
        ++ch;
    }

    // Translate also the endpoint, so that it now points at the end of the
    // translated characters
    *xcur = *xcur + offx;
    *ycur = *ycur + offy;
    return true;
}

void rdpq_paragraph_builder_span(const char *utf8_text, int nbytes)
{
    // We're skipping the current line, so this span isn't useful
    if (builder.skip_current_line) return;

    const rdpq_font_t *fnt = builder.font;
    const rdpq_textparms_t *parms = builder.parms;
    const char *end = utf8_text + nbytes;
    float xcur = builder.x;
    float ycur = builder.y;
    int16_t next_index = -1;
    bool is_space = false;

    #define UTF8_DECODE_NEXT() ({ \
        uint32_t codepoint = *utf8_text > 0 ? *utf8_text++ : utf8_decode(&utf8_text); \
        is_space = (codepoint == ' '); \
        __rdpq_font_glyph(builder.font, codepoint); \
    })

    while (utf8_text < end || next_index >= 0) {
        int16_t index = next_index; next_index = -1;
        if (index < 0) index = UTF8_DECODE_NEXT();
        if (UNLIKELY(index < 0)) continue;

        float xadvance; int8_t xoff2; bool has_kerning; uint8_t atlas_id;
        __rdpq_font_glyph_metrics(fnt, index, &xadvance, NULL, &xoff2, &has_kerning, &atlas_id);

        if (!is_space) {
            builder.layout->chars[builder.layout->nchars++] = (rdpq_paragraph_char_t) {
                .font_id = builder.font_id,
                .atlas_id = atlas_id,
                .style_id = builder.style_id,
                .glyph = index,
                .x = xcur+.5f,
                .y = ycur+.5f,
            };
        } else {       
            builder.ch_last_space = builder.layout->nchars;
        }

        float last_pixel = xcur + xoff2 * builder.xscale;

        xcur += xadvance * builder.xscale;

        if (UNLIKELY(has_kerning && utf8_text < end)) {
            next_index = UTF8_DECODE_NEXT();
            if (next_index >= 0) {
                float kerning = __rdpq_font_kerning(fnt, index, next_index);
                xcur += kerning * builder.xscale;
            }
        }

        // Round to nearest pixel when we find a space. This makes all words
        // start from a pixel boundary, which means words will always look
        // the same in any rendition (since, depending on resolution, a single
        // pixel of relative distance between letters can be very visible).
        if (is_space) xcur = roundf(xcur);

        // Check if we are limited in width
        if (UNLIKELY(parms->width) && UNLIKELY(last_pixel > parms->width)) {
            // Check if we are allowed to wrap
            switch (parms->wrap) {
                case WRAP_CHAR:
                    if (!paragraph_wrap(builder.layout->nchars-1, &xcur, &ycur))
                        return;
                    break;
                case WRAP_WORD:
                    // Find the last space in the line
                    if (builder.ch_last_space >= 0) {
                        if (!paragraph_wrap(builder.ch_last_space, &xcur, &ycur))
                            return;
                        builder.ch_last_space = -1;
                        break;
                    }
                    builder.layout->nchars -= 1;
                    // fallthrough!
                case WRAP_ELLIPSES: {
                    const rdpq_font_t *wfnt = fnt;

                    // Go backward in the string until we find a good position
                    // where to put the ellipsis.
                    float ellipsis_x = 0;
                    int wrapchar = builder.layout->nchars-1;
                    rdpq_paragraph_char_t *wrapch = &builder.layout->chars[wrapchar];
                    while (wrapchar > builder.ch_line_start) {
                        wfnt = rdpq_text_get_font(wrapch[-1].font_id);

                        // Compute the advance of the previous characters and calculate the position
                        // at which we could put the ellipsis. This may be different from wrapch[0].x
                        // because of whitespaces between wrapch[-1] and wrapch[0].
                        float prev_advance;
                        __rdpq_font_glyph_metrics(wfnt, wrapch[-1].glyph, &prev_advance, NULL, NULL, NULL, NULL);
                        ellipsis_x = wrapch[-1].x + prev_advance * builder.xscale;

                        // Check if we can put the ellipsis here
                        if (ellipsis_x + wfnt->ellipsis_width < parms->width)
                            break;

                        wrapchar -= 1;
                        wrapch -= 1;
                    }

                    uint8_t ellipsis_atlas_id;
                    __rdpq_font_glyph_metrics(wfnt, wfnt->ellipsis_glyph, NULL, NULL, NULL, NULL, &ellipsis_atlas_id);

                    uint8_t wrap_font_id = wrapch[-1].font_id, wrap_style_id = wrapch[-1].style_id;
                    for (int i=0; i<wfnt->ellipsis_reps; i++) {
                        builder.layout->chars[wrapchar+i] = (rdpq_paragraph_char_t) {
                            .font_id = wrap_font_id,
                            .atlas_id = ellipsis_atlas_id,
                            .style_id = wrap_style_id,
                            .glyph = wfnt->ellipsis_glyph,
                            .x = (ellipsis_x + wfnt->ellipsis_advance * i * builder.xscale) + .5f,
                            .y = wrapch[-1].y + .5f,
                        };
                    }
                    builder.layout->nchars = wrapchar + fnt->ellipsis_reps;
                }   // fallthrough!
                case WRAP_NONE:
                    // The text doesn't fit on this line anymore.
                    // Skip the rest of the line, including the rest of this span,
                    // including the current character.
                    builder.skip_current_line = true;
                    return;
            }
        }
    }

    builder.x = xcur;
    builder.y = ycur;
}

void __rdpq_paragraph_builder_newline(int ch_newline)
{
    float line_height = 0.0f;
    
    line_height += builder.font->ascent - builder.font->descent + builder.font->line_gap;
    line_height += builder.parms->line_spacing;
     
    builder.y += line_height * builder.yscale;
    builder.x = 0;
    builder.skip_current_line = builder.parms->height && builder.y - builder.font->descent >= builder.parms->height;
    builder.layout->nlines += 1;

    int ix0 = builder.ch_line_start;
    int ix1 = ch_newline;

    // If there's at least one character on this line
    if (ix0 != ix1) {
        rdpq_paragraph_char_t* ch0 = &builder.layout->chars[ix0];
        rdpq_paragraph_char_t* ch1 = &builder.layout->chars[ix1-1];

        const rdpq_font_t *fnt0 = rdpq_text_get_font(ch0->font_id); assert(fnt0);
        const rdpq_font_t *fnt1 = rdpq_text_get_font(ch1->font_id); assert(fnt1);

        // Extract X of first pixel of first char, and last pixel of last char
        // This is a slightly more accurate centering than just using the glyph position
        int8_t off_x0, off_x1;
        __rdpq_font_glyph_metrics(fnt0, ch0->glyph, NULL, &off_x0, NULL,    NULL, NULL);
        __rdpq_font_glyph_metrics(fnt1, ch1->glyph, NULL, NULL,    &off_x1, NULL, NULL);

        // Compute absolute x0/x1 in the paragraph
        float x0 = ch0->x + off_x0 * builder.xscale;
        float x1 = ch1->x + off_x1 * builder.xscale;

        // Do right/center alignment of the row (and adjust extents)
        if (UNLIKELY(builder.parms->width && builder.parms->align)) {
            float offset = builder.parms->width - (x1 - x0);
            if (builder.parms->align == ALIGN_CENTER) offset *= 0.5f;

            int16_t offset_fx = offset;
            for (rdpq_paragraph_char_t *ch = ch0; ch <= ch1; ++ch)
                ch->x += offset_fx;
            x0 += offset;
            x1 += offset;
        }

        // Update bounding box
        bool first_line = builder.layout->nlines == 1;
        if (first_line || builder.layout->bbox.x0 > x0) builder.layout->bbox.x0 = x0;
        if (first_line || builder.layout->bbox.x1 < x1) builder.layout->bbox.x1 = x1;
    }

    builder.ch_line_start = ch_newline;
}

void rdpq_paragraph_builder_newline(void)
{
    __rdpq_paragraph_builder_newline(builder.layout->nchars);
}


static int char_compare(const void *a, const void *b)
{
    const rdpq_paragraph_char_t *ca = a, *cb = b;
    return (ca->sort_key & 0xFFFFFF00) - (cb->sort_key & 0xFFFFFF00);
}

static void insertion_sort_char_array(rdpq_paragraph_char_t *chars, int nchars)
{
    for (int i = 1; i < nchars; ++i) {
        rdpq_paragraph_char_t tmp = chars[i];
        int j = i;
        while (j > 0 && char_compare(chars + j - 1, &tmp) > 0) {
            chars[j] = chars[j - 1];
            --j;
        }
        chars[j] = tmp;
    }
}

rdpq_paragraph_t* rdpq_paragraph_builder_end(void)
{
    // Check if we need to terminate the current line (to calculate alignment,
    // bounding box, etc.).
    if (builder.ch_line_start != builder.layout->nchars)
        __rdpq_paragraph_builder_newline(builder.layout->nchars);

    // Update bounding box (vertically)
    float y0 = builder.layout->chars[0].y - builder.font->ascent;
    float y1 = builder.layout->chars[builder.layout->nchars-1].y - builder.font->descent + builder.font->line_gap + 1;

    if (UNLIKELY(builder.parms->height && builder.parms->valign)) {
        float offset = builder.parms->height - (y1 - y0);
        if (builder.parms->valign == VALIGN_CENTER) offset *= 0.5f;
        offset = fm_truncf(offset);

        builder.layout->y0 = offset;
        y0 += offset;
        y1 += offset;
    }

    builder.layout->bbox.y0 = y0;
    builder.layout->bbox.y1 = y1;

    // Sort the chars by font/style/glyph
    if (builder.layout->nchars < 48) {
        // For small sizes, use insertion sort as it's faster
        insertion_sort_char_array(builder.layout->chars, builder.layout->nchars);
    } else {
        qsort(builder.layout->chars, builder.layout->nchars, sizeof(rdpq_paragraph_char_t),
            char_compare);
    }

    // Make sure there is always a terminator.
    assertf(builder.layout->nchars < builder.layout->capacity,
        "paragraph too long (%d/%d chars)", builder.layout->nchars, builder.layout->capacity);
    builder.layout->chars[builder.layout->nchars].sort_key = 0;

    return builder.layout;
}

static uint8_t must_hex_digit(uint8_t ch, bool *error)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    *error = true;
    return 0;
}

rdpq_paragraph_t* __rdpq_paragraph_build(const rdpq_textparms_t *parms, uint8_t initial_font_id, const char *utf8_text, int *nbytes, rdpq_paragraph_t *layout)
{
    rdpq_paragraph_builder_begin(parms, initial_font_id, layout);

    const char *buf = utf8_text;
    const char *end = utf8_text + *nbytes;
    const char *span = buf;

    while (buf < end) {
        if (UNLIKELY(buf[0] == '$')) {
            rdpq_paragraph_builder_span(span, buf - span);
            if (buf[1] == '$') {
                // next span will include the escaped char
                buf += 2; span = buf - 1;                
            } else {
                bool error = false;
                uint8_t font_id = must_hex_digit(buf[1], &error) << 4 | must_hex_digit(buf[2], &error);
                assertf(!error, "invalid font id: %c%c at position %d (font id must be two hex digits)", buf[1], buf[2], buf-utf8_text);
                assertf(font_id > 0, "invalid usage of font ID 0 (reserved)");
                rdpq_paragraph_builder_font(font_id);
                span = buf + 3;
                buf = span;
            }
            if (UNLIKELY(rdpq_paragraph_builder_full()))
                break;
            continue;
        } else if (UNLIKELY(buf[0] == '^')) {
            rdpq_paragraph_builder_span(span, buf - span);
            if (buf[1] == '^') {
                // next span will include the escaped char
                buf += 2; span = buf - 1;                
            } else {
                bool error = false;
                uint8_t style_id = must_hex_digit(buf[1], &error) << 4 | must_hex_digit(buf[2], &error);
                assertf(!error, "invalid style id: %c%c at position %d (font id must be two hex digits)", buf[1], buf[2], buf-utf8_text);
                rdpq_paragraph_builder_style(style_id);
                span = buf + 3;
                buf = span;
            }
            if (UNLIKELY(rdpq_paragraph_builder_full()))
                break;
            continue;
        } else if (UNLIKELY(buf[0] == '\n')) {
            rdpq_paragraph_builder_span(span, buf - span);
            rdpq_paragraph_builder_newline();
            span = buf + 1;
            buf = span;
            if (UNLIKELY(rdpq_paragraph_builder_full()))
                break;
            continue;
        } else {
            ++buf;
        }
    }

    if (buf != span)
        rdpq_paragraph_builder_span(span, buf - span);
    *nbytes = buf - utf8_text;
    return rdpq_paragraph_builder_end();
}

rdpq_paragraph_t* rdpq_paragraph_build(const rdpq_textparms_t *parms, uint8_t initial_font_id, const char *utf8_text, int *nbytes)
{
    return __rdpq_paragraph_build(parms, initial_font_id, utf8_text, nbytes, NULL);
}

void rdpq_paragraph_render(const rdpq_paragraph_t *layout, float x0, float y0)
{
    const rdpq_paragraph_char_t *ch = layout->chars;

    x0 += layout->x0;
    y0 += layout->y0;
    while (ch->font_id != 0) {
        const rdpq_font_t *fnt = rdpq_text_get_font(ch->font_id);
        int n = rdpq_font_render_paragraph(fnt, ch, x0, y0);
        ch += n;
        assert(ch <= layout->chars + layout->nchars);
    }
}

void rdpq_paragraph_free(rdpq_paragraph_t *layout)
{
    #ifndef NDEBUG
    memset(layout, 0, sizeof(*layout));
    #endif
    free(layout);
}

__attribute__((constructor))
void __rdpq_paragraph_char_check_bitfield(void)
{
    // Check that the layout of the bitfield is the one we expect.
    // If this ever changes across GCC versions, we want to detect this: if the
    // sort key isn't made of font_id/atlas_id/style_id in this order, performance
    // will silently decrease a lot.
    rdpq_paragraph_char_t ch = {0};
    ch.font_id = 0xAA;
    ch.atlas_id = 0xBB;
    ch.style_id = 0xCC;
    assert((ch.sort_key & 0xFFFFFF00) == 0xAABBCC00);
}
