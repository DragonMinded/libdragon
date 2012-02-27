/**
 * @file console.h
 * @brief Console Support
 * @ingroup console
 */

#ifndef __LIBDRAGON_CONSOLE_H
#define __LIBDRAGON_CONSOLE_H

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

/** @brief The console width in characters */
#define CONSOLE_WIDTH       35
/** @brief The console height in characters */
#define CONSOLE_HEIGHT      25
/** @} */

/** 
 * @brief Tab width
 *
 * @note This needs to divide evenly into #CONSOLE_WIDTH 
 */
#define TAB_WIDTH           5

#ifdef __cplusplus
extern "C" {
#endif

void console_init();
void console_close();
void console_set_render_mode(int mode);
void console_clear();
void console_render();

#ifdef __cplusplus
}
#endif

/** @} */ /* console */

#endif
