#include <libdragon.h>

typedef struct {
    const char *filename;
    tex_format_t expected_format;
    bool is_yuv_nv12;
} bg_info_t;

static const bg_info_t bg_info[] = {
    { "rom:/bg0.sprite", FMT_YUV16, true },
    { "rom:/bg1.sprite", FMT_YUV16, false },
    { "rom:/bg2.sprite", FMT_RGBA16, false },
    { "rom:/bg3.sprite", FMT_RGBA32, false },
};

static const int BG_COUNT = sizeof(bg_info) / sizeof(bg_info[0]);

int bg_idx = 0;
sprite_t *bg_sprite = NULL;

void switch_bg(int direction) {
    if (bg_sprite) {
        sprite_free(bg_sprite);
        bg_sprite = NULL;
    }
    bg_idx = (bg_idx + direction + BG_COUNT) % BG_COUNT;
    bg_sprite = sprite_load(bg_info[bg_idx].filename);
    // Verify that the background sprites were loaded with the expected formats
    assertf(sprite_get_format(bg_sprite) == bg_info[bg_idx].expected_format, "%s has unexpected format", bg_info[bg_idx].filename);
    assertf(sprite_is_yuv_nv12(bg_sprite) == bg_info[bg_idx].is_yuv_nv12, "%s should be NV12", bg_info[bg_idx].filename);
}

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
    switch_bg(0); // load the first background sprite

    // Prepare a YUV blitter for the NV12 background sprite for optimized rendering
    yuv_blitter_t bg_blitter = rdpq_sprite_yuv_blitter_new(bg_sprite, 0, 0, NULL);

    // Load the losslessly-compressed brew sprite
    sprite_t *brew_sprite = sprite_load("rom:/n64brew.sprite");

    const int sw = display_get_width();
    const int sh = display_get_height();
    const int bw = brew_sprite->width;
    const int bh = brew_sprite->height;

    float x = 100.0f, y = 80.0f;
    float vx = 2.5f,  vy = 2.0f;

    while (1) {
        // Bounce the brew sprite around the screen edges
        x += vx; y += vy;
        if (x < 0)        { x = 0;        vx = -vx; }
        if (y < 0)        { y = 0;        vy = -vy; }
        if (x + bw > sw)  { x = sw - bw;  vx = -vx; }
        if (y + bh > sh)  { y = sh - bh;  vy = -vy; }

        surface_t *screen = display_get();
        rdpq_attach(screen, NULL);
        {
            // Draw the background
            if (bg_info[bg_idx].is_yuv_nv12) {
                rdpq_sprite_yuv_blitter_run(&bg_blitter, bg_sprite);
            } else {
                rdpq_sprite_blit(bg_sprite, 0, 0, NULL);
            }
            // Draw the brew sprite
            rdpq_set_mode_standard();
            rdpq_mode_alphacompare(1); // colorkey (draw pixel with alpha >= 1)
            rdpq_sprite_blit(brew_sprite, x, y, NULL);
        }
        rdpq_detach_show();

        // Cycle background images on input
        joypad_poll();
        joypad_buttons_t btn = joypad_get_buttons_pressed(JOYPAD_PORT_1);
        if (btn.r || btn.a) switch_bg(1);
        if (btn.l || btn.z || btn.b) switch_bg(-1);
    }
}
