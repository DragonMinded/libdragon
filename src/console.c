/**
 * @file console.c
 * @brief Console Support
 * @ingroup console
 */

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdarg.h>
#include "libdragon.h"

/**
 * @defgroup console Console Support
 * @ingroup display
 * @{
 */

/** @brief Size of the console buffer in bytes */
#define CONSOLE_SIZE        ((sizeof(char) * CONSOLE_WIDTH * CONSOLE_HEIGHT) + sizeof(char))

/** @brief The console buffer */
static char *render_buffer = 0;
/** 
 * @brief Internal state of the render mode
 * @see #RENDER_AUTOMATIC and #RENDER_MANUAL
 */
static int render_now;

/**
 * @brief Set the console rendering mode
 *
 * This sets the render mode of the console.  The #RENDER_AUTOMATIC mode allows
 * console_printf to immediately be placed onto the screen.  This is very similar
 * to a normal console on a unix/windows system.  The #RENDER_MANUAL mode allows
 * console_printf to be buffered, and displayed at a later date using
 * console_render().  This is to allow a rendering interface somewhat analogous
 * to curses
 *
 * @param[in] mode
 *            Render mode (#RENDER_AUTOMATIC or #RENDER_MANUAL)
 */
void console_set_render_mode(int mode)
{
    /* Allow manual buffering somewhat like curses */
    render_now = mode;
}

/**
 * @brief Initialize the console
 *
 * Initialize the console system.  This will initialize the video properly, so
 * a call to the display_init() fuction is not necessary.
 */
void console_init()
{
    /* In case they initialized the display already */
    display_close();
    display_init( RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );

    render_buffer = malloc(CONSOLE_SIZE);

    console_clear();
    console_set_render_mode(RENDER_AUTOMATIC);
}

/**
 * @brief Close the console
 *
 * Free the console system.  This will clean up any dynamic memry that was in
 * use.
 */
void console_close()
{
    if(render_buffer)
    {
        /* Nuke the console buffer */
        free(render_buffer);
        render_buffer = 0;
    }
}

/**
 * @brief Clear the console
 *
 * Clear the console and set the virtual cursor back to the top left.
 */
void console_clear()
{
    if(!render_buffer) { return; }

    memset(render_buffer, 0, CONSOLE_SIZE);
    
    /* Should we display? */
    if(render_now == RENDER_AUTOMATIC)
    {
        console_render();
    }
}

/**
 * @brief Render the console
 *
 * Render the console to the screen.  This should be called when in manual
 * rendering mode to display the console to the screen.  In automatic mode
 * it is not necessary to call.
 */
void console_render()
{
    if(!render_buffer) { return; }

    static display_context_t dc = 0;

    /* Wait until we get a valid context */
    while(!(dc = display_lock()));

    /* Background color! */
    graphics_fill_screen( dc, 0 );

    for(int y = 0; y < CONSOLE_HEIGHT; y++)
    {
        for(int x = 0; x < CONSOLE_WIDTH; x++)
        {
            char t_buf = render_buffer[y * CONSOLE_WIDTH + x];

            if(t_buf == 0)
            {
                display_show(dc);
                return;
            }

            /* White foreground, transparent background */
            graphics_draw_character( dc, 20 + 8 * x, 16 + 8 * y, 0xFFFFFFFF, 0x0, t_buf );
        }
    }

    display_show(dc);
}

/**
 * @brief Macro to move the console up one line
 */
#define move_buffer() \
    memmove(render_buffer, render_buffer + (sizeof(char) * CONSOLE_WIDTH), CONSOLE_SIZE - (CONSOLE_WIDTH * sizeof(char))); \
    pos -= CONSOLE_WIDTH;

/**
 * @brief Write to the console
 *
 * @note This function works equivalent to printf
 *
 * @param[in] format
 *            A printf-style format string to print to the console
 */
void console_printf( const char * const format, ... )
{
    static char buf[1024];
    va_list args;

    if(!render_buffer) { return; }

    /* Render to a temporary buffer */
    va_start(args, format);
    vsprintf(buf, format, args);
    va_end(args);

    int pos = strlen(render_buffer);

    /* Copy over to screen buffer */
    for(int x = 0; x < strlen(buf); x++)
    {
        if(pos == CONSOLE_WIDTH * CONSOLE_HEIGHT)
        {
            /* Need to scroll the buffer */
            move_buffer();
        }

        switch(buf[x])
        {
            case '\r':
            case '\n':
                /* Add enough space to get to next line */
                if(!(pos % CONSOLE_WIDTH))
                {
                    render_buffer[pos++] = ' ';
                }

                while(pos % CONSOLE_WIDTH)
                {
                    render_buffer[pos++] = ' ';
                }

                /* Make sure we don't run down the end */
                if(pos == CONSOLE_WIDTH * CONSOLE_HEIGHT)
                {
                    move_buffer();
                }

                break;
            case '\t':
                /* Add enough spaces to go to the next tab stop */
                if(!(pos % TAB_WIDTH))
                {
                    render_buffer[pos++] = ' ';
                }

                while(pos % TAB_WIDTH)
                {
                    render_buffer[pos++] = ' ';
                }

                /* Make sure we don't run down the end */
                if(pos == CONSOLE_WIDTH * CONSOLE_HEIGHT)
                {
                    move_buffer();
                }
                break;
            default:
                /* Copy character over */
                render_buffer[pos++] = buf[x];
                break;
        }
    }

    /* Cap off the end! */
    render_buffer[pos] = 0;

    /* Out to screen! */
    if(render_now == RENDER_AUTOMATIC)
    {
        console_render();
    }
}

/** @} */ /* console */
