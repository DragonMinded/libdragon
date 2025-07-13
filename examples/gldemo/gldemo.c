#include <libdragon.h>
#include <GL/gl.h>
#include <GL/gl_integration.h>

static const GLfloat environment_color[] = { 0.1f, 0.03f, 0.2f, 1.f };

int main()
{
	debug_init(DEBUG_FEATURE_LOG_ISVIEWER | DEBUG_FEATURE_LOG_USB);
    joypad_init();
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS_DEDITHER);
    gl_init();

    while (true)
    {
        joypad_poll();

        surface_t *disp = display_get();
        surface_t *zbuf = display_get_zbuf();

        rdpq_attach(disp, zbuf);

        gl_context_begin();

        glClearColor(environment_color[0], environment_color[1], environment_color[2], environment_color[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        gl_context_end();

        rdpq_detach_show();
    }
}
