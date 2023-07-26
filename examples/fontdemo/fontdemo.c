#include <libdragon.h>

enum {
    FONT_PACIFICO = 1,
};

int main()
{
    debug_init_isviewer();
    debug_init_usblog();
    controller_init();

    dfs_init(DFS_DEFAULT_LOCATION);
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    rdpq_init();
    // rdpq_debug_start();
    // rdpq_debug_log(true);

    rdpq_font_t *fnt1 = rdpq_font_load("rom:/Pacifico.font64");
    rdpq_font_style(fnt1, 0, &(rdpq_fontstyle_t){
        .color = RGBA32(0xED, 0xAE, 0x49, 0xFF),
    });
    rdpq_text_register_font(FONT_PACIFICO, fnt1);

    // rdpq_font_t *fnt2 = rdpq_font_load("rom:/Acme.font64");
    // rdpq_font_t *fnt3 = rdpq_font_load("rom:/ComicSans.font64");
    // rdpq_font_t *fnt4 = rdpq_font_load("rom:/Videophreak.font64");
    // rdpq_font_t *fnt5 = rdpq_font_load("rom:/ZeroVelocity.font64");
    // rdpq_font_t *fnt6 = rdpq_font_load("rom:/GameMusicLove.font64");
    // rdpq_font_t *fnt7 = rdpq_font_load("rom:/BoxyBold.font64");
    // rdpq_font_t *fnt8 = rdpq_font_load("rom:/ReggaeOne.font64");
    // rdpq_font_t *fnt9 = rdpq_font_load("rom:/FerriteCoreDX.font64");
    // rdpq_font_t *fnt10 = rdpq_font_load("rom:/ArialUnicode.font64");

    const char *text = 
        "Two households, both alike in dignity,\n"
        "In fair Verona, where we lay our scene,\n"
        "From ancient grudge break to new mutiny,\n"
        "Where civil blood makes civil hands unclean.\n"
        "From forth the fatal loins of these two foes\n"
        "A pair of star-cross'd lovers take their life;\n";

    int box_width = 250;
    int box_height = 150;

    int drawn_chars = 0;
    int total_chars = strlen(text);
    int frame = 0;

    rdpq_paragraph_t *partext = rdpq_paragraph_build(&(rdpq_textparms_t){
            .line_spacing = -3,
            .width = box_width,
            .height = box_height,
            .wrap = WRAP_WORD,
        }, FONT_PACIFICO, text, strlen(text));

    while (1) {
        ++frame;
        if ((frame % 4) == 0) {
            ++drawn_chars;
            if (drawn_chars > total_chars) {
                drawn_chars = total_chars;
            }
        }

        controller_scan();
        struct controller_data keys = get_keys_down();
        if (keys.c[0].C_up) { box_height += 2; }
        if (keys.c[0].C_down) { box_height -= 2; }
        if (keys.c[0].C_left) { box_width += 2; }
        if (keys.c[0].C_right) { box_width -= 2; }

        surface_t *screen = display_get();

        rdpq_attach_clear(screen, NULL);
        
        rdpq_set_mode_fill(RGBA32(0x30,0x63,0x8E,0xFF));
        rdpq_fill_rectangle((320-box_width)/2, (240-box_height)/2, (320+box_width)/2, (240+box_height)/2);

        uint32_t t0 = get_ticks();
        // rdpq_text_print(&(rdpq_textparms_t){
        //     .line_spacing = -3,
        //     .width = box_width,
        //     .height = box_height,
        //     .wrap = WRAP_WORD,
        // }, FONT_PACIFICO, (320-box_width)/2, (240-box_height)/2, text);
        rdpq_paragraph_render(partext, (320-box_width)/2, (240-box_height)/2);
        uint32_t t1 = get_ticks();
        debugf("rdpq_text_print: %d us\n", TIMER_MICROS(t1-t0));

#if 0
        rdpq_font_begin(RGBA32(0xED, 0xAE, 0x49, 0xFF));
        rdpq_font_position((320-box_width)/2, (240-box_height)/2);
        rdpq_font_printn(fnt1, text, drawn_chars, &(rdpq_parparms_t){
            .line_spacing = -3,
            .width = box_width,
            .height = box_height,
            .wrap = WRAP_WORD,
        });

        // rdpq_font_print(fnt1, "Jumping over the river");
        // rdpq_font_position(20, 70);
        // rdpq_font_print(fnt2, "Jumping over the river");
        // rdpq_font_position(20, 90);
        // rdpq_font_print(fnt3, "Jumping over the river");
        // rdpq_font_position(20, 110);
        // rdpq_font_print(fnt4, "Jumping over the river");
        // rdpq_font_position(20, 130);
        // rdpq_font_print(fnt5, "Jumping over the river");
        // rdpq_font_position(20, 150);
        // rdpq_font_print(fnt6, "Jumping over the river");
        // rdpq_font_position(20, 170);
        // rdpq_font_print(fnt7, "Jumping over the river");
        // rdpq_font_position(20, 190);
        // rdpq_font_print(fnt8, "Jumping over the river");
        // rdpq_font_position(20, 210);
        // rdpq_font_print(fnt9, "Jumping over the river");
        // rdpq_font_position(20, 230);
        // rdpq_font_print(fnt10, "こうちゃに レモンを いれます。");
        rdpq_font_end();
#endif

        rdpq_detach_show();
    }
}