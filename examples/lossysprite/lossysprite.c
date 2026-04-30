#include <libdragon.h>

typedef struct {
    const char *filename;
    tex_format_t tex_format;
    yuv_format_t yuv_format;
} bg_info_t;

static const bg_info_t bg_info[] = {
    { .filename = "rom:/bg0.sprite", .tex_format = FMT_YUV16, .yuv_format = YUV_NV12 },
    { .filename = "rom:/bg1.sprite", .tex_format = FMT_YUV16, .yuv_format = YUV_NV16 },
    { .filename = "rom:/bg2.sprite", .tex_format = FMT_YUV16, .yuv_format = YUV_UYVY },
    { .filename = "rom:/bg3.sprite", .tex_format = FMT_RGBA16 },
    { .filename = "rom:/bg4.sprite", .tex_format = FMT_RGBA32 },
    { .filename = "rom:/bg5.sprite", .tex_format = FMT_YUV16, .yuv_format = YUV_NV12 },
    { .filename = "rom:/bg6.sprite", .tex_format = FMT_YUV16, .yuv_format = YUV_NV12 },
};

static const int BG_COUNT = sizeof(bg_info) / sizeof(bg_info[0]);

int bg_idx = 0;
sprite_t *bg_sprite = NULL;
// Prepare a YUV blitter for the semi-planar background sprite for optimized
// rendering (the rspq block from the YUV setup is recorded once and replayed).
yuv_blitter_t bg_blitter;

void switch_bg(int direction) {
    // The previous frame's blitter block and sprite pixels may still be
    // in flight on the RSP/RDP. Drain before freeing either, otherwise
    // the RSP reads stale data from the freed block and crashes with a
    // bogus "overlay X not registered" assertion.
    rspq_wait();
    if (bg_sprite) {
        sprite_free(bg_sprite);
    }
    if (bg_blitter.block) {
        yuv_blitter_free(&bg_blitter);
    }
    bg_idx = (bg_idx + direction + BG_COUNT) % BG_COUNT;
    debugf("Loading background sprite: %s\n", bg_info[bg_idx].filename);
    bg_sprite = sprite_load(bg_info[bg_idx].filename);
    // Verify that the background sprites were loaded with the expected formats
    assertf(
        sprite_get_format(bg_sprite) == bg_info[bg_idx].tex_format,
        "%s has unexpected format", bg_info[bg_idx].filename
    );
    if (bg_info[bg_idx].tex_format == FMT_YUV16) {
        assertf(
            sprite_get_yuv_format(bg_sprite) == bg_info[bg_idx].yuv_format,
            "%s YUV format mismatch", bg_info[bg_idx].filename
        );
    }
    if (sprite_is_yuv_semiplanar(bg_sprite)) {
        bg_blitter = rdpq_sprite_yuv_blitter_new(bg_sprite, 0, 0, NULL);
    }
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
            if (bg_blitter.block) {
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
