#include "rdpq_paragraph.h"
#include "rdpq_text.h"
#include "rdpq_font.h"
#include "rdpq_font_internal.h"
#include "debug.h"
#include <stdlib.h>
#include <assert.h>

#define UNLIKELY(x) __builtin_expect(!!(x), 0)

static struct {
    rdpq_paragraph_t *layout;
    const rdpq_textparms_t *parms;
    const rdpq_font_t *font;
    uint8_t font_id;
    uint8_t style_id;
    float xscale, yscale; // FIXME: this should be a drawing context
    float xline;
    float yline;
    float x, y;
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

void rdpq_paragraph_builder_begin(const rdpq_textparms_t *parms, uint8_t initial_font_id, rdpq_paragraph_t *layout)
{
    static const rdpq_textparms_t empty_parms = {0};
    memset(&builder, 0, sizeof(builder));
    builder.parms = parms ? parms : &empty_parms;
    if (!layout) {
        const int initial_chars = 256;
        layout = malloc(sizeof(rdpq_paragraph_t) + sizeof(rdpq_paragraph_char_t) * initial_chars);
        memset(layout, 0, sizeof(*layout));
        layout->capacity = initial_chars;
    }
    builder.layout = layout;
    builder.font_id = initial_font_id;
    builder.font = rdpq_text_get_font(initial_font_id);
    builder.xscale = 1.0f;
    builder.yscale = 1.0f;
    // start at center of pixel so that all rounds are to nearest
    builder.x = 0.5f + builder.parms->indent;
    builder.y = 0.5f + builder.parms->height ? builder.font->ascent : 0;
    builder.skip_current_line = builder.parms->height && builder.y - builder.font->descent >= builder.parms->height;
    builder.layout->nlines = 1;
}

void rdpq_paragraph_builder_font(uint8_t font_id)
{
    builder.must_sort |= builder.font_id > font_id;
    builder.font_id = font_id;
    builder.font = rdpq_text_get_font(font_id);
    builder.style_id = 0;
}

void rdpq_paragraph_builder_style(uint8_t style_id)
{
    builder.must_sort |= builder.style_id > style_id;
    builder.style_id = style_id;
}

static bool paragraph_wrap(int wrapchar, float *xcur, float *ycur)
{
    builder.x = *xcur;
    builder.y = *ycur;
    rdpq_paragraph_builder_newline();
    if (builder.skip_current_line) {
        builder.layout->nchars = wrapchar;
        return false;
    }

    rdpq_paragraph_char_t *ch = &builder.layout->chars[wrapchar];
    rdpq_paragraph_char_t *end = &builder.layout->chars[builder.layout->nchars];
    end->x = *xcur * 4;
    end->y = *ycur * 4;

    float x = builder.x;
    float y = builder.y;
    while (ch < end) {
        float advance = (ch[1].x - ch[0].x) * 0.25f;
        ch->x = x*4;
        ch->y = y*4;
        x += advance;
        ++ch;
    }
    builder.x = x;

    *xcur = builder.x;
    *ycur = builder.y;
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
    int last_space_index = -1;
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

        float xadvance; int8_t xoff2; bool has_kerning; uint8_t sort_key;
        __rdpq_font_glyph_metrics(fnt, index, &xadvance, &xoff2, &has_kerning, &sort_key);

        if (!is_space) {
            builder.layout->chars[builder.layout->nchars++] = (rdpq_paragraph_char_t) {
                .font_id = builder.font_id,
                .style_id = builder.style_id,
                .sort_key = sort_key,
                .glyph = index,
                .x = xcur*4,
                .y = ycur*4,
            };
        } else {       
            last_space_index = builder.layout->nchars;
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
                    if (last_space_index >= 0) {
                        if (!paragraph_wrap(last_space_index, &xcur, &ycur))
                            return;
                        last_space_index = -1;
                        break;
                    }
                    builder.layout->nchars -= 1;
                    // fallthrough!
                case WRAP_ELLIPSES:
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

void rdpq_paragraph_builder_newline(void)
{
    float line_height = 0.0f;
    
    line_height += builder.font->ascent - builder.font->descent + builder.font->line_gap;
    line_height += builder.parms->line_spacing;
     
    builder.y += line_height * builder.yscale;
    builder.x = 0.5f;
    builder.skip_current_line = builder.parms->height && builder.y - builder.font->descent >= builder.parms->height;
    builder.layout->nlines += 1;
}

static int char_compare(const void *a, const void *b)
{
    const rdpq_paragraph_char_t *ca = a, *cb = b;
    return (ca->fsg & 0xFFFF0000) - (cb->fsg & 0xFFFF0000);
}

void insertion_sort_char_array(rdpq_paragraph_char_t *chars, int nchars)
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

void __rdpq_paragraph_builder_optimize(void)
{
    builder.must_sort = true;
}

rdpq_paragraph_t* rdpq_paragraph_builder_end(void)
{
    // Sort the chars by font/style/glyph
    if (builder.must_sort) {
        if (builder.layout->nchars < 48) {
            insertion_sort_char_array(builder.layout->chars, builder.layout->nchars);
        } else {
            qsort(builder.layout->chars, builder.layout->nchars, sizeof(rdpq_paragraph_char_t),
                char_compare);
        }
    }

    // Make sure there is always a terminator.
    assertf(builder.layout->nchars < builder.layout->capacity,
        "paragraph too long (%d/%d chars)", builder.layout->nchars, builder.layout->capacity);
    builder.layout->chars[builder.layout->nchars].fsg = 0;

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

rdpq_paragraph_t* __rdpq_paragraph_build(const rdpq_textparms_t *parms, uint8_t initial_font_id, const char *utf8_text, int nbytes, rdpq_paragraph_t *layout, bool optimize)
{
    rdpq_paragraph_builder_begin(parms, initial_font_id, layout);

    const char *buf = utf8_text;
    const char *end = utf8_text + nbytes;
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
                assertf(!error, "invalid font id: %c%c at position %d", buf[1], buf[2], buf-utf8_text);
                rdpq_paragraph_builder_font(font_id);
                span = buf + 3;
                buf = span;
            }
            continue;
        } else if (UNLIKELY(buf[0] == '^')) {
            rdpq_paragraph_builder_span(span, buf - span);
            if (buf[1] == '^') {
                // next span will include the escaped char
                buf += 2; span = buf - 1;                
            } else {
                bool error = false;
                rdpq_paragraph_builder_span(span, buf - span);
                uint8_t style_id = must_hex_digit(buf[1], &error) << 4 | must_hex_digit(buf[2], &error);
                assertf(!error, "invalid style id: %c%c at position %d", buf[1], buf[2], buf-utf8_text);
                rdpq_paragraph_builder_style(style_id);
                span = buf + 3;
                buf = span;
            }
            continue;
        } else if (UNLIKELY(buf[0] == '\n')) {
            rdpq_paragraph_builder_span(span, buf - span);
            rdpq_paragraph_builder_newline();
            span = buf + 1;
            buf = span;
            continue;
        } else {
            ++buf;
        }
    }

    if (buf != span)
        rdpq_paragraph_builder_span(span, buf - span);
    if (optimize)
        __rdpq_paragraph_builder_optimize();
    return rdpq_paragraph_builder_end();
}

rdpq_paragraph_t* rdpq_paragraph_build(const rdpq_textparms_t *parms, uint8_t initial_font_id, const char *utf8_text, int nbytes)
{
    return __rdpq_paragraph_build(parms, initial_font_id, utf8_text, nbytes, NULL, true);
}

void rdpq_paragraph_render(const rdpq_paragraph_t *layout, float x0, float y0)
{
    const rdpq_paragraph_char_t *ch = layout->chars;

    while (ch->font_id != 0) {
        const rdpq_font_t *fnt = rdpq_text_get_font(ch->font_id);
        int n = rdpq_font_render_paragraph(fnt, ch, x0, y0);
        ch += n;
        assert(ch <= layout->chars + layout->nchars);
    }
}
