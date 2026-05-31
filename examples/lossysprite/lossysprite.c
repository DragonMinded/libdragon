#include <libdragon.h>

typedef enum {
    FMT_RAW   = 0,
    FMT_BC1Q  = 1,
    FMT_H264I = 2,
    FMT_COUNT
} fmt_t;

static const char *fmt_name[FMT_COUNT]   = { "LOSSLESS",  "BC1Q", "H264I" };
static const char *fmt_suffix[FMT_COUNT] = { "raw",  "bc1q", "h264i" };

#define BG_COUNT 7
#define FONT_ID  1

static int bg_idx = 0;
static fmt_t fmt_idx = FMT_BC1Q;
static sprite_t *bg_sprite = NULL;
static uint64_t last_load_us = 0;
static size_t last_encoded_bytes = 0;
static size_t last_raw_bytes = 0;

static void load_bg(void) {
    if (bg_sprite) sprite_free(bg_sprite);
    char fn[64];
    snprintf(fn, sizeof(fn), "rom:/bg%d_%s.sprite", bg_idx, fmt_suffix[fmt_idx]);

    // Stat the on-disk size so the corner readout can show a compression
    // ratio relative to the uncompressed RGBA16 footprint.
    last_encoded_bytes = 0;
    FILE *f = fopen(fn, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        last_encoded_bytes = (size_t)ftell(f);
        fclose(f);
    }

    uint64_t t0 = get_ticks_us();
    bg_sprite = sprite_load(fn);
    last_load_us = get_ticks_us() - t0;
    last_raw_bytes = (size_t)bg_sprite->width * bg_sprite->height * 2;

    float pct = last_raw_bytes
        ? 100.0f * (float)last_encoded_bytes / (float)last_raw_bytes : 0.0f;
    debugf("Loaded %s [%s] %u B (%.1f%% of raw) in %llu us\n",
        fn, fmt_name[fmt_idx], (unsigned)last_encoded_bytes, pct, last_load_us);
}

int main(void)
{
    debug_init_emulog();
    debug_init_usblog();
    display_init(RESOLUTION_640x480, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_DISABLED);
    dfs_init(DFS_DEFAULT_LOCATION);
    rdpq_init();
    joypad_init();

    // Register the Level 3 (H264I) and Level 1 (BC1Q) lossy-sprite decoders
    lspr3_init();
    lspr1_init();

    // Font for the on-screen codec/timing readout
    rdpq_text_register_font(FONT_ID, rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO));

    // Load the initial background sprite
    load_bg();

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
            rdpq_set_mode_standard();
            // Draw the background
            rdpq_sprite_blit(bg_sprite, 0, 0, NULL);
            // Draw the brew sprite
            rdpq_mode_alphacompare(1); // colorkey (draw pixel with alpha >= 1)
            rdpq_sprite_blit(brew_sprite, x, y, NULL);

            // Codec readout in the top-left corner: shows active format,
            // last decode time, encoded size, and encoded-as-percent of
            // the uncompressed RGBA16 footprint.
            float pct = last_raw_bytes
                ? 100.0f * (float)last_encoded_bytes / (float)last_raw_bytes : 0.0f;
            rdpq_text_printf(NULL, FONT_ID, 12, 20,
                "bg%d  %s\n%u B  %.1f%%\n%llu us",
                bg_idx, fmt_name[fmt_idx],
                (unsigned)last_encoded_bytes, pct,
                last_load_us
            );
        }
        rdpq_detach_show();

        // L/R or A/B cycle backgrounds; C-Left/Right cycle codecs.
        joypad_poll();
        joypad_buttons_t btn = joypad_get_buttons_pressed(JOYPAD_PORT_1);
        bool reload = false;
        if (btn.r || btn.a)         { bg_idx = (bg_idx + 1) % BG_COUNT;             reload = true; }
        if (btn.l || btn.z || btn.b){ bg_idx = (bg_idx - 1 + BG_COUNT) % BG_COUNT;  reload = true; }
        if (btn.c_right)            { fmt_idx = (fmt_idx + 1) % FMT_COUNT;          reload = true; }
        if (btn.c_left)             { fmt_idx = (fmt_idx - 1 + FMT_COUNT) % FMT_COUNT; reload = true; }
        if (reload) load_bg();
    }
}
