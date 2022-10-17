/**
 * @file display.h
 * @brief Display Subsystem
 * @ingroup display
 */
#ifndef __LIBDRAGON_DISPLAY_H
#define __LIBDRAGON_DISPLAY_H

#include <stdint.h>

///@cond
struct surface_s;
typedef struct surface_s surface_t;
///@endcond

/**
 * @addtogroup display
 * @{
 */

/**
 * @brief Valid video resolutions
 */
typedef enum
{
    /** @brief 320x240 mode */
    RESOLUTION_320x240,
    /** @brief 640x480 mode */
    RESOLUTION_640x480,
    /** @brief 256x240 mode */
    RESOLUTION_256x240,
    /** @brief 512x480 mode */
    RESOLUTION_512x480,
    /** @brief 512x240 mode, high-res progressive */
    RESOLUTION_512x240,
    /** @brief 640x240 mode, high-res progressive */
    RESOLUTION_640x240,
} resolution_t;

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

void display_init( resolution_t res, bitdepth_t bit, uint32_t num_buffers, gamma_t gamma, antialias_t aa );
surface_t* display_lock(void);
void display_show(surface_t* surf);
void display_close();

uint32_t display_get_width(void);
uint32_t display_get_height(void);
uint32_t display_get_bitdepth(void);
uint32_t display_get_num_buffers(void);

#ifdef __cplusplus
}
#endif

/** @} */ /* display */

#endif
