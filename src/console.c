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
#include <unistd.h>
#include "system.h"
#include "libdragon.h"

/* Prototypes */
static void __console_render(void);

/** @brief Size of the console buffer in bytes */
#define CONSOLE_SIZE        ((sizeof(char) * CONSOLE_WIDTH * CONSOLE_HEIGHT) + sizeof(char))

/** @brief The console buffer */
static char *render_buffer = 0;
/** 
 * @brief Internal state of the render mode
 * @see #RENDER_AUTOMATIC and #RENDER_MANUAL
 */
static int render_now;
/** @brief True if the console output is sent to debug channel as well */
static bool console_redirect_debug = true;

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

    /* Redirect to stderr if requested for debugging purposes */
    if (console_redirect_debug)
        write(2, buf, len);

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

void console_init()
{
    /* In case they initialized the display already */
    display_close();
    display_init( RESOLUTION_640x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE );

    render_buffer = malloc(CONSOLE_SIZE);

    console_set_render_mode(RENDER_AUTOMATIC);
    console_clear();
    console_set_debug(true);
    graphics_set_color(0xFFFFFFFF, 0x00000000);

    /* Register ourselves with newlib */
    stdio_t console_calls = { 0, __console_write, 0 };
    hook_stdio_calls( &console_calls );

    graphics_set_default_font();
}

void console_close()
{
    if(render_buffer)
    {
        /* Nuke the console buffer */
        free(render_buffer);
        render_buffer = 0;
    }

    /* Unregister ourselves from newlib */
    stdio_t console_calls = { 0, __console_write, 0 };
    unhook_stdio_calls( &console_calls );
}

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
static void __console_render(void)
{
    if(!render_buffer) { return; }

    /* Wait until we get a valid context */
    surface_t *dc = display_get();

    /* Background color! */
    graphics_fill_screen( dc, 0 );

    for(int y = 0; y < CONSOLE_HEIGHT; y++)
    {
        for(int x = 0; x < CONSOLE_WIDTH; x++)
        {
            char t_buf = render_buffer[y * CONSOLE_WIDTH + x];

            if(t_buf == 0)
            {
                goto end;
            }

            /* Draw to the screen using the forecolor and backcolor set in the graphics
             * subsystem */
            graphics_draw_character( dc, HORIZONTAL_PADDING + 8 * x, VERTICAL_PADDING + 8 * y, t_buf );
        }
    }

end:;
    /* If the interrupts are disabled, the console wouldn't show to the screen.
     * Since the console is only used for development and emergency context,
     * it is better to force display irrespective of vblank. */
    uint32_t c0_status = C0_STATUS();
    if ((c0_status & C0_STATUS_IE) == 0 || ((c0_status & (C0_STATUS_EXL|C0_STATUS_ERL)) != 0))
    {
        extern void display_show_force(display_context_t dc);
        display_show_force(dc);
    }
    else
        display_show(dc);
}

void console_render()
{
    /* Ensure data is flushed before rendering */
    fflush( stdout );

    /* Render now */
    __console_render();
}

void console_set_debug(bool debug)
{
    console_redirect_debug = debug;
}
