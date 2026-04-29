#include <libdragon.h>

int main(void)
{
    debug_init_isviewer();
    debug_init_usblog();
    display_init(RESOLUTION_640x480, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_DISABLED);
    dfs_init(DFS_DEFAULT_LOCATION);
    rdpq_init();
    joypad_init();

    // Register the LSPR decoder with sprite_load
    lossysprite_init();

    // Load the lossily-compressed background sprites
    sprite_t *bg_sprites[] = {
        sprite_load("rom:/bg0.sprite"),
        sprite_load("rom:/bg1.sprite"),
        sprite_load("rom:/bg2.sprite"),
        sprite_load("rom:/bg3.sprite"),
    };

    // Verify that the background sprites were loaded with the expected formats
    assertf(sprite_is_yuv_nv12(bg_sprites[0]), "bg0.sprite should be NV12");
    assertf(sprite_get_format(bg_sprites[1]) == FMT_YUV16, "bg1.sprite should be YUV16");
    assertf(sprite_get_format(bg_sprites[2]) == FMT_RGBA16, "bg2.sprite should be RGBA16");
    assertf(sprite_get_format(bg_sprites[3]) == FMT_RGBA32, "bg3.sprite should be RGBA32");

    // Load the losslessly-compressed brew sprite
    sprite_t *brew_sprite = sprite_load("rom:/n64brew.sprite");

    const int sw = display_get_width();
    const int sh = display_get_height();
    const int bw = brew_sprite->width;
    const int bh = brew_sprite->height;

    float x = 100.0f, y = 80.0f;
    float vx = 2.5f,  vy = 2.0f;

    int bg_idx = 0;

    while (1) {
        // Bounce the brew sprite around the screen edges
        x += vx; y += vy;
        if (x < 0)        { x = 0;        vx = -vx; }
        if (y < 0)        { y = 0;        vy = -vy; }
        if (x + bw > sw)  { x = sw - bw;  vx = -vx; }
        if (y + bh > sh)  { y = sh - bh;  vy = -vy; }

        // Draw the current background and the brew sprite on top
        surface_t *screen = display_get();
        rdpq_attach(screen, NULL);
        rdpq_set_mode_standard();
        rdpq_mode_alphacompare(1); // colorkey (draw pixel with alpha >= 1)
        rdpq_sprite_blit(bg_sprites[bg_idx],  0, 0, NULL);
        rdpq_sprite_blit(brew_sprite, x, y, NULL);
        rdpq_detach_show();

        // Cycle background images on input
        joypad_poll();
        joypad_buttons_t btn = joypad_get_buttons_pressed(JOYPAD_PORT_1);
        if (btn.r || btn.a) bg_idx = (bg_idx + 1) % 4;
        if (btn.l || btn.z || btn.b) bg_idx = (bg_idx - 1 + 4) % 4;
    }
}
