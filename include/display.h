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
 * #display_lock.  To draw to the acquired surface, code should use functions
 * present in the @ref graphics and the @ref rdp modules.  Once drawing to a surface
 * is complete, the rendered graphic can be displayed to the screen using 
 * #display_show.  Once code has finished rendering all graphics, #display_close can 
 * be used to shut down the display subsystem.
 *
 */

///@cond
struct surface_s;
typedef struct surface_s surface_t;
///@endcond

/**
 * @addtogroup display
 * @{
 */

/**
 * @brief Video resolution structure
 *
 * You can either use one of the pre-defined constants
 * (such as #RESOLUTION_320x240) or define a custom resolution.
 */
typedef struct {
    /** @brief Screen width (must be between 1 and 800) */
    int32_t width;
    /** @brief Screen height (must be between 1 and 600) */
    int32_t height;
    /** @brief True if interlaced mode enabled */
    bool interlaced;
} resolution_t;

///@cond
#define const static const /* fool doxygen to document these static members */
///@endcond
/** @brief 256x240 mode */
const resolution_t RESOLUTION_256x240 = {256, 240, false};
/** @brief 320x240 mode */
const resolution_t RESOLUTION_320x240 = {320, 240, false};
/** @brief 512x240 mode, high-res progressive */
const resolution_t RESOLUTION_512x240 = {512, 240, false};
/** @brief 640x240 mode, high-res progressive */
const resolution_t RESOLUTION_640x240 = {640, 240, false};
/** @brief 512x480 mode, interlaced */
const resolution_t RESOLUTION_512x480 = {512, 480, true};
/** @brief 640x480 mode, interlaced */
const resolution_t RESOLUTION_640x480 = {640, 480, true};
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
    /** @brief Uncorrected gamma */
    GAMMA_NONE,
    /** @brief Corrected gamma */
    GAMMA_CORRECT,
    /** @brief Corrected gamma with hardware dither */
    GAMMA_CORRECT_DITHER
} gamma_t;

/** @brief Valid antialiasing settings */
typedef enum
{
    /** @brief No anti-aliasing */
    ANTIALIAS_OFF,
    /** @brief Resampling anti-aliasing */
    ANTIALIAS_RESAMPLE,
    /** @brief Anti-aliasing and resampling with fetch-on-need */
    ANTIALIAS_RESAMPLE_FETCH_NEEDED,
    /** @brief Anti-aliasing and resampling with fetch-always */
    ANTIALIAS_RESAMPLE_FETCH_ALWAYS
} antialias_t;

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
 *            The requested resolution. Use eiter one of the pre-defined
 *            resolution (such as #RESOLUTION_320x240) or define a custom one.
 * @param[in] bit
 *            The requested bit depth (#DEPTH_16_BPP or #DEPTH_32_BPP)
 * @param[in] num_buffers
 *            Number of buffers, usually 2 or 3, but can be more. Triple buffering
 *            is recommended in case the application cannot hold a steady full framerate,
 *            so that slowdowns don't impact too much.
 * @param[in] gamma
 *            The requested gamma setting
 * @param[in] aa
 *            The requested anti-aliasing setting
 */
void display_init( resolution_t res, bitdepth_t bit, uint32_t num_buffers, gamma_t gamma, antialias_t aa );

/**
 * @brief Close the display
 *
 * Close a display and free buffer memory associated with it.
 */
void display_close();

/**
 * @brief Lock a display buffer for rendering
 *
 * Grab a surface that is safe for drawing.  If none is available
 * then this will return 0, without blocking. 
 * 
 * When you are done drawing on the buffer, use #display_show to unlock
 * the surface and schedule the buffer to be displayed on the screen during
 * next vblank.
 * 
 * It is possible to lock more than a display buffer at the same time, for
 * instance to begin working on a new frame while the previous one is still
 * being rendered in parallel through RDP. It is important to notice that
 * surfaces will always be shown on the screen in locking order,
 * irrespective of the order #display_show is called.
 *
 * @return A valid surface to render to or NULL if none is available.
 */
surface_t* display_lock(void);

/**
 * @brief Display a previously locked buffer
 *
 * Display a previously-locked surface to the screen on the next vblank. The
 * surface should be locked via #display_lock.
 * 
 * This function does not accept any arbitrary surface, but only those returned
 * by #display_lock.
 * 
 * @param[in] surf
 *            A surface to show (previously retrieved using #display_lock)
 */
void display_show(surface_t* surf);

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

#ifdef __cplusplus
}
#endif

/** @} */ /* display */

#endif
