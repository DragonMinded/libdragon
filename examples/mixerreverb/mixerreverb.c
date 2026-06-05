/*
 * mixerreverb — interactive demo of the libdragon mixer's global reverb.
 *
 * Controls:
 *   A           — fire transient (cannon)
 *   B           — toggle reverb master enable
 *   Start       — toggle sustained loop (monosample8)
 *   Z           — clear reverb work area (kills tail instantly)
 *   D-Pad U/D   — cycle preset 0..9
 *   D-Pad L/R   — decrease / increase depth (both channels)
 *   C-Left/Right— stereo split: L-only / balanced / R-only
 */

#include <libdragon.h>
#include <stdio.h>

#define CHANNEL_SFX     0
#define CHANNEL_LOOP    1
#define MIXER_RATE      32000

static const char *PRESET_NAMES[10] = {
    "OFF",
    "ROOM (short, bright)",
    "STUDIO_A (small)",
    "STUDIO_B (medium)",
    "STUDIO_C (medium, dark)",
    "HALL (long, lush)",
    "SPACE (very long)",
    "ECHO (sparse repeats)",
    "PRE-DELAY (low fb)",
    "HALF-ECHO",
};

typedef enum {
    STEREO_BALANCED = 0,
    STEREO_LEFT_ONLY,
    STEREO_RIGHT_ONLY,
} stereo_mode_t;

static const char *STEREO_NAMES[3] = {
    "Balanced (L=R)",
    "Left only",
    "Right only",
};

int main(void)
{
    debug_init_isviewer();
    debug_init_usblog();

    joypad_init();
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE,
                 FILTERS_RESAMPLE);

    int ret = dfs_init(DFS_DEFAULT_LOCATION);
    assert(ret == DFS_ESUCCESS);

    audio_init(MIXER_RATE, 4);
    mixer_init(8);
    wav64_init_compression(3);

    /* Source assets are 48 kHz; allow channels to play them on a 32 kHz mixer
     * (libdragon will resample). The reverb itself runs at the mixer rate. */
    mixer_ch_set_limits(CHANNEL_SFX,  0, 48000, 0);
    mixer_ch_set_limits(CHANNEL_LOOP, 0, 48000, 0);

    mixer_reverb_init(audio_get_frequency());
    mixer_reverb_set_enabled(true);
    mixer_reverb_set_type(4);          /* medium studio, decent default */
    mixer_reverb_set_depth(0.5f, 0.5f);

    wav64_t *sfx_cannon = wav64_load("rom:/cannon.wav64", NULL);
    wav64_t *sfx_loop   = wav64_load("rom:/monosample8.wav64", NULL);
    wav64_set_loop(sfx_loop, true);

    int preset = 4;
    bool enabled = true;
    bool loop_on = false;
    stereo_mode_t stereo = STEREO_BALANCED;

    char line_preset[64];
    char line_depth[64];
    char line_state[64];
    char line_stereo[64];

    int depth_x10 = 5;   /* depth tracked as fixed-point tenths to avoid %.f */

    while (1) {
        /* --- render first (mirrors examples/mixertest pacing) --- */
        sprintf(line_preset, "Preset %d/9: %s", preset, PRESET_NAMES[preset]);
        sprintf(line_depth,  "Depth: 0.%d (%d%%)", depth_x10, depth_x10 * 10);
        sprintf(line_state,  "Reverb: %s   Loop: %s",
                enabled ? "ON " : "OFF",
                loop_on ? "ON " : "OFF");
        sprintf(line_stereo, "Stereo: %s", STEREO_NAMES[stereo]);

        display_context_t disp = display_get();
        graphics_fill_screen(disp, 0);
        graphics_draw_text(disp, 80, 10,  "Mixer Reverb Demo");
        graphics_draw_text(disp, 80, 20,  "----------------");
        graphics_draw_text(disp, 20, 40,  line_preset);
        graphics_draw_text(disp, 20, 50,  line_depth);
        graphics_draw_text(disp, 20, 60,  line_state);
        graphics_draw_text(disp, 20, 70,  line_stereo);
        graphics_draw_text(disp, 20, 100, "A       fire transient");
        graphics_draw_text(disp, 20, 110, "Start   toggle loop");
        graphics_draw_text(disp, 20, 120, "B       toggle reverb");
        graphics_draw_text(disp, 20, 130, "Z       clear work area");
        graphics_draw_text(disp, 20, 140, "D-U/D   cycle preset");
        graphics_draw_text(disp, 20, 150, "D-L/R   adjust depth");
        graphics_draw_text(disp, 20, 160, "C-L/R   stereo split");
        display_show(disp);

        /* --- input --- */
        joypad_poll();
        joypad_buttons_t p1 = joypad_get_buttons_pressed(JOYPAD_PORT_1);

        if (p1.a) {
            wav64_play(sfx_cannon, CHANNEL_SFX);
        }
        if (p1.b) {
            enabled = !enabled;
            mixer_reverb_set_enabled(enabled);
        }
        if (p1.start) {
            loop_on = !loop_on;
            if (loop_on)
                wav64_play(sfx_loop, CHANNEL_LOOP);
            else
                mixer_ch_stop(CHANNEL_LOOP);
        }
        if (p1.z) {
            mixer_reverb_clear_work_area();
        }
        if (p1.d_up) {
            preset = (preset + 1) % 10;
            mixer_reverb_set_type(preset);
        }
        if (p1.d_down) {
            preset = (preset + 9) % 10;
            mixer_reverb_set_type(preset);
        }
        if (p1.d_right && depth_x10 < 10) depth_x10++;
        if (p1.d_left  && depth_x10 > 0)  depth_x10--;
        if (p1.c_left)  stereo = (stereo + 2) % 3;
        if (p1.c_right) stereo = (stereo + 1) % 3;
        if (p1.d_right || p1.d_left || p1.c_left || p1.c_right) {
            float depth = depth_x10 * 0.1f;
            float l = depth, r = depth;
            if (stereo == STEREO_LEFT_ONLY)  r = 0.0f;
            if (stereo == STEREO_RIGHT_ONLY) l = 0.0f;
            mixer_reverb_set_depth(l, r);
        }

        mixer_try_play();
    }
}
