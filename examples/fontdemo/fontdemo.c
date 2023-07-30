#include <libdragon.h>

int main()
{
    debug_init_isviewer();
    debug_init_usblog();

    dfs_init(DFS_DEFAULT_LOCATION);
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    rdpq_init();
    rdpq_debug_start();

    rdpq_font_t *fnt1 = rdpq_font_load("rom:/Pacifico.font64");

    while (1) {
        surface_t *screen = display_get();

        rdpq_attach(screen, NULL);
        rdpq_clear(RGBA32(0x30,0x63,0x8E,0xFF));

        rdpq_font_begin(RGBA32(0xED, 0xAE, 0x49, 0xFF));
        rdpq_font_position(20, 50);
        rdpq_font_print(fnt1, "Jumping over the river");
        rdpq_font_end();

        rdpq_detach_show();
        break;
    }
}