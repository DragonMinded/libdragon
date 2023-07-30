#include "libdragon.h"
#include <malloc.h>
#include <math.h>

static sprite_t *brew_sprite;
static sprite_t *tiles_sprite;

static rspq_block_t *tiles_block;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;
    float scale_factor;
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
static int32_t cur_tick = 0;
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
        obj->scale_factor = sinf(cur_tick * 0.1f + i) * 0.5f + 1.5f;
    }
    cur_tick++;
}

void render(int cur_frame)
{
    // Attach and clear the screen
    surface_t *disp = display_get();
    rdpq_attach_clear(disp, NULL);

    // Draw the tile background, by playing back the compiled block.
    // This is using copy mode by default, but notice how it can switch
    // to standard mode (aka "1 cycle" in RDP terminology) in a completely
    // transparent way. Even if the block is compiled, the RSP commands within it
    // will adapt its commands to the current render mode, Try uncommenting
    // the line below to see.
    rdpq_debug_log_msg("tiles");
    rdpq_set_mode_copy(false);
    // rdpq_set_mode_standard();
    rspq_block_run(tiles_block);
    
    // Draw the brew sprites. Use standard mode because copy mode cannot handle
    // scaled sprites.
    rdpq_debug_log_msg("sprites");
    rdpq_set_mode_standard();
    rdpq_mode_filter(FILTER_BILINEAR);
    rdpq_mode_alphacompare(1);                // colorkey (draw pixel with alpha >= 1)

    for (uint32_t i = 0; i < num_objs; i++)
    {
        rdpq_sprite_blit(brew_sprite, objects[i].x, objects[i].y, &(rdpq_blitparms_t){
            .scale_x = objects[i].scale_factor, .scale_y = objects[i].scale_factor,
        });
    }

    rdpq_detach_show();
}

int main()
{
    debug_init_isviewer();
    debug_init_usblog();

    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);

    controller_init();
    timer_init();

    uint32_t display_width = display_get_width();
    uint32_t display_height = display_get_height();
    
    dfs_init(DFS_DEFAULT_LOCATION);

    rdpq_init();
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

    // Create a block for the background, so that we can replay it later.
    rspq_block_begin();

    // Check if the sprite was compiled with a paletted format. Normally
    // we should know this beforehand, but for this demo we pretend we don't
    // know. This also shows how rdpq can transparently work in both modes.
    bool tlut = false;
    tex_format_t tiles_format = sprite_get_format(tiles_sprite);
    if (tiles_format == FMT_CI4 || tiles_format == FMT_CI8) {
        // If the sprite is paletted, turn on palette mode and load the
        // palette in TMEM. We use the mode stack for demonstration,
        // so that we show how a block can temporarily change the current
        // render mode, and then restore it at the end.
        rdpq_mode_push();
        rdpq_mode_tlut(TLUT_RGBA16);
        rdpq_tex_upload_tlut(sprite_get_palette(tiles_sprite), 0, 16);
        tlut = true;
    }
    uint32_t tile_width = tiles_sprite->width / tiles_sprite->hslices;
    uint32_t tile_height = tiles_sprite->height / tiles_sprite->vslices;
 
    for (uint32_t ty = 0; ty < display_height; ty += tile_height)
    {
        for (uint32_t tx = 0; tx < display_width; tx += tile_width)
        {
            // Load a random tile among the 4 available in the texture,
            // and draw it as a rectangle.
            // Notice that this code is agnostic to both the texture format
            // and the render mode (standard vs copy), it will work either way.
            int s = RANDN(2)*32, t = RANDN(2)*32;
            rdpq_tex_upload_sub(TILE0, &tiles_surf, NULL, s, t, s+32, t+32);
            rdpq_texture_rectangle(TILE0, tx, ty, tx+32, ty+32, s, t);
        }
    }
    
    // Pop the mode stack if we pushed it before
    if (tlut) rdpq_mode_pop();
    tiles_block = rspq_block_end();

    update(0);
    new_timer(TIMER_TICKS(1000000 / 60), TF_CONTINUOUS, update);

    int cur_frame = 0;
    while (1)
    {
        render(cur_frame);

        controller_scan();
        struct controller_data ckeys = get_keys_down();

        if (ckeys.c[0].C_up && num_objs < NUM_OBJECTS) {
            ++num_objs;
        }

        if (ckeys.c[0].C_down && num_objs > 1) {
            --num_objs;
        }

        cur_frame++;
    }
}
