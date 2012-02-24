
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

/* hardware definitions */
// Pad buttons
#define A_BUTTON(a)     ((a) & 0x8000)
#define B_BUTTON(a)     ((a) & 0x4000)
#define Z_BUTTON(a)     ((a) & 0x2000)
#define START_BUTTON(a) ((a) & 0x1000)

// D-Pad
#define DU_BUTTON(a)    ((a) & 0x0800)
#define DD_BUTTON(a)    ((a) & 0x0400)
#define DL_BUTTON(a)    ((a) & 0x0200)
#define DR_BUTTON(a)    ((a) & 0x0100)

// Triggers
#define TL_BUTTON(a)    ((a) & 0x0020)
#define TR_BUTTON(a)    ((a) & 0x0010)

// Yellow C buttons
#define CU_BUTTON(a)    ((a) & 0x0008)
#define CD_BUTTON(a)    ((a) & 0x0004)
#define CL_BUTTON(a)    ((a) & 0x0002)
#define CR_BUTTON(a)    ((a) & 0x0001)

#define PAD_DEADZONE     5
#define PAD_ACCELERATION 10
#define PAD_CHECK_TIME   40


unsigned short gButtons = 0;
struct controller_data gKeys;

volatile int gTicks;                    /* incremented every vblank */

/* input - do getButtons() first, then getAnalogX() and/or getAnalogY() */
unsigned short getButtons(int pad)
{
    // Read current controller status
    controller_scan();
    gKeys = get_keys_pressed();
    return (unsigned short)(gKeys.c[0].data >> 16);
}

unsigned char getAnalogX(int pad)
{
    return (unsigned char)gKeys.c[pad].x;
}

unsigned char getAnalogY(int pad)
{
    return (unsigned char)gKeys.c[pad].y;
}

display_context_t lockVideo(int wait)
{
    display_context_t dc;

    if (wait)
        while (!(dc = display_lock()));
    else
        dc = display_lock();
    return dc;
}

void unlockVideo(display_context_t dc)
{
    if (dc)
        display_show(dc);
}

/* text functions */
void drawText(display_context_t dc, char *msg, int x, int y)
{
    if (dc)
        graphics_draw_text(dc, x, y, msg);
}

void printText(display_context_t dc, char *msg, int x, int y)
{
    if (dc)
        graphics_draw_text(dc, x*8, y*8, msg);
}

/* vblank callback */
void vblCallback(void)
{
    gTicks++;
}

void delay(int cnt)
{
    int then = gTicks + cnt;
    while (then > gTicks) ;
}

/* initialize console hardware */
void init_n64(void)
{
    /* enable interrupts (on the CPU) */
    init_interrupts();

    /* Initialize peripherals */
    display_init( RESOLUTION_320x240, DEPTH_32_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );

    register_VI_handler(vblCallback);

    controller_init();
}

/* main code entry point */
int main(void)
{
    display_context_t _dc;
    char temp[128];
	int res = 0;
	unsigned short buttons, previous = 0;

    init_n64();

    while (1)
    {
		int j;
		int width[4] = { 320, 640, 256, 512 };
		int height[4] = { 240, 480, 240, 480 };
		unsigned int color;

        _dc = lockVideo(1);
		color = graphics_make_color(0xCC, 0xCC, 0xCC, 0xFF);
		graphics_fill_screen(_dc, color);

		color = graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF);
		graphics_draw_line(_dc, 0, 0, width[res]-1, 0, color);
		graphics_draw_line(_dc, width[res]-1, 0, width[res]-1, height[res]-1, color);
		graphics_draw_line(_dc, width[res]-1, height[res]-1, 0, height[res]-1, color);
		graphics_draw_line(_dc, 0, height[res]-1, 0, 0, color);

		graphics_draw_line(_dc, 0, 0, width[res]-1, height[res]-1, color);
		graphics_draw_line(_dc, 0, height[res]-1, width[res]-1, 0, color);

		color = graphics_make_color(0x00, 0x00, 0x00, 0xFF);
		graphics_set_color(color, 0);

        printText(_dc, "Video Resolution Test", width[res]/16 - 10, 3);
		switch (res)
		{
			case 0:
				printText(_dc, "320x240", width[res]/16 - 3, 5);
				break;
			case 1:
				printText(_dc, "640x480", width[res]/16 - 3, 5);
				break;
			case 2:
				printText(_dc, "256x240", width[res]/16 - 3, 5);
				break;
			case 3:
				printText(_dc, "512x480", width[res]/16 - 3, 5);
				break;
		}

		for (j=0; j<8; j++)
		{
			sprintf(temp, "Line %d", j);
			printText(_dc, temp, 3, j);
			sprintf(temp, "Line %d", height[res]/8 - j - 1);
			printText(_dc, temp, 3, height[res]/8 - j - 1);
		}
		printText(_dc, "0123456789", 0, 16);
		printText(_dc, "9876543210", width[res]/8 - 10, 16);

        unlockVideo(_dc);

        while (1)
        {
            // wait for change in buttons
            buttons = getButtons(0);
            if (buttons ^ previous)
                break;
            delay(1);
        }

        if (A_BUTTON(buttons ^ previous))
        {
            // A changed
            if (!A_BUTTON(buttons))
			{
				resolution_t mode[4] = { RESOLUTION_320x240, RESOLUTION_640x480, RESOLUTION_256x240, RESOLUTION_512x480 };
				res = (res+1) & 3;
				display_close();
				display_init(mode[res], DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
			}
		}

        previous = buttons;
    }

    return 0;
}
