#ifndef __RDPQ_FONT_INTERNAL_H
#define __RDPQ_FONT_INTERNAL_H

/** @brief font64 file magic header */
#define FONT_MAGIC_V0            0x464E5448 // "FNT0"

/** @brief A range of codepoint (part of #rdpq_font_t) */
typedef struct {
    uint32_t first_codepoint;    ///< First codepoint in the range
    uint32_t num_codepoints;     ///< Number of codepoints in the range
    uint32_t first_glyph;        ///< Index of the first glyph in the range
} range_t;

/** @brief A glyph in the font (part of #rdpq_font_t) */
typedef struct glyph_s {
    int16_t xadvance;                   ///< Number of pixels to advance the cursor after drawing the glyph (scaled by 64)
    int8_t xoff;                        ///< Offset of the x0 coordinate of the glyph from the cursor
    int8_t yoff;                        ///< Offset of the y0 coordinate of the glyph from the cursor
    int8_t xoff2;                       ///< Offset of the x1 coordinate of the glyph from the cursor
    int8_t yoff2;                       ///< Offset of the y1 coordinate of the glyph from the cursor
    uint8_t s;                          ///< S texture coordinate of the glyph in the atlas
    uint8_t t;                          ///< T texture coordinate of the glyph in the atlas
    uint8_t natlas;                     ///< Index of atlas that contains this glyph
    uint8_t __padding[3];               ///< Padding
    uint16_t kerning_lo;                ///< Index of the first kerning pair for this glyph
    uint16_t kerning_hi;                ///< Index of the last kerning pair for this glyph
} glyph_t;

/** @brief A texture atlas (part of #rdpq_font_t) */
typedef struct atlas_s {
    uint8_t *buf;                       ///< Texture buffer
    uint16_t width;                     ///< Texture width
    uint16_t height;                    ///< Texture height
    uint8_t fmt;                        ///< Texture format (see #tex_format_t)
    uint8_t __padding[3];               ///< Padding
} atlas_t;

typedef struct kerning_s {
    int16_t glyph2;                     ///< Index of second glyph
    int8_t kerning;                     ///< Signed number of pixels to advance after drawing the glyph (scaled by 127 / point_size)
} __attribute__((packed)) kerning_t;

/** @brief A font64 file containing a font */
typedef struct rdpq_font_s {
    uint32_t magic;                     ///< Magic header (FONT_MAGIC_V0)
    uint32_t point_size;                ///< Point size of the font
    uint32_t num_ranges;                ///< Number of ranges in the font
    uint32_t num_glyphs;                ///< Number of glyphs in the font
    uint32_t num_atlases;               ///< Number of atlases in the font
    uint32_t num_kerning;               ///< Number of kerning pairs in the font
    range_t *ranges;                    ///< Array of ranges
    glyph_t *glyphs;                    ///< Array of glyphs
    atlas_t *atlases;                   ///< Array of atlases
    kerning_t *kerning;                 ///< Array of kerning pairs
} rdpq_font_t;

#endif
