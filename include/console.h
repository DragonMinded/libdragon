#ifndef __LIBDRAGON_CONSOLE_H
#define __LIBDRAGON_CONSOLE_H

#include "display.h"

/* Modes for rendering the console to the screen */
#define RENDER_MANUAL       0
#define RENDER_AUTOMATIC    1

/* Useful defines for those drawing with console */
#define CONSOLE_WIDTH       35
#define CONSOLE_HEIGHT      25

/* Tab width (needs to divide evenly into CONSOLE_WIDTH */
#define TAB_WIDTH           5

/* Initialize the console system.  This will initialize the video properly, so
   a call to the display_init() fuction is not necessary */
void console_init();

/* Free the console system.  This will clean up any dynamic memry that was in
   use */
void console_close();

/* This sets the render mode of the console.  The RENDER_AUTOMATIC mode allows
   console_printf to immediately be placed onto the screen.  This is very similar
   to a normal console on a unix/windows system.  The RENDER_MANUAL mode allows
   console_printf to be buffered, and displayed at a later date using
   console_render().  This is to allow a rendering interface somewhat analogous
   to curses */
void console_set_render_mode(int mode);

/* Clear the console.  The mode of rendering is affected by
   console_set_render_mode() */
void console_clear();

/* Render the currently buffered console.  This should only be called by a program
   if the render mode is set up as RENDER_MANUAL */
void console_render();

/* Print to console.  Supports all modes standard printf supports.  The mode of
   rendering is affected by console_set_render_mode(). */
void console_printf(const char * const format, ...);

#endif
