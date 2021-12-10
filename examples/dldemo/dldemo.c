#include "libdragon.h"

static wav64_t sfx_cannon;
static xm64player_t xm;

static int rdp_intr_genid;
volatile int rdp_intr_done;


void dp_interrupt_handler()
{
    ++rdp_intr_done;
}

void wait_for_rdp()
{
    rdp_sync_full();
    int id = ++rdp_intr_genid;
    MEMORY_BARRIER();
    while (id > rdp_intr_done);
}

typedef struct {
    double r;       // a fraction between 0 and 1
    double g;       // a fraction between 0 and 1
    double b;       // a fraction between 0 and 1
} rgb;

typedef struct {
    double h;       // angle in degrees
    double s;       // a fraction between 0 and 1
    double v;       // a fraction between 0 and 1
} hsv;

rgb hsv2rgb(hsv in);
uint32_t rgb16(rgb in);

int main()
{
    debug_init_isviewer();
    debug_init_usblog();

    controller_init();

    display_init(RESOLUTION_512x240, DEPTH_16_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    
    dfs_init(DFS_DEFAULT_LOCATION);
    
    dl_init();

    audio_init(44100, 4);
    mixer_init(32);

    ugfx_init();

    dl_start();

    set_DP_interrupt(1);
    register_DP_handler(dp_interrupt_handler);
    
    wav64_open(&sfx_cannon, "cannon.wav64");

    xm64player_open(&xm, "rom:/Caverns16bit.xm64");
    xm64player_play(&xm, 2);

    while (1)
    {
        display_context_t disp = display_lock();
        if (disp)
        {
            ugfx_set_display(disp);

            uint32_t display_width = display_get_width();
            uint32_t display_height = display_get_height();
            rdp_set_scissor(0, 0, display_width << 2, display_height << 2);

            rdp_set_other_modes(SOM_CYCLE_FILL);
            
            double hue = (double)((get_ticks_ms() / 5) % 360);
            hsv color = { .h = hue, .s = 1.0, .v = 1.0 };
            uint32_t fill_color = rgb16(hsv2rgb(color));
            rdp_set_fill_color(fill_color | (fill_color << 16));
            
            rdp_fill_rectangle(0, 0, display_width << 2, display_height << 2);

            wait_for_rdp();
            display_show(disp);
        }

        controller_scan();
        struct controller_data ckeys = get_keys_down();

        if (ckeys.c[0].A) {
            mixer_ch_play(0, &sfx_cannon.wave);
        }

        if (audio_can_write()) {    	
            short *buf = audio_write_begin();
            mixer_poll(buf, audio_get_buffer_length());
            audio_write_end();
        }
    }
}

// https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
rgb hsv2rgb(hsv in)
{
    double      hh, p, q, t, ff;
    long        i;
    rgb         out;

    if(in.s <= 0.0) {       // < is bogus, just shuts up warnings
        out.r = in.v;
        out.g = in.v;
        out.b = in.v;
        return out;
    }
    hh = in.h;
    if(hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = in.v * (1.0 - in.s);
    q = in.v * (1.0 - (in.s * ff));
    t = in.v * (1.0 - (in.s * (1.0 - ff)));

    switch(i) {
    case 0:
        out.r = in.v;
        out.g = t;
        out.b = p;
        break;
    case 1:
        out.r = q;
        out.g = in.v;
        out.b = p;
        break;
    case 2:
        out.r = p;
        out.g = in.v;
        out.b = t;
        break;

    case 3:
        out.r = p;
        out.g = q;
        out.b = in.v;
        break;
    case 4:
        out.r = t;
        out.g = p;
        out.b = in.v;
        break;
    case 5:
    default:
        out.r = in.v;
        out.g = p;
        out.b = q;
        break;
    }
    return out;     
}

uint32_t rgb16(rgb in)
{
    return RDP_COLOR16(
        ((uint32_t)(in.r * 31) & 0x1F),
        ((uint32_t)(in.g * 31) & 0x1F),
        ((uint32_t)(in.b * 31) & 0x1F),
        1);
}
