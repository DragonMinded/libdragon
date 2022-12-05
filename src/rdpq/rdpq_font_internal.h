#ifndef __RDPQ_FONT_INTERNAL_H
#define __RDPQ_FONT_INTERNAL_H

#define FONT_MAGIC_V0            0x464E5448 // "FNT0"

typedef struct {
    uint32_t first_codepoint;
    uint32_t num_codepoints;
    uint32_t first_glyph;
} range_t;

typedef struct glyph_s {
    int16_t xadvance;   // scaled by 64
    int8_t xoff, yoff, xoff2, yoff2;
    uint8_t s, t;
    uint8_t natlas;
    uint8_t __padding[7];
} glyph_t;

typedef struct atlas_s {
    uint8_t *buf;
    uint16_t width, height;
    uint8_t fmt;
    uint8_t __padding[3];
} atlas_t;

typedef struct rdpq_font_s {
    uint32_t magic;
    uint32_t num_ranges;
    uint32_t num_glyphs;
    uint32_t num_atlases;
    range_t *ranges;
    glyph_t *glyphs;
    atlas_t *atlases;
} rdpq_font_t;

#endif
