#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdarg.h>
#include "libdragon.h"

#define CONSOLE_SIZE        ((sizeof(char) * CONSOLE_WIDTH * CONSOLE_HEIGHT) + sizeof(char))

static char *render_buffer = 0;
static int render_now;

void console_set_render_mode(int mode)
{
    /* Allow manual buffering somewhat like curses */
    render_now = mode;
}

void console_init()
{
    /* In case they initialized the display already */
    display_close();
    display_init( RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );

    render_buffer = malloc(CONSOLE_SIZE);

    console_clear();
    console_set_render_mode(RENDER_AUTOMATIC);
}

void console_close()
{
    if(render_buffer)
    {
        /* Nuke the console buffer */
        free(render_buffer);
        render_buffer = 0;
    }
}

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

/* Use this several times */
#define move_buffer() \
    memmove(render_buffer, render_buffer + (sizeof(char) * CONSOLE_WIDTH), CONSOLE_SIZE - (CONSOLE_WIDTH * sizeof(char))); \
    pos -= CONSOLE_WIDTH;

void console_printf(char *format, ...)
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

