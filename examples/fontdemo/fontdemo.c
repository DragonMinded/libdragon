#include <libdragon.h>

#define MAX(a,b) ((a)>(b)?(a):(b))

enum {
    FONT_PACIFICO = 1,
    FONT_ZEROVELOCITY = 2,
};


float measure(void (*func)(int), int arg)
{
    uint64_t samples = 0;

    for (int i=0; i<16; i++) {
        rspq_wait();
        disable_interrupts();
        uint32_t t0 = get_ticks();
        func(arg);
        uint32_t t1 = get_ticks();
        enable_interrupts();
        if (i > 0) // ignore first sample (cache warmup)
            samples += t1-t0;
    }
    return TIMER_MICROS(samples) / 16.0f;
}

void run_benchmark(void)
{
    const char *text = 
        "Two households, both alike in dignity,\n"
        "In fair Verona, where we lay our scene,\n"
        "From ancient grudge break to new mutiny,\n"
        "Where civil blood makes civil hands unclean.\n"
        "From forth the fatal loins of these two foes\n"
        "A pair of star-cross'd lovers take their life;\n";
    int len = strlen(text);
    int sizes[7] = { 4, 8, 16, 32, 64, 128, len };
    rdpq_paragraph_t *partext = NULL;

    void text_noformat(int nchar)
    {
        rdpq_text_printn(NULL, FONT_PACIFICO, 10, 10, text, nchar);
    }

    void text_format(int nchar)
    {
        rdpq_text_printn(&(rdpq_textparms_t){
            .line_spacing = -3,
            .width = 200,
            .wrap = WRAP_WORD,
        }, FONT_PACIFICO, 10, 10, text, nchar);
    }

    void text_render(int nchar)
    {
        rdpq_paragraph_render(partext, 10, 10);
    }

    for (int i=0; i<7; i++) {
        int nchar = sizes[i]; float t = measure(text_noformat, nchar);
        debugf("text_noformat(%d): %d us\n", nchar, (int)(t+0.5f));
    }
    for (int i=0; i<7; i++) {
        int nchar = sizes[i]; float t = measure(text_format, nchar);
        debugf("text_wordwrap(%d): %d us\n", nchar, (int)(t+0.5f));
    }
    for (int i=0; i<7; i++) {
        int nchar = sizes[i];
        partext = rdpq_paragraph_build(&(rdpq_textparms_t){
                .line_spacing = -3,
                .width = 200,
                .wrap = WRAP_WORD,
            }, FONT_PACIFICO, text, &nchar);
        float t = measure(text_render, nchar);
        debugf("text_render(%d): %d us\n", nchar, (int)(t+0.5f));
        rdpq_paragraph_free(partext); partext = NULL;
    }
}

int main()
{
    debug_init_isviewer();
    debug_init_usblog();
    joypad_init();

    dfs_init(DFS_DEFAULT_LOCATION);
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    // rdpq_debug_start();

    rdpq_font_t *fnt1 = rdpq_font_load("rom:/Pacifico.font64");
    rdpq_font_t *fnt5 = rdpq_font_load("rom:/FerriteCoreDX.font64");
    rdpq_font_style(fnt1, 0, &(rdpq_fontstyle_t){
        .color = RGBA32(0xED, 0xAE, 0x49, 0xFF),
    });
    rdpq_font_style(fnt5, 0, &(rdpq_fontstyle_t){
        .color = RGBA32(0x82, 0x30, 0x38, 0xFF),
    });
    rdpq_text_register_font(FONT_PACIFICO, fnt1);
    rdpq_text_register_font(FONT_ZEROVELOCITY, fnt5);

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
        "Two $02households$01, both alike in dignity,\n"
        "In $02fair Verona$01, where we lay our scene,\n"
        "From ancient grudge break to new $02mutiny$01,\n"
        "Where $02civil blood$01 makes civil hands unclean.\n"
        "From forth the fatal loins of these $02two foes$01\n"
        "A pair of $02star-cross'd lovers$01 take their life;\n";

    int box_width = 262;
    int box_height = 150;

    int drawn_chars = 0;
    int total_chars = strlen(text);
    int frame = 0;

    while (1) {
        ++frame;
        if ((frame % 4) == 0) {
            ++drawn_chars;
            if (drawn_chars > total_chars) {
                drawn_chars = total_chars;
            }
        }

        joypad_poll();
        joypad_buttons_t keys = joypad_get_buttons_held(JOYPAD_PORT_1);
        if (keys.c_up) { box_height += 2; }
        if (keys.c_down) { box_height -= 2; }
        if (keys.c_left) { box_width += 2; }
        if (keys.c_right) { box_width -= 2; }
        box_height = MAX(1, box_height);
        box_width = MAX(1, box_width);

        surface_t *screen = display_get();

        rdpq_attach_clear(screen, NULL);
        
        rdpq_set_mode_fill(RGBA32(0x30,0x63,0x8E,0xFF));
        rdpq_fill_rectangle((320-box_width)/2, (240-box_height)/2, (320+box_width)/2, (240+box_height)/2);

        // rspq_wait();
        disable_interrupts();
        uint32_t t0 = get_ticks();

        int x0 = (320-box_width)/2;
        int y0 = (240-box_height)/2;   

        int nbytes = strlen(text);
        rdpq_paragraph_t* par = rdpq_paragraph_build(&(rdpq_textparms_t){
            // .line_spacing = -3,
            .align = ALIGN_LEFT,
            .valign = VALIGN_CENTER,
            .width = box_width,
            .height = box_height,
            .wrap = WRAP_WORD,
        }, FONT_PACIFICO, text, &nbytes);

        rdpq_paragraph_render(par, x0, y0);
        rdpq_paragraph_free(par);

        uint32_t t1 = get_ticks();
        enable_interrupts();
        debugf("rdpq_text_print: %d us (%dx%d) - %d bytes\n", TIMER_MICROS(t1-t0), box_width, box_height, nbytes);

        rdpq_set_mode_standard();
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_set_prim_color(RGBA32(0xFF,0xFF,0xFF,0x30));
        rdpq_fill_rectangle(par->bbox.x0+x0, par->bbox.y0+y0, par->bbox.x1+x0, par->bbox.y1+y0);

        rdpq_detach_show();
    }
}