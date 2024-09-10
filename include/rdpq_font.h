/**
 * @file rdpq_font.h
 * @brief Text layout engine: font loading and rendering
 * @ingroup rdpq
 */

#ifndef LIBDRAGON_RDPQ_FONT_H
#define LIBDRAGON_RDPQ_FONT_H

#include "graphics.h"
#include "debug.h"

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct rdpq_font_s rdpq_font_t;
typedef struct rdpq_paragraph_char_s rdpq_paragraph_char_t;
rdpq_font_t* __rdpq_font_load_builtin_1(void);
rdpq_font_t* __rdpq_font_load_builtin_2(void);
///@endcond

/** @brief Metrics of a glyph in the font */
typedef struct {
    float xadvance;         ///< Advance after drawing the glyph
    int8_t x0;              ///< Top-left X coordinate of the bbox of the glyph (relative to the pen position)
    int8_t y0;              ///< Top-left Y coordinate of the bbox of the glyph (relative to the pen position)
    int8_t x1;              ///< Bottom-right *exclusive* X coordinate of the bbox of the glyph (relative to the pen position)
    int8_t y1;              ///< Bottom-right *exclusive* Y coordinate of the bbox of the glyph (relative to the pen position)
} rdpq_font_gmetrics_t;

/**
 * @brief Load a font from a file (.font64 format).
 * 
 * This function loads a font from a file. The file must be in the .font64
 * format, as created by the mkfont tool.
 * 
 * @param fn                Filename to load (including filesystem prefix, eg: "rom:/arial.font64")
 * @return rdpq_font_t*     Loaded font
 */
rdpq_font_t* rdpq_font_load(const char *fn);

/**
 * @brief Load a font from a buffer in memory (.font64 format).
 * 
 * This function is similar to #rdpq_font_load, but it loads the font from
 * a buffer in memory instead of a file. The contents of the buffer must
 * be in the .font64 format, as created by the mkfont tool.
 * 
 * As data is not duplicated, the buffer must stay in memory for the lifetime
 * of the font.
 * 
 * @param buf               Buffer to load
 * @param sz                Size of the buffer
 * @return rdpq_font_t*     Loaded font
 */
rdpq_font_t* rdpq_font_load_buf(void *buf, int sz);

/** 
 * @brief Builtin fonts, shipped with libdragon
 * 
 * All builtin fonts are licensed under CC0 or similar license that effectively
 * place them into public domain, so there are no restrictions on their usage.
 */
typedef enum {
    /// ASCII Debug font, outlined, monospace (8x8 pixels, plus outline)
    /// Monogram by datagoblin (https://datagoblin.itch.io/monogram)
    /// License: CC0
    FONT_BUILTIN_DEBUG_MONO = 1,

    /// ASCII Debug font, outlined, variable width (7x9 pixels, plus outline)
    /// At01 by GrafxKid (https://grafxkid.itch.io/at01)
    /// License: CC0
    FONT_BUILTIN_DEBUG_VAR = 2,
} rdpq_font_builtin_t;

/**
 * @brief Load a builtin font provided by libdragon.
 * 
 * Builtin fonts are simple debug fonts shipped with libdragon itself, to let
 * people quickly write something on the screen without much hassle. They are
 * meant mainly for debug purposes.
 * 
 * See #rdpq_font_builtin_t for a list of available builtin fonts.
 * 
 * @param font              Builtin font to load
 * 
 * @return rdpq_font_t*     Loaded font
 */
inline rdpq_font_t* rdpq_font_load_builtin(rdpq_font_builtin_t font) {
    switch (font) {
        case FONT_BUILTIN_DEBUG_MONO:
            return __rdpq_font_load_builtin_1();
        case FONT_BUILTIN_DEBUG_VAR:
            return __rdpq_font_load_builtin_2();
        default:
            assertf(false, "Invalid builtin font");
            return NULL;
    }
}

/**
 * @brief Free a font.
 * 
 * This function frees a font previously loaded with #rdpq_font_load or
 * #rdpq_font_load_buf (though in the latter case, the buffer must then
 * be freed manually).
 * 
 * @param fnt               Font to free
 */
void rdpq_font_free(rdpq_font_t *fnt);

/**
 * @brief A style for a font
 * 
 * This structure describes a style for a font. It is passed to #rdpq_font_style
 * to create a certain style for a font, that can later be used for rendering.
 */
typedef struct rdpq_fontstyle_s {
    color_t color;                  ///< Color of the text
    color_t outline_color;          ///< Color of the outline (if any)
} rdpq_fontstyle_t;

/**
 * @brief Create a style for a font
 * 
 * This function creates a style for a font. Styles are used to change the
 * visual appearance of a font, for example to change the color of the text.
 * 
 * Each style is identified with a "style ID" (which is a number in range 0..255).
 * After calling this function, the style will be available for rendering.
 * 
 * See #rdpq_text_printn for information on how to specify the style of a text
 * being printed.
 * 
 * @param font              Font to create the style for
 * @param style_id          Style ID to assign to the style
 * @param style             Parameters of the style to create
 */
void rdpq_font_style(rdpq_font_t *font, uint8_t style_id, const rdpq_fontstyle_t *style);

/**
 * @brief Render a certain number of chars from a paragraph.
 * 
 * This function will render a set of characters from a paragraph.
 * It assumes that the paragraph has already been fully built, so the
 * chars must be in strict sorted order (by style and glyph index).
 * 
 * The number of chars is not specified. The function is expected to go
 * through the array rendering all characters with the same font_id as the
 * first one, until it finds a different font_id.
 * 
 * @param fnt       Font to use to render the paragraph
 * @param chars     Array of chars to render
 * @param x0        X coordinate of the start of the paragraph (baseline first char)
 * @param y0        Y coordinate of the start of the paragraph (baseline first char)
 * @return int      Number of chars rendered
 */
int rdpq_font_render_paragraph(const rdpq_font_t *fnt, const rdpq_paragraph_char_t *chars, float x0, float y0);

/**
 * @brief Get information on the Unicode codepoint ranges covered by a font.
 * 
 * Fonts can cover several ranges of Unicode codepoints. This function allows
 * to query the ranges one by one, using the @p idx parameter. There is no
 * minimum or maximum size for a range.
 * 
 * If the @p idx parameter is out of bounds, the function will return false.
 * To iterate over all ranges, start with @p idx = 0 and increment it until
 * the function returns false.
 * 
 * The ranges are guranteed to be sorted in ascending order.
 * 
 * @param[in] fnt       Font to query
 * @param[in] idx       Index of the range to query
 * @param[out] start    Start of the range
 * @param[out] end      End of the range (inclusive)
 * @return true         If the index is valid
 * @return false        If the index is out of bounds
 */
bool rdpq_font_get_glyph_ranges(const rdpq_font_t *fnt, int idx, uint32_t *start, uint32_t *end);

/**
 * @brief Get the metrics of a glyph in a font
 * 
 * @note This is a low-level function for very advanced use cases. Most users
 *       should instead rely to the layout functions provided by rdpq_text.h
 *       and rdpq_paragraph.h
 *
 * @param fnt           Font to query
 * @param codepoint     Unicode codepoint of the glyph
 * @param metrics       Pointer to a structure to store the metrics
 * @return bool         true if the glyph was found, false otherwise
 */
bool rdpq_font_get_glyph_metrics(const rdpq_font_t *fnt, uint32_t codepoint, rdpq_font_gmetrics_t *metrics);

#ifdef __cplusplus
}
#endif

#endif
