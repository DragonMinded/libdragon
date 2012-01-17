/**
 * @file display.h
 * @brief Display Subsystem
 * @ingroup display
 */
#ifndef __LIBDRAGON_DISPLAY_H
#define __LIBDRAGON_DISPLAY_H

#include <stdint.h>

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
    RESOLUTION_512x480
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

/** @brief Display context */
typedef int display_context_t;

#ifdef __cplusplus
extern "C" {
#endif

void display_init( resolution_t res, bitdepth_t bit, uint32_t num_buffers, gamma_t gamma, antialias_t aa );
display_context_t display_lock();
void display_show(display_context_t disp);
void display_close();

#ifdef __cplusplus
}
#endif

/** @} */ /* display */

#endif
