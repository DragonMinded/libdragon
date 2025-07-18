#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

static const int MAX_SPRITES = 4;
static char *sprites_rom[] = {
	"rom:/attack1.sprite",
	"rom:/attack2.sprite",
	"rom:/attack3.sprite",
	"rom:/attack4.sprite",
};

static char *sprites_sd[] = {
	"sd:/attack1.sprite",
	"sd:/attack2.sprite",
	"sd:/attack3.sprite",
	"sd:/attack4.sprite",
};
static bool use_sd = true;

static int cur_sprite = -1;
static sprite_t *sprite;

/* Selects the next sprite and loads it (also frees the previous, if any) */
void load_sprite(int id) {
	if (cur_sprite >= 0) {
		sprite_free(sprite);
	}

	cur_sprite = id;

	if (use_sd)
		sprite = sprite_load(sprites_sd[cur_sprite]);
	else
		sprite = sprite_load(sprites_rom[cur_sprite]);
}

int main(void) {
	/* Initialize peripherals */
	display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);
	dfs_init(DFS_DEFAULT_LOCATION);
	joypad_init();

	/* This will initialize the SD filesystem using 'sd:/' to identify it */
	/* This path has to be the same used by the sprites when loading */
	if (!debug_init_sdfs("sd:/", -1)) {
		debugf("Error opening SD (using rom)\n");
		use_sd = false;
	}

	/* Load the first sprite to start */
	load_sprite(0);

	/* Main loop test */
	while (1) {
		surface_t* disp = display_get();

		/* Clear the screen */
		graphics_fill_screen(disp, 0);

		/* Draw the current loaded sprite (can be from SD or ROM, at this point it doesn't matter) */
		graphics_draw_sprite_trans(disp, 20, 40, sprite);

		/* Draw some help text on screen */
		graphics_draw_text(disp, 20, 20, "Press START to change sprites.");
		if (use_sd)
			graphics_draw_text(disp, 20, 10, "Using SD card for images.");
		else
			graphics_draw_text(disp, 20, 10, "Using ROM cart for images");

		/* Force backbuffer flip */
		display_show(disp);

		/* Do we need to change the sprite? */
		joypad_poll();
		joypad_buttons_t keys = joypad_get_buttons_pressed(JOYPAD_PORT_1);

		if (keys.start) {
			/* Load the next sprite */
			const int id = (cur_sprite + 1) % MAX_SPRITES;
			load_sprite(id);
		}
	}
}
