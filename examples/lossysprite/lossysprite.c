#include <libdragon.h>

int main(void)
{
    debug_init_isviewer();
    debug_init_usblog();
    display_init(RESOLUTION_640x480, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_DISABLED);
    dfs_init(DFS_DEFAULT_LOCATION);
    joypad_init();
    rdpq_init();

    sprite_t *bkg = lossysprite_load("rom:/background.sprite");

    while (1) {
        surface_t *screen = display_get();
        rdpq_attach(screen, NULL);
        rdpq_set_mode_copy(false);
        rdpq_sprite_blit(bkg, 0, 0, NULL);
        rdpq_detach_show();

        joypad_poll();
        if (joypad_get_buttons_pressed(JOYPAD_PORT_1).start) break;
    }

    sprite_free(bkg);
    return 0;
}
