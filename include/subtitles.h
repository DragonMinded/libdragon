/**
 * @file subtitles.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Subtitles API via the SUB64 format
 * @ingroup video
 *
 * SUB64 is libdragon-specific format for subtitles, created by the videoconv64
 * tool when converting video files with embedded subtitles, or standalone
 * subtitle files.
 *
 * It allows to store subtitles in a linear stream of events, with each event
 * containing a timestamp and a text string. It has very basic support for
 * styling (bold, italic, and bold+italic), regions (just 3: bottom, top, center),
 * and for multiple subtitles visible at the same time (up to 3: one per region).
 *
 * This library provides a simple API to load and parse SUB64 files. It also
 * offers 3 different backends to visualize subtitles on the screen:
 *
 * - Via rdpq_text.h API to perform hardware-accelerated text rendering. This is
 *   the most efficient way to render subtitles on top of videos. You can provide
 *   your own font files in font64 format. Coloring can be adjusted at runtime,
 *   including foreground and background color and opacity.
 *
 */
#ifndef LIBDRAGON_SUBTITLES_H
#define LIBDRAGON_SUBTITLES_H

#include <stdint.h>
#include <stdbool.h>
#include "graphics.h"

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct subtitles_s subtitles_t;
///@endcond

/** @brief Subtitle cue: a single subtitle visible on screen */
typedef struct subtitle_cue_s {
    /** 
     * @brief The text of the cue, in UTF-8 encoding.
     * 
     * The text might contain markup tags for styling (bold, italic, underline),
     * in rdpq_text format (see rdpq_text.h).
     *
     * It might also contain embedded newlines (\n), though it is recommended
     * to also handle word-wrapping in case the text is too long for a single line.
     */
    const char *text;

    /**
     * @brief The region hint for the cue.
     *
     * This is a hint for the renderer to place the cue in one of the three
     * regions: bottom (0), top (1), center (2).
     */
    int8_t region_hint;

    /**
     * @brief Whether the cue is visible at the current frame.
     *
     * subtitles_get_current_cue() will return cues with a few frames of advance,
     * before they should be shown, in case the renderer needs to perform
     * advance preparation.
     */
    bool visible;
} subtitle_cue_t;

/**
 * @brief Load subtitles from a SUB64 file.
 * 
 * SUB64 is libdragon-specific format for subtitles, created by the videoconv64
 * tool when converting video files with embedded subtitles, or standalone
 * subtitle files.
 *
 *
 * @param fn                Filename to load (including filesystem prefix, eg: "rom:/subtitles.sub64")
 * @return                  Handle to the loaded subtitles
 */
subtitles_t* subtitles_load(const char *fn);

/**
 * @brief Free the subtitles handle.
 *
 * This function frees the subtitles handle and all the resources associated with it.
 *
 * @param sub                 Handle to the subtitles
 */
void subtitles_free(subtitles_t *sub);


/**
 * @brief Advance to the next frame.
 *
 * This function advances to the next frame. It is used to synchronize the subtitles
 * with the video stream. It should be called once per frame.
 *
 * @param sub                 Handle to the subtitles
 */
void subtitles_next_frame(subtitles_t *sub);

/**
 * @brief Get the current cue.
 *
 * This function returns the current cues. It is used to get the current cues
 * to render on the screen.
 *
 * @param sub                 Handle to the subtitles
 * @param cues                Array to store the current cues (if not NULL)
 * @param max_cues            Maximum number of cues to store in the array. Since
 *                            sub64 supports up to 3 subtitles visible at the same time (one per region),
 *                            the max_cues parameter should be 3.
 * @return                    If cues is not NULL, the function will return the
 *                            number of cues stored in the array. Otherwise,
 *                            the function will return the number of cues available.
 */
int subtitles_get_current_cues(subtitles_t *sub, subtitle_cue_t *cues, int max_cues);

/**
 * @brief Seek to a specific frame index.
 *
 * This function seeks to a specific frame index. Seeking is a relatively
 * expensive operation, and should be used only when necessary.
 *
 * @param sub                 Handle to the subtitles
 * @param frame_idx           Frame index to seek to
 */
void subtitles_seek(subtitles_t *sub, int frame_idx);


/**
 * @name Subtitle renderers
 * 
 * These are different subtitle renderers that can be used to render subtitles
 * on the screen. They are all based on the subrenderer_t interface.
 * 
 * @{
 */

/** @brief A subtitle renderer object */
typedef struct subrenderer_s subrenderer_t;


/** @brief Configuration parameters for the RDPQ subtitle renderer */
typedef struct {
    /**
     * @brief Font filenames (normal, bold, italic)
     * 
     * This array must be filled with the filenames of the font64 files to use
     * for rendering subtitles (including filesystem prefix, eg: "rom:/arial.font64").
     * 
     * The first font is the most important one (normal). If not provided, a
     * builtin font will be used.
     * 
     * The other fonts (bold, italic) are optional. If not provided, the normal font
     * will be used for the other styles as well.
     */
    const char *fonts[3];

    /**
     * @brief Background box color+alpha (default: no background)
     * 
     * This color is used to draw a semi-transparent background box behind
     * the subtitles, to improve readability. If the alpha component is 0,
     * no background box is drawn.
     */
    color_t bkg_color;

} subrenderer_rdpq_parms_t;


/**
 * @brief Create a RDPQ-based subtitle renderer.
 * 
 * This renderer uses the rdpq_text API to render subtitles on the screen.
 * It is the most efficient way to render subtitles on top of videos.
 * 
 * @param parms              Parameters for the RDPQ renderer (fonts, background color, etc.)
 * @return                   Handle to the created subtitle renderer
 */
subrenderer_t* subrenderer_create_rdpq(subrenderer_rdpq_parms_t *parms);

/**
 * @brief Create an EIA-608 subtitle renderer.
 */
subrenderer_t* subrenderer_create_eia608(void);

/**
 * @brief Set a custom frame size for the subtitle renderer.
 * 
 * By default, the subtitle renderer assumes a frame size equal to the
 * display size. If the video being played has a different size, this function
 * can be used to set the correct frame size, so that subtitles are rendered
 * in the correct position.
 * 
 * @param base        Subtitle renderer handle
 * @param width       Frame width
 * @param height      Frame height
 */
void subrenderer_set_frame_size(subrenderer_t *base, int width, int height);

/**
 * @brief Render the subtitles for the current frame.
 * 
 * @param renderer      Subtitle renderer handle
 * @param cues          Array of cues to render
 * @param num_cues      Number of cues in the array
 */
void subrenderer_render(subrenderer_t *renderer, subtitle_cue_t *cues, int num_cues);

/**
 * @brief Free the subtitle renderer.
 * 
 * @param renderer      Subtitle renderer handle
 */
void subrenderer_free(subrenderer_t *renderer);

/** @} */


#ifdef __cplusplus
}
#endif

#endif
