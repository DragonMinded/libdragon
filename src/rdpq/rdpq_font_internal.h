#ifndef __RDPQ_FONT_INTERNAL_H
#define __RDPQ_FONT_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include "../../include/graphics.h"

typedef struct rspq_block_s rspq_block_t;

/** @brief font64 file magic header */
#define FONT_MAGIC              "FNT"

/** @brief font64 loaded font buffer magic */
#define FONT_MAGIC_LOADED       "FNL"

/** @brief font64 owned font buffer magic */
#define FONT_MAGIC_OWNED        "FNO"

#define FONT_FLAG_TYPE_MASK     0x0000000F  ///< Mask for the font type

typedef enum {
    FONT_TYPE_ALIASED         = 0,      ///< Aliased font (I4)
    FONT_TYPE_MONO            = 1,      ///< Mono font (CI4, which are 4 1bpp layers)
    FONT_TYPE_MONO_OUTLINE    = 2,      ///< Mono font with outline (CI4, which are 2 2bpp layers)
    FONT_TYPE_ALIASED_OUTLINE = 3,      ///< Aliased font with outline (IA8)
    FONT_TYPE_BITMAP          = 4,      ///< Bitmap font (RGBA32/RGBA16/CI4/CI8)
} fonttype_t;

/** @brief A range of codepoint (part of #rdpq_font_t) */
typedef struct {
    uint32_t first_codepoint;    ///< First codepoint in the range
    uint32_t num_codepoints;     ///< Number of codepoints in the range
    int32_t first_glyph;         ///< Index of the first glyph in the range (-1 if range is sparse)
} range_t;

/** 
 * @brief Sparse range table. 
 * 
 * This is a perfect hash table used to encode glyph indices for codepoints.
 * It is used for all sparse ranges, that is ranges where only some codepoints
 * are defined within the min/max of the range, to avoid wasting too much
 * memory in the glyph table for empty entries.
 * 
 * This uses the CHD (compress hash displace) perfect hash table algorithm,
 * that is very fast at runtime and has very little complexity. Since we
 * handle small tables by today's standard, this algorithm is a good fit.
 */
typedef struct {
    uint32_t seed;                      ///< Seed for the hashing algorithm
    uint32_t r;                         ///< Number of elements in the displacement table
    uint32_t m;                         ///< Number of elements in the values table
    uint16_t *g;                        ///< Displacement table
    int16_t *values;                    ///< Values table (actual glyph indices)
} sparse_range_t;

/** @brief A glyph in the font (part of #rdpq_font_t) */
typedef struct glyph_s {
    uint8_t xadvance;                   ///< Number of pixels to advance the cursor after drawing the glyph
    int8_t xoff;                        ///< Offset of the x0 coordinate of the glyph from the cursor
    int8_t yoff;                        ///< Offset of the y0 coordinate of the glyph from the cursor
    int8_t xoff2;                       ///< Offset of the *exclusive* x1 coordinate of the glyph from the cursor
    int8_t yoff2;                       ///< Offset of the *exclusive* y1 coordinate of the glyph from the cursor
    uint8_t s;                          ///< S texture coordinate of the glyph in the atlas
    uint8_t t;                          ///< T texture coordinate of the glyph in the atlas
    uint8_t natlas : 6;                 ///< Index of atlas that contains this glyph
    uint8_t ntile : 2;                  ///< Tile to use to draw the glyph (for multi-layer atlases)
} glyph_t;

/** @brief For each glyph, range of kerning pairs in the kerning table */
typedef struct glyph_krange_s {
    uint16_t kerning_lo;                ///< Index of the first kerning pair for this glyph
    uint16_t kerning_hi;                ///< Index of the last kerning pair for this glyph
} glyph_krange_t;

/** @brief A texture atlas (part of #rdpq_font_t) */
typedef struct atlas_s {
    sprite_t *sprite;                   ///< Texture sprite
    uint32_t size;                      ///< Size of the sprite in bytes
    rspq_block_t *up;                   ///< RSPQ block that uploads the sprite
} atlas_t;

/** @brief Kerning data for a pair of glyphs. */
typedef struct kerning_s {
    int16_t glyph2;                     ///< Index of second glyph
    int8_t kerning;                     ///< Signed number of pixels to advance after drawing the glyph (scaled by 127 / point_size)
} __attribute__((packed)) kerning_t;

/** @brief Data related to font styling */
typedef struct style_s {
    color_t color;                      ///< Color of the text
    color_t outline_color;              ///< Color of the outline (if any)
} style_t;

/** @brief A font64 file containing a font */
typedef struct rdpq_font_s {
    char magic[3];                      ///< Magic header (FONT_MAGIC)
    uint8_t version;                    ///< Version number (1)
    uint32_t flags;                     ///< Flags
    uint32_t point_size;                ///< Point size of the font
    int32_t ascent;                     ///< Ascent (number of pixels above baseline)
    int32_t descent;                    ///< Descent (number of pixels below baseline)
    int32_t line_gap;                   ///< Line gap of the font (spacing between descent and ascent)
    int32_t space_width;                ///< Width of the space character
    int16_t ellipsis_width;             ///< Width of the ellipsis character
    uint16_t ellipsis_glyph;            ///< Index of the ellipsis glyph
    uint16_t ellipsis_reps;             ///< Number of ellipsis glyphs to draw
    uint16_t ellipsis_advance;          ///< Advance of the ellipsis character
    uint32_t num_ranges;                ///< Number of ranges in the font
    uint32_t num_glyphs;                ///< Number of glyphs in the font
    uint32_t num_atlases;               ///< Number of atlases in the font
    uint32_t num_kerning;               ///< Number of kerning pairs in the font
    uint32_t num_styles;                ///< Number of styles in the font
    style_t builtin_style;              ///< Default style for the font
    range_t *ranges;                    ///< Array of ranges
    sparse_range_t *sparse_range;       ///< Sparse table of glyph indices
    glyph_t *glyphs;                    ///< Array of glyphs
    glyph_krange_t *glyphs_kranges;     ///< Array of glyph kerning ranges
    atlas_t *atlases;                   ///< Array of atlases
    kerning_t *kerning;                 ///< Array of kerning pairs
    style_t *styles;                    ///< Array of styles
} rdpq_font_t;

/**
 * @brief Look up a glyph in a font
 * 
 * @param font          Font to look up the glyph in
 * @param codepoint     Unicode codepoint of the glyph
 * @return int16_t      Index of the glyph in the font, or -1 if not found
 */
int16_t __rdpq_font_glyph(const rdpq_font_t *font, uint32_t codepoint);

inline void __rdpq_font_glyph_metrics(const rdpq_font_t *fnt, int16_t index, float *xadvance, int8_t *xoff, int8_t *xoff2, bool *has_kerning, uint8_t *atlas_id)
{
    glyph_t *g = &fnt->glyphs[index];
    if (xadvance) *xadvance = g->xadvance;
    if (xoff) *xoff = g->xoff;
    if (xoff2) *xoff2 = g->xoff2;
    if (has_kerning) *has_kerning = fnt->glyphs_kranges && fnt->glyphs_kranges[index].kerning_lo != 0;
    if (atlas_id) *atlas_id = g->natlas;
}

float __rdpq_font_kerning(const rdpq_font_t *font, int16_t glyph1, int16_t glyph2);

#endif
