#ifndef RDPQ_FONT_H
#define RDPQ_FONT_H

struct rdpq_font_s;
typedef struct rdpq_font_s rdpq_font_t;

rdpq_font_t* rdpq_font_load(const char *fn);
void rdpq_font_free(rdpq_font_t *fnt);

void rdpq_font_begin(color_t color);
void rdpq_font_position(float x, float y);
void rdpq_font_end(void);


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
inline void rdpq_font_print(rdpq_font_t *fnt, const char *text)
{
    rdpq_font_printn(fnt, text, strlen(text));
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
void rdpq_font_printf(rdpq_font_t *fnt, const char *fmt, ...);

#endif
