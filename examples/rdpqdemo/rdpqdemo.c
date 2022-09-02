#include "libdragon.h"
#include <malloc.h>

static wav64_t sfx_cannon;
static xm64player_t xm;
static sprite_t *brew_sprite;
static sprite_t *tiles_sprite;

static rspq_block_t *tiles_block;

typedef struct {
    int32_t x;
    int32_t y;
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

static int32_t obj_max_x;
static int32_t obj_max_y;

static uint32_t num_objs = 1;

void update(int ovfl)
{
    for (uint32_t i = 0; i < NUM_OBJECTS; i++)
    {
        object_t *obj = &objects[i];

        int32_t x = obj->x + obj->dx;
        int32_t y = obj->y + obj->dy;

        if (x >= obj_max_x) x -= obj_max_x;
        if (x < 0) x += obj_max_x;
        if (y >= obj_max_y) y -= obj_max_y;
        if (y < 0) y += obj_max_y;
        
        obj->x = x;
        obj->y = y;
    }
}

void render()
{
    surface_t *disp = display_lock();
    if (!disp)
    {
        return;
    }

    rdp_attach(disp);
    
    rdpq_set_mode_copy(true);

    rspq_block_run(tiles_block);
    
    for (uint32_t i = 0; i < num_objs; i++)
    {
        uint32_t obj_x = objects[i].x;
        uint32_t obj_y = objects[i].y;
        for (uint32_t y = 0; y < brew_sprite->vslices; y++)
        {
            for (uint32_t x = 0; x < brew_sprite->hslices; x++)
            {
                rdp_load_texture_stride(0, 0, MIRROR_DISABLED, brew_sprite, y*brew_sprite->hslices + x);
                rdp_draw_sprite(0, obj_x + x * (brew_sprite->width / brew_sprite->hslices), obj_y + y * (brew_sprite->height / brew_sprite->vslices), MIRROR_DISABLED);
            }
        }
    }

    rdp_detach_show(disp);
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
    rdpq_debug_start();

    brew_sprite = sprite_load("rom:/n64brew.sprite");

    obj_max_x = display_width - brew_sprite->width;
    obj_max_y = display_height - brew_sprite->height;

    for (uint32_t i = 0; i < NUM_OBJECTS; i++)
    {
        object_t *obj = &objects[i];

        obj->x = RANDN(obj_max_x);
        obj->y = RANDN(obj_max_y);

        obj->dx = -3 + RANDN(7);
        obj->dy = -3 + RANDN(7);
    }

    tiles_sprite = sprite_load("rom:/tiles.sprite");

    surface_t tiles_surf = sprite_get_pixels(tiles_sprite);

    rspq_block_begin();

    // Enable palette mode and load palette into TMEM
    if (tiles_sprite->format == FMT_CI4 || tiles_sprite->format == FMT_CI8) {
        rdpq_mode_tlut(TLUT_RGBA16);
        rdpq_tex_load_tlut(sprite_get_palette(tiles_sprite), 0, 16);
    }

    uint32_t tile_width = tiles_sprite->width / tiles_sprite->hslices;
    uint32_t tile_height = tiles_sprite->height / tiles_sprite->vslices;

    for (uint32_t ty = 0; ty < display_height; ty += tile_height)
    {
        for (uint32_t tx = 0; tx < display_width; tx += tile_width)
        {
            int s = RANDN(2)*32, t = RANDN(2)*32;
            rdpq_tex_load_sub(TILE0, &tiles_surf, 0, s, t, s+32, t+32);
            rdpq_texture_rectangle(TILE0, tx, ty, tx+32, ty+32, s, t, 1, 1);
        }
    }

    rdpq_mode_tlut(TLUT_NONE);    
    tiles_block = rspq_block_end();
    
    
    wav64_open(&sfx_cannon, "cannon.wav64");

    xm64player_open(&xm, "rom:/Caverns16bit.xm64");
    xm64player_play(&xm, 2);

    new_timer(TIMER_TICKS(1000000 / 60), TF_CONTINUOUS, update);

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
