
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

void printText(surface_t* dc, char *msg, int x, int y)
{
    graphics_draw_text(dc, x*8, y*8, msg);
}

/* main code entry point */
int main(void)
{
    char temp[128];
    int res = 0;

    /* Initialize peripherals */
    display_init( RESOLUTION_320x240, DEPTH_32_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE );

    joypad_init();

    while (1)
    {
        int width[6] = { 320, 640, 256, 512, 512, 640 };
        int height[6] = { 240, 480, 240, 480, 240, 240 };

        surface_t* display = display_get();
        uint32_t color = graphics_make_color(0xCC, 0xCC, 0xCC, 0xFF);
        graphics_fill_screen(display, color);

        color = graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF);
        graphics_draw_line(display, 0, 0, width[res]-1, 0, color);
        graphics_draw_line(display, width[res]-1, 0, width[res]-1, height[res]-1, color);
        graphics_draw_line(display, width[res]-1, height[res]-1, 0, height[res]-1, color);
        graphics_draw_line(display, 0, height[res]-1, 0, 0, color);

        graphics_draw_line(display, 0, 0, width[res]-1, height[res]-1, color);
        graphics_draw_line(display, 0, height[res]-1, width[res]-1, 0, color);

        color = graphics_make_color(0x00, 0x00, 0x00, 0xFF);
        graphics_set_color(color, 0);

        printText(display, "Video Resolution Test", width[res]/16 - 10, 3);
        switch (res)
        {
            case 0:
                printText(display, "320x240p", width[res]/16 - 3, 5);
                break;
            case 1:
                printText(display, "640x480i", width[res]/16 - 3, 5);
                break;
            case 2:
                printText(display, "256x240p", width[res]/16 - 3, 5);
                break;
            case 3:
                printText(display, "512x480i", width[res]/16 - 3, 5);
                break;
            case 4:
                printText(display, "512x240p", width[res]/16 - 3, 5);
                break;
            case 5:
                printText(display, "640x240p", width[res]/16 - 3, 5);
                break;
        }

        for (int j = 0; j < 8; j++)
        {
            sprintf(temp, "Line %d", j);
            printText(display, temp, 3, j);
            sprintf(temp, "Line %d", height[res]/8 - j - 1);
            printText(display, temp, 3, height[res]/8 - j - 1);
        }
        printText(display, "0123456789", 0, 16);
        printText(display, "9876543210", width[res]/8 - 10, 16);

        display_show(display);

        joypad_poll();

        joypad_buttons_t buttons = joypad_get_buttons_pressed(JOYPAD_PORT_1);

        // A changed
        if (buttons.a)
        {
            resolution_t mode[6] = {
                RESOLUTION_320x240,
                RESOLUTION_640x480,
                RESOLUTION_256x240,
                RESOLUTION_512x480,
                RESOLUTION_512x240,
                RESOLUTION_640x240,
            };
            res++;
            res %= 6;
            display_close();
            display_init(mode[res], DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);
        }
    }

    return 0;
}
