#include <libdragon.h>

// Mixer channel allocation
#define CHANNEL_SFX1    0
#define CHANNEL_SFX2    1
#define CHANNEL_MUSIC   2

int main(void) {
	debug_init_usblog();
	debug_init_isviewer();
	controller_init();
	display_init(RESOLUTION_512x240, DEPTH_16_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);

	int ret = dfs_init(DFS_DEFAULT_LOCATION);
	assert(ret == DFS_ESUCCESS);

	audio_init(44100, 4);
	mixer_init(16);  // Initialize up to 16 channels

	// Bump maximum frequency of music channel to 128k.
	// The default is the same of the output frequency (44100), but we want to
	// let user increase it.
	mixer_ch_set_limits(CHANNEL_MUSIC, 0, 128000, 0);

	wav64_t sfx_cannon, sfx_laser, sfx_monosample;

	wav64_open(&sfx_cannon, "cannon.wav64");
	
	wav64_open(&sfx_laser, "laser.wav64");
	wav64_set_loop(&sfx_laser, true);

	wav64_open(&sfx_monosample, "monosample8.wav64");
	wav64_set_loop(&sfx_monosample, true);

	bool music = false;
	int music_frequency = sfx_monosample.wave.frequency;

	while (1) {
		display_context_t disp = display_lock();
		graphics_fill_screen(disp, 0);
		graphics_draw_text(disp, 200-75, 10, "Audio mixer test");
		graphics_draw_text(disp, 200-70, 20, "v1.0 - by Rasky");
		graphics_draw_text(disp, 50, 60, "A - Play cannon");
		graphics_draw_text(disp, 50, 70, "B - Play laser (keep pressed)");
		graphics_draw_text(disp, 50, 80, "Z - Start / stop background music");
		graphics_draw_text(disp, 70, 90, "L/R - Change music frequency");
		graphics_draw_text(disp, 50, 140, "Music courtesy of MishtaLu / indiegamemusic.com");
		display_show(disp);

		controller_scan();
		struct controller_data ckeys = get_keys_down();

		if (ckeys.c[0].A) {
			mixer_ch_play(CHANNEL_SFX1, &sfx_cannon.wave);
		}
		if (ckeys.c[0].B) {
			mixer_ch_play(CHANNEL_SFX2, &sfx_laser.wave);
			mixer_ch_set_vol(CHANNEL_SFX2, 0.25f, 0.25f);
		}
		if (ckeys.c[0].Z) {
			music = !music;
			if (music) {
				mixer_ch_play(CHANNEL_MUSIC, &sfx_monosample.wave);
				music_frequency = sfx_monosample.wave.frequency;
			}
			else
				mixer_ch_stop(CHANNEL_MUSIC);
		}
		if (music && music_frequency >= 8000 && ckeys.c[0].L) {
			music_frequency /= 1.1;
			mixer_ch_set_freq(CHANNEL_MUSIC, music_frequency);
		}
		if (music && music_frequency*1.1 <= 128000 && ckeys.c[0].R) {
			music_frequency *= 1.1;
			mixer_ch_set_freq(CHANNEL_MUSIC, music_frequency);
		}

		ckeys = get_keys_up();

		if (ckeys.c[0].B) {
			mixer_ch_stop(CHANNEL_SFX2);
		}

		// Check whether one audio buffer is ready, otherwise wait for next
		// frame to perform mixing.
		if (audio_can_write()) {    	
			short *buf = audio_write_begin();
			mixer_poll(buf, audio_get_buffer_length());
			audio_write_end();
		}
	}
}
