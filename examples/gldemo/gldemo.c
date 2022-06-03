#include <libdragon.h>
#include <GL/gl.h>
#include <GL/gl_integration.h>

int main()
{
	debug_init_isviewer();
	debug_init_usblog();

    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 1, GAMMA_NONE, ANTIALIAS_RESAMPLE);

    gl_init();

    while (1)
    {
        glClearColor(0.4f, 0.1f, 0.5f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        gl_swap_buffers();
    }
}
