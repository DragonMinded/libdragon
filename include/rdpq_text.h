/**
 * @file rdpq_text.h
 * @brief Text layout engine
 * @ingroup rdpq
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
 * 
 *          // Load the font and register it into the text layout engine with ID 1.
 *          rdpq_text_register_font(FONT_ARIAL, rdpq_font_load("Arial.font64"));
 * 
 *          while (1) {
 *              surface_t *fb = display_get();
 *              rdpq_attach_clear();
 *              rdpq_text_print(NULL, FONT_ARIAL, 20, 20, "Hello, world");
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
 *          rdpq_text_print(&(text_parms_t) {
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
 *          rdpq_paragraph_t *layout = rdpq_text_layout(&(text_parms_t) {
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
 *          rdpq_set_fog_color(RGBA32(255, 255, 255, 128));
 *          rdpq_mode_blender(RDPQ_BLEND_MULTIPLY_CONST);
 *          rdpq_fill_rectangle(
 *              x0 - margin - layout->bbox[0],
 *              y0 - margin - layout->bbox[1],
 *              x0 + margin + layout->bbox[2],
 *              y0 + margin + layout->bbox[3]
 *          );
 * 
 *          // Render the text
 *          rdpq_text_layout_render(layout, x0, y0);
 * 
 *          // Free the layout
 *          rdpq_text_layout_free(layout);
 * @endcode{.c}
 *
 * Example 4: multi-color text
 * 
 * @code{.c}
 * 
 *      rdpq_font_style(font, 0, (rdpq_fontstyle_t){ 
 *          .color = .RGBA32(255, 255, 255, 255),
 *      });
 *      rdpq_font_style(font, 1, (rdpq_fontstyle_t){ 
 *          .color = .RGBA32(255, 0, 0, 255),
 *      });
 *      rdpq_font_style(font, 2, (rdpq_fontstyle_t){ 
 *          .color = .RGBA32(0, 255, 0, 255),
 *      });
 *      rdpq_font_style(font, 3, (rdpq_fontstyle_t){ 
 *          .color = .RGBA32(0, 0, 255, 255),
 *      });
 *      rdpq_font_style(font, 4, (rdpq_fontstyle_t){ 
 *          .color = .RGBA32(255, 0, 255, 255),
 *      });
 * 
 *      rdpq_text_print(NULL, FONT_ARIAL, 20, 20, 
 *          "Hello, ^01world^00! ^02This^00 is ^03a^00 ^04test^00.");
 * @endcode{.c}
 * 
 */


#ifndef LIBDRAGON_RDPQ_TEXT_H
#define LIBDRAGON_RDPQ_TEXT_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rdpq_font_s;
typedef struct rdpq_font_s rdpq_font_t;

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
} rdpq_textwrap_t;

/**
 * @brief Print formatting parameters: horizontal alignment
 */
typedef enum {
    ALIGN_LEFT = 0,         ///< Left alignment
    ALIGN_CENTER = 1,       ///< Center alignment
    ALIGN_RIGHT = 2,        ///< Right alignment
} rdpq_align_t;

/**
 * @brief Print formatting parameters: horizontal alignment
 */
typedef enum {
    VALIGN_TOP = 0,         ///< Top alignment
    VALIGN_CENTER = 1,      ///< Center alignment
    VALIGN_BOTTOM = 2,      ///< Vertical alignment
} rdpq_valign_t;

/** @brief Print formatting parameters */
typedef struct rdpq_textparms_s {
    int16_t width;           ///< Maximum horizontal width of the paragraph, in pixels (0 if unbounded)
    int16_t height;          ///< Maximum vertical height of the paragraph, in pixels (0 if unbounded)
    rdpq_align_t align;      ///< Horizontal alignment (0=left, 1=center, 2=right)
    rdpq_valign_t valign;    ///< Vertical alignment (0=top, 1=center, 2=bottom)
    int16_t indent;          ///< Indentation of the first line, in pixels (only valid for left alignment)
    int16_t char_spacing;    ///< Extra spacing between chars (in addition to glyph width and kerning)
    int16_t line_spacing;    ///< Extra spacing between lines (in addition to font height)
    rdpq_textwrap_t wrap;    ///< Wrap mode
} rdpq_textparms_t;


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
void rdpq_text_register_font(uint8_t font_id, const rdpq_font_t *font);

/**
 * @brief Get a registered font by its ID.
 * 
 * @param font_id      Font ID
 * @return const rdpq_font_t*   Registered font or NULL
 */
const rdpq_font_t *rdpq_text_get_font(uint8_t font_id);

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
void rdpq_text_printn(const rdpq_textparms_t *parms, uint8_t font_id, float x0, float y0, 
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
__attribute__((format(printf, 5, 6)))
void rdpq_text_printf(const rdpq_textparms_t *parms, uint8_t font_id, float x0, float y0, 
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
inline void rdpq_text_print(const rdpq_textparms_t *parms, uint8_t font_id, float x0, float y0, 
    const char *utf8_text)
{
    return rdpq_text_printn(parms, font_id, x0, y0, utf8_text, strlen(utf8_text));
}

#ifdef __cplusplus
}
#endif

#endif
