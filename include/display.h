/**
 * @file display.h
 * @brief Display Subsystem
 * @ingroup display
 */
#ifndef __LIBDRAGON_DISPLAY_H
#define __LIBDRAGON_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup display Display Subsystem
 * @ingroup libdragon
 * @brief Video interface system for configuring video output modes and displaying rendered
 *        graphics.
 *
 * The display subsystem handles interfacing with the video interface (VI)
 * and the hardware rasterizer (RDP) to allow software and hardware graphics
 * operations.  It consists of the @ref display, the @ref graphics and the
 * @ref rdp modules.  A separate module, the @ref console, provides a rudimentary
 * console for developers.  Only the display subsystem or the console can be
 * used at the same time.  However, commands to draw console text to the display
 * subsystem are available.
 *
 * The display subsystem module is responsible for initializing the proper video
 * mode for displaying 2D, 3D and software graphics.  To set up video on the N64,
 * code should call #display_init with the appropriate options.  Once the display
 * has been set, a surface can be requested from the display subsystem using
 * #display_get.  To draw to the acquired surface, code should use functions
 * present in the @ref graphics and the @ref rdp modules.  Once drawing to a surface
 * is complete, the rendered graphic can be displayed to the screen using 
 * #display_show.  Once code has finished rendering all graphics, #display_close can 
 * be used to shut down the display subsystem.
 *
 */

///@cond
typedef struct surface_s surface_t;
///@endcond

/**
 * @addtogroup display
 * @{
 */

/** @brief Valid interlace modes */
typedef enum {
    /** @brief Video output is not interlaced */
    INTERLACE_OFF,
    /** @brief Video output is interlaced and buffer is swapped on odd and even fields */
    INTERLACE_HALF,
    /** @brief Video output is interlaced and buffer is swapped only on even fields */
    INTERLACE_FULL,
} interlace_mode_t;

/**
 * @brief Video resolution structure
 *
 * You can either use one of the pre-defined constants
 * (such as #RESOLUTION_320x240) or define a custom resolution.
 */
typedef struct {
    /** @brief Screen width (must be between 2 and 800) */
    int32_t width;
    /** @brief Screen height (must be between 1 and 720) */
    int32_t height;
    /** @brief Interlace mode */
    interlace_mode_t interlaced;
} resolution_t;

///@cond
#define const static const /* fool doxygen to document these static members */
///@endcond
/** @brief 256x240 mode */
const resolution_t RESOLUTION_256x240 = {256, 240, INTERLACE_OFF};
/** @brief 320x240 mode */
const resolution_t RESOLUTION_320x240 = {320, 240, INTERLACE_OFF};
/** @brief 512x240 mode, high-res progressive */
const resolution_t RESOLUTION_512x240 = {512, 240, INTERLACE_OFF};
/** @brief 640x240 mode, high-res progressive */
const resolution_t RESOLUTION_640x240 = {640, 240, INTERLACE_OFF};
/** @brief 512x480 mode, interlaced */
const resolution_t RESOLUTION_512x480 = {512, 480, INTERLACE_HALF};
/** @brief 640x480 mode, interlaced */
const resolution_t RESOLUTION_640x480 = {640, 480, INTERLACE_HALF};
#undef const

/** @brief Valid bit depths */
typedef enum
{
    /** @brief 16 bits per pixel (5-5-5-1) */
    DEPTH_16_BPP,
    /** @brief 32 bits per pixel (8-8-8-8) */
    DEPTH_32_BPP
} bitdepth_t;

/** @brief Valid gamma correction settings */
typedef enum
{
    /** @brief Uncorrected gamma, should be used by default and with assets built by libdragon tools */
    GAMMA_NONE,
    /** @brief Corrected gamma, should be used on a 32-bit framebuffer
     * only when assets have been produced in linear color space and accurate blending is important */
    GAMMA_CORRECT,
    /** @brief Corrected gamma with hardware dithered output */
    GAMMA_CORRECT_DITHER
} gamma_t;

/** @brief Valid display filter options.
 * 
 * Libdragon uses preconfigured options for enabling certain 
 * combinations of Video Interface filters due to a large number of wrong/invalid configurations 
 * with very strict conditions, and to simplify the options for the user.
 * 
 * Like for example antialiasing requiring resampling; dedithering not working with 
 * resampling, unless always fetching; always enabling divot filter under AA etc.
 * 
 * The options below provide all possible configurations that are deemed useful in development. */
typedef enum
{
    /** @brief All display filters are disabled */
    FILTERS_DISABLED,
    /** @brief Resize the output image with a bilinear filter. 
     * In general, VI is in charge of resizing the framebuffer to fit the TV resolution 
     * (which is always NTSC 640x480 or PAL 640x512). 
     * This option enables a bilinear interpolation that can be used during this resize. */
    FILTERS_RESAMPLE,
    /** @brief Reconstruct a 32-bit output from dithered 16-bit framebuffer. */
    FILTERS_DEDITHER,
    /** @brief Resize the output image with a bilinear filter (see #FILTERS_RESAMPLE). 
     * Add a video interface anti-aliasing pass with a divot filter. 
     * To be able to see correct anti-aliased output, this display filter must be enabled,
     * along with anti-aliased rendering of surfaces. */
    FILTERS_RESAMPLE_ANTIALIAS,
    /** @brief Resize the output image with a bilinear filter (see #FILTERS_RESAMPLE). 
     * Add a video interface anti-aliasing pass with a divot filter (see #FILTERS_RESAMPLE_ANTIALIAS).
     * Reconstruct a 32-bit output from dithered 16-bit framebuffer. */
    FILTERS_RESAMPLE_ANTIALIAS_DEDITHER
} filter_options_t;

///@cond
/** 
 * @brief Display anti-aliasing options (DEPRECATED: Use #filter_options_t instead)
 * 
 * @see #filter_options_t
 */
typedef filter_options_t antialias_t;
/** @brief Display no anti-aliasing (DEPRECATED: Use #FILTERS_DISABLED instead)
 * 
 * @see #filter_options_t
 */
#define ANTIALIAS_OFF                   FILTERS_DISABLED
/** @brief Display resampling anti-aliasing (DEPRECATED: Use #FILTERS_RESAMPLE instead)
 * 
 * @see #filter_options_t
 */
#define ANTIALIAS_RESAMPLE              FILTERS_RESAMPLE
/** @brief Display anti-aliasing and resampling with fetch-on-need (DEPRECATED: Use #FILTERS_RESAMPLE_ANTIALIAS instead)
 * 
 * @see #filter_options_t
 */
#define ANTIALIAS_RESAMPLE_FETCH_NEEDED FILTERS_RESAMPLE_ANTIALIAS
/** @brief Display anti-aliasing and resampling with fetch-always (DEPRECATED: Use #FILTERS_RESAMPLE_ANTIALIAS_DEDITHER instead)
 * 
 * @see #filter_options_t
 */
#define ANTIALIAS_RESAMPLE_FETCH_ALWAYS FILTERS_RESAMPLE_ANTIALIAS_DEDITHER

///@endcond

/** 
 * @brief Display context (DEPRECATED: Use #surface_t instead)
 * 
 * @see #surface_t
 */
typedef surface_t* display_context_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the display to a particular resolution and bit depth
 *
 * Initialize video system.  This sets up a double, triple, or multiple
 * buffered drawing surface which can be blitted or rendered to using
 * software or hardware.
 *
 * @param[in] res
 *            The requested resolution. Use either one of the pre-defined
 *            resolution (such as #RESOLUTION_320x240) or define a custom one.
 * @param[in] bit
 *            The requested bit depth (#DEPTH_16_BPP or #DEPTH_32_BPP)
 * @param[in] num_buffers
 *            Number of buffers, usually 2 or 3, but can be more. Triple buffering
 *            is recommended in case the application cannot hold a steady full framerate,
 *            so that slowdowns don't impact too much.
 * @param[in] gamma
 *            The requested gamma setting
 * @param[in] filters
 *            The requested display filtering options, see #filter_options_t
 */
void display_init( resolution_t res, bitdepth_t bit, uint32_t num_buffers, gamma_t gamma, filter_options_t filters );

/**
 * @brief Close the display
 *
 * Close a display and free buffer memory associated with it.
 */
void display_close();

/**
 * @brief Get a display buffer for rendering
 *
 * Grab a surface that is safe for drawing, spin-waiting until one is
 * available.
 * 
 * When you are done drawing on the buffer, use #display_show to schedule
 * the buffer to be displayed on the screen during next vblank.
 * 
 * It is possible to get more than a display buffer at the same time, for
 * instance to begin working on a new frame while the previous one is still
 * being rendered in parallel through RDP. It is important to notice that
 * surfaces will always be shown on the screen in the order they were gotten,
 * irrespective of the order #display_show is called.
 * 
 * @return A valid surface to render to.
 */
surface_t* display_get(void);

/**
 * @brief Try getting a display surface
 * 
 * This is similar to #display_get, but it does not block if no
 * display is available and return NULL instead.
 * 
 * @return A valid surface to render to or NULL if none is available.
 */
surface_t* display_try_get(void);

/**
 * @brief Display a buffer on the screen
 *
 * Display a surface to the screen on the next vblank. 
 * 
 * Notice that this function does not accept any arbitrary surface, but only
 * those returned by #display_get, which are owned by the display module.
 * 
 * @param[in] surf
 *            A surface to show (previously retrieved using #display_get)
 */
void display_show(surface_t* surf);

/**
 * @brief Return a memory surface that can be used as Z-buffer for the current
 *        resolution
 *
 * This function lazily allocates and returns a surface that can be used
 * as Z-buffer for the current resolution. The surface is automatically freed
 * when the display is closed.
 *
 * @return surface_t    The Z-buffer surface
 */
surface_t* display_get_zbuf(void);

/**
 * @brief Get the currently configured width of the display in pixels
 */
uint32_t display_get_width(void);

/**
 * @brief Get the currently configured height of the display in pixels
 */
uint32_t display_get_height(void);

/**
 * @brief Get the currently configured bitdepth of the display (in bytes per pixels)
 */
uint32_t display_get_bitdepth(void);

/**
 * @brief Get the currently configured number of buffers
 */
uint32_t display_get_num_buffers(void);

/**
 * @brief Get the current number of frames per second being rendered
 * 
 * @return float Frames per second
 */
float display_get_fps(void);


/** @cond */
__attribute__((deprecated("use display_get or display_try_get instead")))
static inline surface_t* display_lock(void) {
    return display_try_get();
}
/** @endcond */

#ifdef __cplusplus
}
#endif

/** @} */ /* display */

#endif
