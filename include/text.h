/**
 * @file text.h
 * @brief Text layout engine
 * @ingroup display
 * 
 * Example 1: draw a single text on the screen
 * 
 * @code{.c}
 *      #include <libdragon.h>
 * 
 *      enum {
 *          FONT_ARIAL = 1
 *      } FONTS;
 *      
 *      int main(void) {
 *          dfs_init(DFS_DEFAULT_LOCATION);
 *          display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
 *          rdpq_init();
 *          text_init();
 * 
 *          // Load the font and register it into the text layout engine with ID 1.
 *          rdpq_font_t *font = rdpq_font_load("Arial.font64");  
 *          rdpq_font_register(font, FONT_ARIAL)
 * 
 *          while (1) {
 *              surface_t *fb = display_get();
 *              rdpq_attach_clear();
 *              text_print(NULL, FONT_ARIAL, 20, 20, "Hello, world");
 *              rdpq_detach_show();
 *          }
 *      }
 * @endcode{.c}
 * 
 * Example 2: how to draw a longer text in a paragraph, split in multiple lines
 *            with word-wrapping
 * 
 * @code{.c}
 *          char *text = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
 * 
 *          text_print(&(text_parms_t) {
 *              .width = 200,       // maximum width of the paragraph
 *              .height = 150,      // maximum height of the paragraph
 *              .wrap = WRAP_WORD   // wrap at word boundaries
 *          }, FONT_ARIAL, 20, 20, text);
 * @endcode{.c}
 * 
 * 
 * Example 3: draw the text with a transparent box behind it
 * 
 * @code{.c}
 *          // First, calculate the layout of the text
 *          text_layout_t *layout = text_layout(&(text_parms_t) {
 *             .width = 200,       // maximum width of the paragraph
 *             .height = 150,      // maximum height of the paragraph
 *             .wrap = WRAP_WORD   // wrap at word boundaries
 *          }, FONT_ARIAL, text);
 * 
 *          // Draw the box
 *          const int margin = 10;
 *          const float x0 = 20;
 *          const float y0 = 20;
 * 
 *          rdpq_set_mode_standard();
 *          rdpq_set_fill_color(RGBA32(120, 63, 32, 255));
 *          rdpq_set_fot_color(RGBA32(255, 255, 255, 128));
 *          rdpq_set_blend_mode(RDPQ_BLEND_MULTIPLY_CONST);
 *          rdpq_fill_rectangle(
 *              x0 - margin - layout->bbox[0],
 *              y0 - margin - layout->bbox[1],
 *              x0 + margin + layout->bbox[2],
 *              y0 + margin + layout->bbox[3]
 *          );
 * 
 *          // Render the text
 *          text_layout_render(layout, x0, y0);
 * 
 *          // Free the layout
 *          text_layout_free(layout);
 * @endcode{.c}
 *
 * Example 4: multi-color text
 * 
 * @code{.c}
 * 
 *      rdpq_font_style_color(font, 0, RGBA32(255, 255, 255, 255));
 *      rdpq_font_style_color(font, 1, RGBA32(255, 0, 0, 255));
 *      rdpq_font_style_color(font, 2, RGBA32(0, 255, 0, 255));
 *      rdpq_font_style_color(font, 3, RGBA32(0, 0, 255, 255));
 *      rdpq_font_style_color(font, 4, RGBA32(255, 0, 255, 255));
 * 
 *      text_print(NULL, FONT_ARIAL, 20, 20, "Hello, ^01world^00! ^02This^00 is ^03a^00 ^04test^00.");
 * @endcode{.c}
 * 
 */


#ifndef LIBDRAGON_TEXT_H
#define LIBDRAGON_TEXT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * @brief Print formatting parameters: wrapping modes 
 * 
 * These modes take effect on each line that doesn't fit the width provided
 * in #rdpq_parparms_t. If no width is specified, the text is never wrapped,
 * not even on the border of the screen.
 */
typedef enum {
    WRAP_NONE = 0,         ///< Truncate the text (if any)
    WRAP_ELLIPSES = 1,     ///< Truncate the text adding ellipsis (if any)
    WRAP_CHAR = 2,         ///< Wrap at character boundaries 
    WRAP_WORD = 3,         ///< Wrap at word boundaries 
} text_wrap_t;

/** @brief Print formatting parameters */
typedef struct {
    int16_t width;           ///< Maximum horizontal width of the paragraph, in pixels (0 if unbounded)
    int16_t height;          ///< Maximum vertical height of the paragraph, in pixels (0 if unbounded)
    uint8_t align;           ///< Horizontal alignment (0=left, 1=center, 2=right)
    uint8_t valign;          ///< Vertical alignment (0=top, 1=center, 2=bottom)
    int16_t indent;          ///< Indentation of the first line, in pixels
    int16_t char_spacing;    ///< Extra spacing between chars (in addition to glyph width and kerning)
    int16_t line_spacing;    ///< Extra spacing between lines (in addition to font height)
    text_wrap_t wrap;        ///< Wrap mode
} text_parms_t;

typedef struct {
    void *ctx;               ///< Opaque pointer for callback functions
    int32_t ascent;          ///< Distance from the baseline to the top of the glyph
    int32_t descent;         ///< Distance from the baseline to the bottom of the glyph
    int32_t linegap;         ///< Distance from the bottom of the glyph to the baseline of the next line
    int32_t space_width;     ///< Width of the space character

    /// Get glyph index from an Unicode codepoint. Return -1 if the codepoint is
    /// not supported by the font.
    /// 
    /// If this function is NULL, the codepoint to glyph index mapping is
    /// assumed to be the identity function for codepoints 0-127, and -1 for
    /// all other codepoints.
    ///
    /// @note This text engine does not support graphemes made of multiple
    ///       codepoints. For instance, you can use U+00E9 has a single
    ///       codepoint representing "é", but you can't represent that grapheme
    ///       with the sequence U+0065 U+0301 (e + combining acute accent).
    ///       So in the context of this function, codepoints and graphemes are
    ///       synonyms.
    int16_t (*glyph)(void *ctx, uint32_t codepoint);

    /**
     * @brief Get the size and advance of a glyph.
     * 
     * This function lookups two similar but different properties of each glyph:
     * "rwidth" and "advance". 
     * 
     * "rwidth" (right width) is the width of the glyph on the right of its
     * origin. A glyph might be "centered" on its origin, so its actual width
     * would be the sum of lwidth -- which we don't query -- and rwidth). In
     * other words, assuming to draw the glyph at (x,y), x+rwidth is the
     * first column that does not contain any pixel of the glyph.
     * 
     * "advance" is the number of pixels to advance the cursor after drawing
     * the glyph. Depending on the font style, this is usually larger than
     * rwidth, but in some cases could even be smaller.
     * 
     * If this function is NULL, the rwidth and advance of each glyph is
     * assumed to be the same as #text_font_t::space_width.
     * 
     * @param ctx     Opaque pointer for callback functions
     * @param glyph   Glyph index
     * @param rwidth  Pointer to the variable that will receive the rwidth of the glyph
     * @param advance Pointer to the variable that will receive the advance of the glyph
     */
    void (*width)(void *ctx, int16_t glyph, int16_t *rwidth, int16_t *advance);

    /// Get the kerning adjustment between two glyphs (NULL if no kerning)
    float (*kerning)(void *ctx, int16_t glyph1, int16_t glyph2); 

    /// @brief Render an array of chars at a certain position.
    /// The array is guaranteed to be sorted by font_id+style_id+glyph
    /// Return the number of processed chars (that is, the index of the first
    /// char in another font, if any).
    int (*render)(void *ctx, const text_char_t *chars, int nchars, float x0, float y0);
} text_font_t;

/**
 * @brief Initialize the text engine.
 */
void text_init(void);

/**
 * @brief Register a new font into the text engine.
 * 
 * After this call, the font is available to be used by the text engine
 * for layout and render. If @p font_id is already registered, this function
 * will fail by asserting.
 * 
 * A #text_font_t is a generic "interface" for a font. This text engine
 * doesn't provide itself any font or a way to create and load them. If you
 * have your own font format, you can create a #text_font_t that wraps it
 * by providing the required callbacks and information. 
 * 
 * In libdragon, there is currently only one font implementation: #rdpq_font_t,
 * part of the rdpq graphics library.
 * 
 * @param font_id      Font ID
 * @param font         Font to register
 */
void text_register_font(uint8_t font_id, const text_font_t *font);

/**
 * @brief Lookup a font in the text engine.
 * 
 * @param font_id      Font ID
 * @return Font registered with the specified ID, or NULL if the ID is unused.
 */
const text_font_t* text_get_font(uint8_t font_id);

/**
 * @brief Layout and render a text in a single call.
 * 
 * This function accepts UTF-8 encoded text. It will layout the text according
 * to the parameters provided in #rdpq_parparms_t, and then render it at the
 * specified coordinates. 
 * 
 * The text is layout and rendered using the specified font by default (using
 * its default style 0), but it can contain special escape codes to change the
 * font or its style.
 * 
 * Escape codes are sequences of the form:
 * 
 *    $xx        Select font "xx", where "xx" is the hexadecimal ID of the font
 *               For instance, $04 will switch to font 4. The current style
 *               is reset to 0.
 *    ^xx        Switch to style "xx" of the current font, where "xx" is the
 *               hexadecimal ID of the style. For instance, ^02 will switch to
 *               style 2. A "style" is an font-dependent rendering style, which
 *               can be anything (a color, a faux-italic variant, etc.). It is
 *               up the the font to define what styles are available.
 * 
 * To use a stray "$" or "^" character in the text, you can escape it by
 * repeating them twice: "$$" or "^^".
 * 
 * @param parms         Layout parameters
 * @param font_id       Font ID to use to render the text (at least initially;
 *                      it can modified via escape codes).
 * @param x0            X coordinate where to start rendering the text
 * @param y0            Y coordinate where to start rendering the text
 * @param utf8_text     Text to render, in UTF-8 encoding. Does not need to be
 *                      NULL terminated.
 * @param nbytes        Number of bytes in the text to render
 */
void text_printn(const text_parms_t *parms, uint8_t font_id, float x0, float y0, 
    const char *utf8_text, int nbytes);

/**
 * @brief Layout and render a formatted text in a single call.
 * 
 * This function is similar to #rdpq_font_print, but it accepts a printf-like
 * format string. The format string is expected to be UTF-8 encoded.
 * 
 * @param parms         Layout parameters
 * @param font_id       Font ID to use to render the text (at least initially;
 *                      it can modified via escape codes).
 * @param x0            X coordinate where to start rendering the text
 * @param y0            Y coordinate where to start rendering the text
 * @param utf8_fmt      Format string, in UTF-8 encoding
 */
void text_printf(const text_parms_t *parms, uint8_t font_id, float x0, float y0, 
    const char *utf8_fmt, ...);

/**
 * @brief Layout and render a text in a single call.
 * 
 * This function is similar to #rdpq_font_print, but it accepts a UTF-8 encoded,
 * NULL-terminated string.
 * 
 * @param parms         Layout parameters
 * @param font_id       Font ID to use to render the text (at least initially;
 *                      it can modified via escape codes).
 * @param x0            X coordinate where to start rendering the text
 * @param y0            Y coordinate where to start rendering the text
 * @param utf8_text     Text to render, in UTF-8 encoding, NULL terminated.
 */
void text_print(const text_parms_t *parms, uint8_t font_id, float x0, float y0, 
    const char *utf8_text);


/** @brief A single char in a layout */
typedef struct {
    uint8_t font_id;     ///< Font ID
    uint8_t style_id;    ///< Style ID
    int16_t glyph;       ///< Glyph index
    int16_t x;           ///< X position of the glyph
    int16_t y;           ///< Y position of the glyph
} text_layoutchar_t;

/**
 * @brief Layout of a text.
 * 
 * This structure is returned by #text_layout. It contains information on
 * the layout of the text, that is the position of each glyph to be drawn.
 * It also contains some metrics calculated from the layout engine, such as
 * the bounding box of the text, and the number of lines.
 */
typedef struct {
    float bbox[4];               ///< Bounding box of the text (x0, y0, x1, y1)
    int nlines;                  ///< Number of lines of the text
    int nchars;                  ///< Total number of chars in this layout
    text_layoutchar_t chars[];   ///< Array of chars
} text_layout_t;

/**
 * @brief Calculate the layout of a text using the specified parameters.
 * 
 * This function accepts UTF-8 encoded text. It will layout the text according
 * to the parameters provided in #rdpq_parparms_t, and return an array of
 * #rdpq_char_t that can be used to later render the text via #text_render.
 * 
 * This function is useful if you want to layout a text once, and then draw
 * it multiple times (eg: for multiple frames). Layouting a text isn't
 * necessarily a slow operation (depending on what parameters are used), but
 * it's not free either.
 * 
 * This function is called internally by #text_printn, #text_printf, and #text_print,
 * so it supports the same escape codes that they do, that allow to layout a
 * text using multiple fonts and styles.
 * 
 * @param parms             Layout parameters
 * @param font_id           Font ID to use to render the text (at least initially;
 *                          it can modified via escape codes). See #text_printn
 *                          for more details.
 * @param utf8_text         Text to render, in UTF-8 encoding.
 * @param nbytes            Number of bytes in the text to render
 * @return text_layout_t*   Calculated layout. Free it with #text_free when not needed anymore.
 */
text_layout_t* text_layout(const text_parms_t *parms, uint8_t font_id, 
    const char *utf8_text, int nbytes);

/**
 * @brief Render a text that was laid out by #text_layout.
 * 
 * This function will render the text that was previously layouted by #text_layout.
 * To perform the actual drawing, it will defer to the #text_font_t::render
 * callback of the font(s) the text is using.
 * 
 * @param layout    Layout to render
 * @param x0        X coordinate where to start rendering the text
 * @param y0        Y coordinate where to start rendering the text
 */
void text_layout_render(const text_layout_t *layout, float x0, float y0);

/**
 * @brief Free the memory allocated by #text_layout.
 * 
 * @param layout    Layout to free
 */
void text_layout_free(text_layout_t *layout);


/**
 * @brief Start a multi-step text layout.
 * 
 * This function is a lower-level version of #text_layout. It allows to layout
 * multiple "spans" of texts, using different fonts and styles. This function
 * does not support the special escape codes (as described in #text_printn),
 * but expect the text to be split in "spans", each one using a single font
 * and style that must be specified.
 * 
 * After calling #text_layout_begin, use #text_layout_add_span to add each
 * span of text. Finally, call #text_layout_end to retrieve the final array of
 * chars that can be used to render the text.
 * 
 * @param parms         Layout parameters
 * @param buf           Preallocated array of chars to use to store the layout
 *                      (to save memory allocations). If NULL, the array will
 *                      be allocated dynamically.
 * @param nchars        Number of chars in the preallocated array. If @p buf
 *                      is NULL and this number is non-zero, it will be used
 *                      as a hint of the expected total number of characters
 *                      in the text, to size the memory allocation accordingly.
 */
void text_layout_begin(const text_parms_t *parms, text_char_t *buf, int nchars);

/**
 * @brief Add a span of text to a multi-step layout.
 * 
 * @param font_id       Font ID to use for this span
 * @param style_id      Style ID to use for this span
 * @param utf8_text     Text to add, in UTF-8 encoding
 * @param nbytes        Number of bytes in the text to add
 */
void text_layout_add_span(uint8_t font_id, uint8_t style_id, char *utf8_text, int nbytes);

/**
 * @brief Finalize a multi-step text layout.
 * 
 * @param nchars        Pointer to the variable that will receive the number
 * @return text_char_t* 
 */
text_char_t* text_layout_end(int *nchars);

#ifdef __cplusplus
}
#endif

#endif
