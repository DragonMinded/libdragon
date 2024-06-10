/**
 * @file rdpq_paragraph.h
 * @brief Text paragraph layout engine
 * @ingroup rdpq
 */

#ifndef LIBDRAGON_RDPQ_PARAGRAPH_H
#define LIBDRAGON_RDPQ_PARAGRAPH_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

///@cond
struct rdpq_textparms_s;
typedef struct rdpq_textparms_s rdpq_textparms_t;
///@endcond

/** @brief A single char in a layout */
typedef struct __attribute__((packed)) rdpq_paragraph_char_s {
    union {
        struct __attribute__((packed)) {
            uint8_t font_id   : 8;     ///< Font ID
            uint8_t atlas_id  : 8;     ///< Atlas ID
            uint8_t style_id  : 8;     ///< Style ID
            int x             : 12;    ///< Y position
            int y             : 12;    ///< X position
            int16_t glyph     : 16;    ///< Glyph index
        };
        uint32_t sort_key;
    };
} rdpq_paragraph_char_t;

_Static_assert(sizeof(rdpq_paragraph_char_t) == 8, "rdpq_paragraph_char_t is not packed");

/** @brief Bitmask flags for #rdpq_paragraph_t */
enum rdpq_paragraph_flag_e {
    /// @brief Draw a transparent background rectangle to avoid AA artifacts
    /// When drawing text on a 3D background using anti-aliasing (also enabled
    /// in #display_init), the text might interact with the AA filter performed
    /// by the VI and causes artifacts such as smearing. To avoid this, this flag
    /// tells #rdpq_paragraph_render to draw a transparent rectangle behind the
    /// text.
    /// This flag is set by default when using #rdpq_text_printn, #rdpq_text_printf,
    /// and #rdpq_text_print. It can be disabled by setting #rdpq_textparms_t::disable_aa_fix
    /// while rendering.
    RDPQ_PARAGRAPH_FLAG_ANTIALIAS_FIX = (1 << 0),
};

/**
 * @brief A paragraph of text, fully laid out
 * 
 * A paragraph contains information the layout of the text. In addition to
 * some general metrics like the bounding box, the number of lines and the number
 * of chars, it contains an array with all the characters to print, each one
 * with its relative position.
 * 
 * To layout a text, use #rdpq_paragraph_build (or the lower level paragraph
 * builder, via #rdpq_paragraph_builder_begin / #rdpq_paragraph_builder_end).
 * To render it, use #rdpq_paragraph_render. To free it, use #rdpq_paragraph_free.
 */
typedef struct {
    struct {
        float x0;                    ///< Top-left corner (X coord) of the bounding box, relative to drawing position
        float y0;                    ///< Top-left corner (Y coord) of the bounding box, relative to drawing position
        float x1;                    ///< Bottom-right corner (X coord) of the bounding box, relative to drawing position
        float y1;                    ///< Bottom-right corner (Y coord) of the bounding box, relative to drawing position
    } bbox;                          ///< Bounding box of the text, relative to the drawing position
    int nlines;                      ///< Number of lines of the text
    int nchars;                      ///< Total number of chars in this layout
    int capacity;                    ///< Capacity of the chars array
    float x0, y0;                    ///< Alignment offset of the text
    int flags;                       ///< Flags (see #rdpq_paragraph_flag_e)
    rdpq_paragraph_char_t chars[];   ///< Array of chars
} rdpq_paragraph_t;

/**
 * @brief Calculate the layout of a text using the specified parameters.
 * 
 * This function accepts UTF-8 encoded text. It will layout the text according
 * to the parameters provided in #rdpq_textparms_t, and return a new instance of
 * #rdpq_paragraph_t that can be used to later render the text via #rdpq_paragraph_render.
 * 
 * This function is useful if you want to layout a text once, and then draw
 * it multiple times (eg: for multiple frames). Layouting a text isn't
 * necessarily a slow operation (depending on what parameters are used), but
 * it's not free either.
 * 
 * This function is called internally by #rdpq_text_printn, #rdpq_text_printf, and #rdpq_text_print,
 * so it supports the same escape codes that they do, that allow to layout a
 * text using multiple fonts and styles.
 * 
 * The @p nbytes parameter is used to specify the number of bytes to layout.
 * It is then modified to provide the number of bytes actually consumed in the
 * input. The consumed bytes can be less than the input when the text is
 * truncated vertically (which requires the height to be specified in the @p parms
 * structure), which is useful to implement a pagination system. Notice that
 * horizontal truncation (as obtained using #WRAP_NONE
 * or #WRAP_ELLIPSES) still result in the whole line being consumed (as in
 * a paragraph, multiple lines could be truncated and thus shown only partially).
 * 
 * @param parms             Layout parameters
 * @param initial_font_id   Font ID to use to render the text (at least initially;
 *                          it can modified via escape codes). See #rdpq_text_printn
 *                          for more details.
 * @param utf8_text         Text to render, in UTF-8 encoding.
 * @param nbytes            Number of bytes in the text to render. On return, the number
 *                          of bytes consumed in the input.
 * @return                  Calculated layout. Free it with #rdpq_paragraph_free when not needed anymore.
 */
rdpq_paragraph_t* rdpq_paragraph_build(const rdpq_textparms_t *parms, uint8_t initial_font_id, 
    const char *utf8_text, int *nbytes);

/**
 * @brief Render a text that was laid out by #rdpq_paragraph_build.
 * 
 * This function will render the text that was previously layouted by #rdpq_paragraph_build,
 * or via the paragraph builder (#rdpq_paragraph_builder_begin / #rdpq_paragraph_builder_end).
 * To perform the actual drawing, it will defer to the #rdpq_font_render_paragraph.
 * callback of the font(s) the text is using.
 * 
 * @param layout    Layout to render
 * @param x0        X coordinate where to start rendering the text
 * @param y0        Y coordinate where to start rendering the text
 */
void rdpq_paragraph_render(const rdpq_paragraph_t *layout, float x0, float y0);

/**
 * @brief Free the memory allocated by #rdpq_paragraph_build or #rdpq_paragraph_builder_end
 * 
 * @param layout    Paragraph to free
 */
void rdpq_paragraph_free(rdpq_paragraph_t *layout);

/**
 * @name Paragraph builder
 * 
 * These functions are the lower-level API to create a paragraph by combining
 * multiple spans of texts. It is normally not required to use them directly,
 * unless you need to layout a paragraph of text using special rules that you
 * can devise from some special form of hypertext markers, or via other means.
 * 
 * \{
 */

/**
 * @brief Start a paragraph builder.
 * 
 * This function is a lower-level version of #rdpq_paragraph_build. It allows to layout
 * multiple "spans" of texts, using different fonts and styles. This function
 * does not support the special escape codes (as described in #rdpq_text_printn),
 * but expect the text to be split in "spans", each one using a single font
 * and style that must be specified.
 * 
 * After calling #rdpq_paragraph_builder_begin, use #rdpq_paragraph_builder_span to add each
 * span of text, and #rdpq_paragraph_builder_font or #rdpq_paragraph_builder_style
 * to change respectively font and style. It is also required to call 
 * #rdpq_paragraph_builder_newline to start a new line: the paragraph builder
 * does not otherwise support newlines in the text.
 * 
 * Finally, call #rdpq_paragraph_builder_end to retrieve the instance of
 * #rdpq_paragraph_t that contains the layout of the text.
 * 
 * @param parms             Layout parameters
 * @param initial_font_id   Font ID to use to render the text (at least initially;
 * @param layout            Preallocated layout to reuse from scratch. If NULL, the
 *                          array will be allocated dynamically.
 * 
 * @see #rdpq_paragraph_build
 * @see #rdpq_paragraph_builder_span
 * @see #rdpq_paragraph_builder_font
 * @see #rdpq_paragraph_builder_style
 * @see #rdpq_paragraph_builder_newline
 * @see #rdpq_paragraph_builder_end
 */
void rdpq_paragraph_builder_begin(const rdpq_textparms_t *parms, uint8_t initial_font_id, rdpq_paragraph_t *layout);

/**
 * @brief Change the current font
 * 
 * Set the current font in the paragraph, that will be used for spans added
 * after this call. Notice that after a font change, the current style is
 * always reset to 0.
 * 
 * @param font_id       New font ID
 */
void rdpq_paragraph_builder_font(uint8_t font_id);

/**
 * @brief Change the current style
 * 
 * Set the current font style in the paragraph, that will be used for spans
 * added after this call.
 * 
 * @param style_id       New style ID
 */
void rdpq_paragraph_builder_style(uint8_t style_id);

/**
 * @brief Add a span of text
 * 
 * This function adds a span of text to the paragraph. The text will use
 * the current font and style. You can call this function multiple times to
 * append multiple spans of text to the paragraph, though it is better to
 * batch calls as much as reasonably possible, at least for text using
 * the same font and style.
 * 
 * @note This function does not support newlines. Use #rdpq_paragraph_builder_newline
 *       to start a new line.
 * 
 * @param utf8_text     Text to add, in UTF-8 encoding
 * @param nbytes        Number of bytes in the text to add
 */
void rdpq_paragraph_builder_span(const char *utf8_text, int nbytes);

/**
 * @brief Start a new line
 * 
 * This function is required to start a new line in the paragraph. Notice
 * that #rdpq_paragraph_builder_span does not support newlines, so it is
 * necessary to call this function any time a newline is required.
 */
void rdpq_paragraph_builder_newline(void);

/**
 * @brief Finalize the paragraph builder and returns the paragraph
 * 
 * After calling this function, the paragraph is ready to use. Call
 * #rdpq_paragraph_render to render it (even multiple times), and 
 * #rdpq_paragraph_free to free it when you don't need it anymore
 * 
 * @return The generated paragraph
 * 
 * @see #rdpq_paragraph_render
 * @see #rdpq_paragraph_free
 */
rdpq_paragraph_t* rdpq_paragraph_builder_end(void);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif
