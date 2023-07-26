#ifndef LIBDRAGON_RDPQ_FONT_H
#define LIBDRAGON_RDPQ_FONT_H

#include "graphics.h"

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct rdpq_font_s rdpq_font_t;
typedef struct rdpq_paragraph_char_s rdpq_paragraph_char_t;
///@endcond

rdpq_font_t* rdpq_font_load(const char *fn);
rdpq_font_t* rdpq_font_load_buf(void *buf, int sz);
void rdpq_font_free(rdpq_font_t *fnt);

void rdpq_font_begin(color_t color);
void rdpq_font_position(float x, float y);
void rdpq_font_scale(float xscale, float yscale);
void rdpq_font_end(void);

typedef struct rdpq_fontstyle_s {
    color_t color;
} rdpq_fontstyle_t;

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
 * @param y0        Y coordinate of the start of the paragraph (baseline first char)
 * @param x0        X coordinate of the start of the paragraph (baseline first char)
 * @return int      Number of chars rendered
 */
int rdpq_font_render_paragraph(const rdpq_font_t *fnt, const rdpq_paragraph_char_t *chars, float x0, float y0);

#if 0
/** 
 * @brief Print formatting parameters: wrapping modes 
 * 
 * These modes take effect on each line that doesn't fit the width provided
 * in #rdpq_parparms_t. If no width is specified, the text is never wrapped,
 * not even on the border of the screen.
 */
typedef enum {
    WRAP_NONE = 0,         ///< Truncate the text (if any)
//    WRAP_ELLIPSES = 1,     ///< Truncate the text adding ellipsis (if any)
    WRAP_CHAR = 2,         ///< Wrap at character boundaries 
    WRAP_WORD = 3,         ///< Wrap at word boundaries 
} rdpq_printwrap_t;

/** @brief Print formatting parameters */
typedef struct {
    int16_t width;           ///< Maximum horizontal width of the paragraph, in pixels (0 if unbounded)
    int16_t height;          ///< Maximum vertical height of the paragraph, in pixels (0 if unbounded)
    uint8_t align;           ///< Horizontal alignment (0=left, 1=center, 2=right)
    uint8_t valign;          ///< Vertical alignment (0=top, 1=center, 2=bottom)
    int16_t indent;          ///< Indentation of the first line, in pixels
    int16_t char_spacing;    ///< Extra spacing between chars (in addition to glyph width and kerning)
    int16_t line_spacing;    ///< Extra spacing between lines (in addition to font height)
    rdpq_printwrap_t wrap;   ///< Wrap mode
} rdpq_parparms_t;

/**
 * @brief Draw a line of text using the specified font.
 * 
 * This is the inner function for text drawing. Most users would probably
 * use either #rdpq_font_print or #rdpq_font_printf, though both of them
 * will call this one.
 * 
 * @note This function will not respect any zero termination in the input string,
 *       but blindly draw the specified number of bytes. If you are manipulating
 *       zero-terminated strings, use #rdpq_font_print instead.
 * 
 * @param fnt      Font to use to draw the text
 * @param text     Text to draw (in UTF-8)
 * @param nbytes   Length of the text as number of bytes (not characters)
 * 
 * @see #rdpq_font_print
 * @see #rdpq_font_printf
 */
void rdpq_font_printn(rdpq_font_t *fnt, const char *text, int nbytes);

/**
 * @brief Draw a line of text using the specified font.
 * 
 * @param fnt      Font to use to draw the text
 * @param text     Text to draw (in UTF-8), null-terminated
 */
inline void rdpq_font_print(rdpq_font_t *fnt, const char *text, const rdpq_parparms_t *parms)
{
    rdpq_font_printn(fnt, text, strlen(text), parms);
}

/**
 * @brief Draw a formatted line of text using the specified font.
 * 
 * This is similar to #rdpq_font_printn but allows for the handy
 * printf syntax in case some formatting is required.
 * 
 * Note that this function is limited to 256 byte strings for
 * efficiency reasons. If you need to format more, use sprintf
 * yourself and pass the buffer to #rdpq_font_printn.
 * 
 * @see #rdpq_font_printn
 * @see #rdpq_font_print
 */
void rdpq_font_printf(rdpq_font_t *fnt, const rdpq_parparms_t *parms, const char *fmt, ...);
#endif

#ifdef __cplusplus
}
#endif

#endif
