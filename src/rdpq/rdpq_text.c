#include "rdpq_text.h"
#include "rdpq_font.h"
#include "rdpq_paragraph.h"
#include "debug.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static const rdpq_font_t *fonts[256];

void rdpq_text_register_font(uint8_t font_id, const rdpq_font_t *font)
{
    assertf(font_id != 0, "font id 0 is reserved");
    assertf(!fonts[font_id], "font 0x%02x already registered", font_id);
    fonts[font_id] = font;
}

const rdpq_font_t* rdpq_text_get_font(uint8_t font_id) {
    return fonts[font_id];
}

extern rdpq_paragraph_t* __rdpq_paragraph_build(const rdpq_textparms_t *parms, uint8_t initial_font_id, const char *utf8_text, int *nbytes, rdpq_paragraph_t *layout, bool optimize);

rdpq_textmetrics_t rdpq_text_printn(const rdpq_textparms_t *parms, uint8_t initial_font_id, float x0, float y0, 
    const char *utf8_text, int nbytes)
{
    rdpq_paragraph_t *layout = alloca(sizeof(rdpq_paragraph_t) + sizeof(rdpq_paragraph_char_t) * (nbytes+1));
    memset(layout, 0, sizeof(*layout));
    layout->capacity = nbytes+1;

    layout = __rdpq_paragraph_build(parms, initial_font_id, utf8_text, &nbytes, layout, false);
    rdpq_paragraph_render(layout, x0, y0);
    return (rdpq_textmetrics_t){ 
        .advance_x = layout->advance_x,
        .advance_y = layout->advance_y,
        .nlines = layout->nlines,
        .utf8_text_advance = nbytes
    };
}

rdpq_textmetrics_t rdpq_text_vprintf(const rdpq_textparms_t *parms, uint8_t font_id, float x0, float y0, 
    const char *utf8_fmt, va_list va)
{
    char buf[512];
    size_t n = sizeof(buf);
    char *buf2 = vasnprintf(buf, &n, utf8_fmt, va);

    if (LIKELY(buf == buf2))
        return rdpq_text_printn(parms, font_id, x0, y0, buf2, n);

    rdpq_textmetrics_t m = rdpq_text_printn(parms, font_id, x0, y0, buf2, n);
    free(buf2);
    return m;
}

rdpq_textmetrics_t rdpq_text_printf(const rdpq_textparms_t *parms, uint8_t font_id, float x0, float y0, 
    const char *utf8_fmt, ...)
{
    va_list va;
    va_start(va, utf8_fmt);
    // NOTE: we don't call va_end here. This is a theoretical violation of the C
    // standard but it does nothing in GCC MIPS, and still it prevents RVO.
    return rdpq_text_vprintf(parms, font_id, x0, y0, utf8_fmt, va);
}

/* Extern inline declarations */
extern inline rdpq_textmetrics_t rdpq_text_print(const rdpq_textparms_t *parms, uint8_t font_id, float x0, float y0, const char *utf8_text);
