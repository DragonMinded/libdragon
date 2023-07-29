#include "rdpq_text.h"
#include "rdpq_font.h"
#include "rdpq_paragraph.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define UNLIKELY(x) __builtin_expect(!!(x), 0)

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

int rdpq_text_printn(const rdpq_textparms_t *parms, uint8_t initial_font_id, float x0, float y0, 
    const char *utf8_text, int nbytes)
{
    rdpq_paragraph_t *layout = alloca(sizeof(rdpq_paragraph_t) + sizeof(rdpq_paragraph_char_t) * (nbytes+1));
    memset(layout, 0, sizeof(*layout));
    layout->capacity = nbytes+1;

    layout = __rdpq_paragraph_build(parms, initial_font_id, utf8_text, &nbytes, layout, false);
    rdpq_paragraph_render(layout, x0, y0);
    return nbytes;
}

int rdpq_text_printf(const rdpq_textparms_t *parms, uint8_t font_id, float x0, float y0, 
    const char *utf8_fmt, ...)
{
    char buf[512];
    size_t n = sizeof(buf);

    va_list va;
    va_start(va, utf8_fmt);
    char *buf2 = vasnprintf(buf, &n, utf8_fmt, va);
    va_end(va);

    int nbytes = rdpq_text_printn(parms, font_id, x0, y0, buf2, n);
    if (buf2 != buf) free(buf2);
    return nbytes;
}

/* Extern inline declarations */
extern inline int rdpq_text_print(const rdpq_textparms_t *parms, uint8_t font_id, float x0, float y0, const char *utf8_text);
