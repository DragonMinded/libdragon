#include <libdragon.h>

#define MAX(a, b)       ((a) > (b) ? (a) : (b))
#define MIN(a, b)       ((a) < (b) ? (a) : (b))

int main(void)
{
    debug_init_isviewer();
    debug_init_usblog();
    joypad_init();

    vi_init();
    dfs_init(DFS_DEFAULT_LOCATION);
    rdpq_init();
    rdpq_debug_start();

    sprite_t *bkg = sprite_load("rom:/philips.rgba32.sprite");
    surface_t fb1 = surface_alloc(FMT_RGBA16, 320, 240);
    surface_t fb2 = surface_alloc(FMT_RGBA16, 640, 480);

    rdpq_attach(&fb1, NULL);
    rdpq_set_mode_standard();
    rdpq_sprite_blit(bkg, 0, 0, &(rdpq_blitparms_t){
        .scale_x = 0.5f, .scale_y = 0.5f,
    });
    rdpq_detach_wait();

    rdpq_attach(&fb2, NULL);
    rdpq_set_mode_standard();
    rdpq_sprite_blit(bkg, 0, 0, &(rdpq_blitparms_t){});
    rdpq_detach_wait();

    vi_show(&fb1);

    bool interlacing = false;
    bool hires = false;
    bool borders = false;

    while (1) {
        joypad_poll();
        joypad_buttons_t btn = joypad_get_buttons_pressed(JOYPAD_PORT_1);
        joypad_buttons_t down = joypad_get_buttons_held(JOYPAD_PORT_1);

        vi_write_begin();

        if (btn.a) { 
            borders = !borders;
            vi_set_borders(vi_calc_borders(4.0f / 3.0f, borders ? VI_CRT_MARGIN : 0));
            surface_t *fb = hires ? &fb2 : &fb1;
            vi_set_xscale(fb->width);
            vi_set_yscale(fb->height);
        }
        if (btn.b) {
            hires = !hires;
            debugf("HiRes: %d\n", hires);
            vi_show(hires ? &fb2 : &fb1);
            borders = false;
        }
        if (btn.z) {
            interlacing = !interlacing;
            debugf("Interlacing: %d\n", interlacing);
            vi_set_interlaced(interlacing);
        }

        if (down.c_up)    vi_scroll_output(0, -1);
        if (down.c_down)  vi_scroll_output(0, 1);
        if (down.c_left)  vi_scroll_output(-1, 0);
        if (down.c_right) vi_scroll_output(1, 0);
        vi_write_end();

        vi_wait_vblank();
    }
}