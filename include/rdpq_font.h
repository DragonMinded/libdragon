#ifndef RDPQ_FONT_H
#define RDPQ_FONT_H

struct rdpq_font_s;
typedef struct rdpq_font_s rdpq_font_t;

rdpq_font_t* rdpq_font_load(const char *fn);
void rdpq_font_free(rdpq_font_t *fnt);

void rdpq_font_begin(color_t color);
void rdpq_font_end(void);

/**
 * @brief Draw a line of text using the specified font.
 * 
 * This is the inner function for text drawing. Most users would probably
 * use either #rdpq_font_print or #rdpq_font_printf, though either of them
 * will call this one.
 * 
 * 
 * 
 * @param fnt 
 * @param text 
 * @param nbytes 
 */
void rdpq_font_printn(rdpq_font_t *fnt, const char *text, int nbytes);

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
