#ifndef __LIBDRAGON_DISPLAY_H
#define __LIBDRAGON_DISPLAY_H

#include <stdint.h>

typedef enum
{
    RESOLUTION_320x240,
    RESOLUTION_640x480
} resolution_t;

typedef enum
{
    DEPTH_16_BPP,
    DEPTH_32_BPP
} bitdepth_t;

typedef enum
{
    GAMMA_NONE,
    GAMMA_CORRECT,
    GAMMA_CORRECT_DITHER
} gamma_t;

typedef enum
{
    ANTIALIAS_OFF,
    ANTIALIAS_RESAMPLE,
    ANTIALIAS_RESAMPLE_FETCH_NEEDED,
    ANTIALIAS_RESAMPLE_FETCH_ALWAYS
} antialias_t;

typedef int display_context_t;

/* Initialize video system.  This sets up a triple buffered drawing surface which can
   be blitted or rendered to. */
void display_init( resolution_t res, bitdepth_t bit, uint32_t num_buffers, gamma_t gamma, antialias_t aa );

/* Grab a display context that is safe for drawing.  If none is available
   then this will return 0.  You should not check more than one display
   context out at once. */
display_context_t display_lock();

/* Display a valid display context to the screen on the next vblank.  Display
   contexts should be locked via display_lock() */
void display_show(display_context_t disp);

/* Close a display and free buffer memory associated with it */
void display_close();

#endif
