/**
 * @file console.c
 * @brief Console Support
 * @ingroup console
 */

#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "system.h"
#include "libdragon.h"

/**
 * @defgroup console Console Support
 * @ingroup display
 * @brief Software console emulation for debugging and simple text output.
 *
 * Console support is provided as a poor-man's console for simple debugging on
 * the N64.  It does not respect common escape sequences and is nonstandard in
 * size.  When using the console, code should be careful to make sure that the
 * display system has not been initialized.  Similarly, if the display system
 * is needed, code should be sure that the console is not initialized.
 *
 * Code wishing to use the console should first initialize the console support in
 * libdragon with #console_init.  Once the console has been initialized, it wil
 * operate in one of two modes.  In automatic mode, every write to the console will
 * be immediately displayed on the screen.  The console will be scrolled when the
 * buffer fills.  In manual mode, the console will only be displayed after calling
 * #console_render.  To set the render mode, use #console_set_render_mode.  To
 * add data to the console, use #printf or #iprintf.  To clear the console and reset
 * the scroll, use #console_clear.  Once the console is not needed or when the
 * code wishes to switch to the display subsystem, #console_clear should be called
 * to cleanly shut down the console support.
 *
 * @{
 */

/* Prototypes */
static void __console_render();

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
 * @brief Macro to move the console up one line
 */
#define move_buffer() \
    memmove(render_buffer, render_buffer + (sizeof(char) * CONSOLE_WIDTH), CONSOLE_SIZE - (CONSOLE_WIDTH * sizeof(char))); \
    pos -= CONSOLE_WIDTH;

/**
 * @brief Newlib hook to allow printf/iprintf to appear on console
 *
 * @param[in] buf
 *            Pointer to data buffer containing the data to write
 * @param[in] len
 *            Length of data in bytes expected to be written
 *
 * @return Number of bytes written
 */
static int __console_write( char *buf, unsigned int len )
{
    int pos = strlen(render_buffer);

    /* Copy over to screen buffer */
    for(int x = 0; x < len; x++)
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
        __console_render();
    }

    /* Always write all */
    return len;
}

/**
 * @brief Structure used for hooking console commands to stdio
 */
static stdio_t console_calls = {
    0,
    __console_write,
    0
};

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

    /* Register ourselves with newlib */
    hook_stdio_calls( &console_calls );
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

    /* Unregister ourselves from newlib */
    unhook_stdio_calls();
}

/**
 * @brief Clear the console
 *
 * Clear the console and set the virtual cursor back to the top left.
 */
void console_clear()
{
    if(!render_buffer) { return; }

    /* Force fflush not to draw regardless */
    int render = render_now;
    render_now = RENDER_MANUAL;

    /* Flush the stdout so that we don't get data after the clear */
    fflush( stdout );

    /* Return to original */
    render_now = render;

    /* Remove all data */
    memset(render_buffer, 0, CONSOLE_SIZE);
    
    /* Should we display? */
    if(render_now == RENDER_AUTOMATIC)
    {
        __console_render();
    }
}

/**
 * @brief Helper function to render the console
 */
static void __console_render()
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

            /* Draw to the screen using the forecolor and backcolor set in the graphics
             * subsystem */
            graphics_draw_character( dc, 20 + 8 * x, 16 + 8 * y, t_buf );
        }
    }

    display_show(dc);
}

/**
 * @brief Render the console
 *
 * Render the console to the screen.  This should be called when in manual
 * rendering mode to display the console to the screen.  In automatic mode
 * it is not necessary to call.
 *
 * The color that is used to draw the text can be set using #graphics_set_color.
 */
void console_render()
{
    /* Ensure data is flushed before rendering */
    fflush( stdout );

    /* Render now */
    __console_render();
}

/** @} */ /* console */
