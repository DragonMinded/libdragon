#include <libdragon.h>

int main(void)
{
    debug_init_isviewer();
    debug_init_usblog();
    display_init(RESOLUTION_640x480, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_DISABLED);
    dfs_init(DFS_DEFAULT_LOCATION);
    rdpq_init();

    sprite_init_lossy();
    sprite_t *bkg  = sprite_load("rom:/background.sprite");
    sprite_t *brew = sprite_load("rom:/n64brew.sprite");

    const int sw = display_get_width();
    const int sh = display_get_height();
    const int bw = brew->width;
    const int bh = brew->height;

    float x = 100.0f, y = 80.0f;
    float vx = 2.5f,  vy = 2.0f;

    while (1) {
        x += vx; y += vy;
        if (x < 0)        { x = 0;        vx = -vx; }
        if (y < 0)        { y = 0;        vy = -vy; }
        if (x + bw > sw)  { x = sw - bw;  vx = -vx; }
        if (y + bh > sh)  { y = sh - bh;  vy = -vy; }

        surface_t *screen = display_get();
        rdpq_attach(screen, NULL);
        rdpq_set_mode_standard();
        rdpq_mode_alphacompare(1); // colorkey (draw pixel with alpha >= 1)
        rdpq_sprite_blit(bkg,  0, 0, NULL);
        rdpq_sprite_blit(brew, x, y, NULL);
        rdpq_detach_show();
    }
}
