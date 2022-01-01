#include "libdragon.h"
#include <malloc.h>

static wav64_t sfx_cannon;
static xm64player_t xm;
static sprite_t *sprite;

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

typedef struct {
    uint32_t x;
    uint32_t y;
    int32_t dx;
    int32_t dy;
} object_t;

#define NUM_OBJECTS 64

static object_t objects[NUM_OBJECTS];

// Fair and fast random generation (using xorshift32, with explicit seed)
static uint32_t rand_state = 1;
static uint32_t rand(void) {
	uint32_t x = rand_state;
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 5;
	return rand_state = x;
}

// RANDN(n): generate a random number from 0 to n-1
#define RANDN(n) ({ \
	__builtin_constant_p((n)) ? \
		(rand()%(n)) : \
		(uint32_t)(((uint64_t)rand() * (n)) >> 32); \
})

static uint32_t obj_max_x;
static uint32_t obj_max_y;

static uint32_t num_objs = 1;

void update(int ovfl)
{
    for (uint32_t i = 0; i < NUM_OBJECTS; i++)
    {
        object_t *obj = &objects[i];
        obj->x = (obj->x + obj->dx) % obj_max_x;
        obj->y = (obj->y + obj->dy) % obj_max_y;
    }
}

void render()
{
    if (!rdp_can_attach_display())
    {
        return;
    }

    display_context_t disp = display_lock();
    if (!disp)
    {
        return;
    }

    rdp_attach_display(disp);
    rdp_set_default_clipping();

    rdp_enable_primitive_fill();
    
    double hue = (double)((get_ticks_ms() / 5) % 360);
    hsv color = { .h = hue, .s = 1.0, .v = 1.0 };
    uint32_t fill_color = rgb16(hsv2rgb(color));
    rdp_set_primitive_color(fill_color | (fill_color << 16));
    
    uint32_t display_width = display_get_width();
    uint32_t display_height = display_get_height();
    rdp_draw_filled_rectangle(0, 0, display_width, display_height);

    rdp_sync_pipe();

    rdp_enable_texture_copy();

    for (uint32_t y = 0; y < sprite->vslices; y++)
    {
        for (uint32_t x = 0; x < sprite->hslices; x++)
        {
            rdp_sync_load();
            rdp_load_texture_stride(0, 0, MIRROR_DISABLED, sprite, y*sprite->hslices + x);
            for (uint32_t i = 0; i < num_objs; i++)
            {
                rdp_draw_sprite(0, objects[i].x + x * (sprite->width / sprite->hslices), objects[i].y + y * (sprite->height / sprite->vslices), MIRROR_DISABLED);
            }
        }
    }

    rdp_auto_show_display();
}

int main()
{
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);

    debug_init_isviewer();
    debug_init_usblog();

    controller_init();
    timer_init();

    uint32_t display_width = display_get_width();
    uint32_t display_height = display_get_height();
    
    dfs_init(DFS_DEFAULT_LOCATION);

    audio_init(44100, 4);
    mixer_init(32);

    rdp_init();

    int fp = dfs_open("n64brew.sprite");
    sprite = malloc(dfs_size(fp));
    dfs_read(sprite, 1, dfs_size(fp), fp);
    dfs_close(fp);

    uint32_t obj_min_x = 0;
    uint32_t obj_min_y = 0;
    obj_max_x = display_width - sprite->width;
    obj_max_y = display_height - sprite->height;

    for (uint32_t i = 0; i < NUM_OBJECTS; i++)
    {
        object_t *obj = &objects[i];

        obj->x = obj_min_x + RANDN(obj_max_x - obj_min_x);
        obj->y = obj_min_y + RANDN(obj_max_y - obj_min_y);

        obj->dx = -4 + RANDN(9);
        obj->dy = -4 + RANDN(9);
    }
    
    wav64_open(&sfx_cannon, "cannon.wav64");

    xm64player_open(&xm, "rom:/Caverns16bit.xm64");
    xm64player_play(&xm, 2);

    new_timer(TIMER_TICKS(1000000 / 30), TF_CONTINUOUS, update);

    while (1)
    {
        render();

        controller_scan();
        struct controller_data ckeys = get_keys_down();

        if (ckeys.c[0].A) {
            mixer_ch_play(0, &sfx_cannon.wave);
        }

        if (ckeys.c[0].C_up && num_objs < NUM_OBJECTS) {
            ++num_objs;
        }

        if (ckeys.c[0].C_down && num_objs > 1) {
            --num_objs;
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
