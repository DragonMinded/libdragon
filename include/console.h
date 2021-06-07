/**
 * @file console.h
 * @brief Console Support
 * @ingroup console
 */

#ifndef __LIBDRAGON_CONSOLE_H
#define __LIBDRAGON_CONSOLE_H

#include <stdbool.h>
#include "display.h"

/**
 * @addtogroup console
 * @{ 
 */

/**
 * @name Console Render Modes
 * @brief Modes for rendering the console to the screen
 *
 * @{
 */

/** 
 * @brief Manual Rendering
 *
 * The user must call #console_render when the screen should be updated
 */
#define RENDER_MANUAL       0
/** 
 * @brief Automatic Rendering
 *
 * The screen will be updated on every console interaction
 */
#define RENDER_AUTOMATIC    1
/** @} */

/**
 * @name Console Dimensions
 * @{
 */

/**
 * @brief The console width in characters
 * @note Right padding is not enforced by code, adjust this to fit into
 * [horizontal resolution] - 2 * HORIZONTAL_PADDING
 */
#define CONSOLE_WIDTH       64
/**
 * @brief The console height in characters
 * @note Bottom padding is not enforced by code, adjust this to fit into
 * [vertical resolution] - 2 * VERTICAL_PADDING
 */
#define CONSOLE_HEIGHT      28
/** @} */

/** 
 * @brief Tab width
 *
 * @note This needs to divide evenly into #CONSOLE_WIDTH 
 */
#define TAB_WIDTH           4

/**
 * @brief Padding from the left and right ends of the screen in pixels
 *
 * @note This should be ~10% of the horizontal resolution to be safely visible
 */
#define HORIZONTAL_PADDING  64

/**
 * @brief Padding from the top and bottom ends of the screen in pixels
 */
#define VERTICAL_PADDING    8

#ifdef __cplusplus
extern "C" {
#endif

void console_init();
void console_close();
void console_set_debug(bool debug);
void console_set_render_mode(int mode);
void console_clear();
void console_render();

#ifdef __cplusplus
}
#endif

/** @} */ /* console */

#endif
