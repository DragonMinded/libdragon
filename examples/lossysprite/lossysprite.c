#include <libdragon.h>

static const char *bg_filenames[] = {
    "rom:/bg0.sprite",
    "rom:/bg1.sprite",
    "rom:/bg2.sprite",
    "rom:/bg3.sprite",
    "rom:/bg4.sprite",
    "rom:/bg5.sprite",
    "rom:/bg6.sprite",
};

static const int BG_COUNT = sizeof(bg_filenames) / sizeof(bg_filenames[0]);

int bg_idx = 0;
sprite_t *bg_sprite = NULL;

static const char *bg_codec_name(const char *fn) {
    // Peek the 8-byte magic to identify the codec on disk.
    FILE *f = fopen(fn, "rb");
    if (!f) return "?";
    char magic[8] = {0};
    fread(magic, 1, sizeof(magic), f);
    fclose(f);
    if (memcmp(magic, "\0\0\0\0LSPR", 8) == 0) return "LSPR";
    if (memcmp(magic, "\0\0\0\0BCSP", 8) == 0) return "BCSP";
    return "RAW ";
}

void switch_bg(int direction) {
    if (bg_sprite) sprite_free(bg_sprite);
    bg_idx = (bg_idx + direction + BG_COUNT) % BG_COUNT;
    const char *fn = bg_filenames[bg_idx];
    const char *codec = bg_codec_name(fn);
    uint64_t t0 = get_ticks_us();
    bg_sprite = sprite_load(fn);
    uint64_t dt = get_ticks_us() - t0;
    debugf("Loaded %s [%s] in %llu us\n", fn, codec, dt);
}

int main(void)
{
    debug_init_emulog();
    debug_init_usblog();
    display_init(RESOLUTION_640x480, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_DISABLED);
    dfs_init(DFS_DEFAULT_LOCATION);
    rdpq_init();
    joypad_init();

    // Register the LSPR and BCSP decoders with sprite_load
    lossysprite_init();
    bcsprite_init();

    // Load the first background sprite
    switch_bg(0);

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
            rdpq_sprite_blit(bg_sprite, 0, 0, NULL);
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
