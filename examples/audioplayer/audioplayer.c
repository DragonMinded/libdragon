#include <libdragon.h>
#include <stdio.h>

// We need to show lots of internal details of the module which are not
// exposed via public API, so include the internal header file.
#include "../../src/audio/libxm/xm_internal.h"
#include "../../src/audio/mixer_internal.h"
#include "../../src/accounting_internal.h"

#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

enum Page {
	PAGE_INTRO = 0,
	PAGE_MENU = 1,
	PAGE_SONG = 2,
	PAGE_INTRO_ERROR = 3,
};

char *cur_rom = NULL;
bool mute[32] = {0};
int chselect = 0;

int menu_sel = 0;

static char* songfiles[4096];
static int num_songs = 0;

static char* bankfiles[64];
static int num_banks = 0;
static int bank_sel = 0;

/** Mixer channels reserved for SF64 polyphony when playing MID64. */
#define MID_VOICES  24

static sf64_bank_t *g_sf64_bank;

/** Filename without "rom:/" prefix and extension (writes into @p out). */
static void asset_basename(const char *path, char *out, int out_sz) {
	strlcpy(out, path + 5, out_sz);
	char *dot = strrchr(out, '.');
	if (dot) *dot = '\0';
}

static void draw_header(display_context_t disp) {
	graphics_draw_text(disp, 200-70, 10, "XM/YM/MID Audio Player");
	graphics_draw_text(disp, 200-45, 20, "v2.1 - by Rasky");
}

static bool strendswith(const char *str, const char *suffix) {
	char *p = strstr(str, suffix);
	return p && p[strlen(suffix)] == '\0';
}

static int wordlen(const char * str) {
	int i=0;
	while (str[i]!=' ' && str[i]!=0 && str[i]!='\n') i++;
	return i;
}

static void wrap(char * s, const int wrapline) {
	int i=0;
	int curlen = 0;
	while (s[i] != '\0') {
		if (s[i] == '\n') {
			curlen=0;
		} else if (s[i] == ' ') {
			if (curlen+wordlen(&s[i+1]) >= wrapline) {
				s[i] = '\n';
				curlen = 0;
			}
		}
		curlen++;
		i++;
	}
}

enum Page page_intro(void) {
	display_context_t disp = display_get();
	graphics_fill_screen(disp, 0);
	draw_header(disp);

	graphics_draw_text(disp, 30, 50, "This player plays .XM/.YM modules and .MID songs,");
	graphics_draw_text(disp, 30, 58, "up to 32 channels and 48Khz, using an optimized");
	graphics_draw_text(disp, 30, 66, "engine that uses little CPU and RSP time. ");

	graphics_draw_text(disp, 30, 80, "Files are converted with audioconv64 into XM64,");
	graphics_draw_text(disp, 30, 88, "YM64, or MID64. MIDI synthesis uses SF64 banks");
	graphics_draw_text(disp, 30, 96, "(default: florestan-full). Press Z to cycle banks.");

	graphics_draw_text(disp, 30,112, "Most sample data is streamed from ROM, so RDRAM");
	graphics_draw_text(disp, 30,120, "usage stays modest even with large banks.");

	graphics_draw_text(disp, 30,144, "Press START to begin!");

	display_show(disp);

	while (1) {
		joypad_poll();
		joypad_buttons_t ckeys = joypad_get_buttons_pressed(JOYPAD_PORT_1);

		if (ckeys.start) {
			return PAGE_MENU;
		}
	}
}

enum Page page_intro_error(void) {
	display_context_t disp = display_get();
	graphics_fill_screen(disp, 0);
	draw_header(disp);
	graphics_draw_text(disp, 40, 50, "No XM64/YM64/MID64 files in the filesystem");
	display_show(disp);
	abort();
}

enum Page page_menu(void) {
	char sbuf[1024];
	display_context_t disp = display_get();
	graphics_fill_screen(disp, 0);
	draw_header(disp);

	#define NUM_COLUMNS 3
	#define COL_ROWS    18
	#define HMARGIN     30
	#define YSTART      40

	menu_sel = CLAMP(menu_sel, 0, num_songs-1);

	int total_cols = (num_songs + COL_ROWS - 1) / COL_ROWS;
	int last_col_rows = num_songs - (total_cols-1)*COL_ROWS;

	int first_col = (menu_sel / COL_ROWS / NUM_COLUMNS) * NUM_COLUMNS;

	for (int j=first_col;j<first_col+NUM_COLUMNS;j++) {
		if (j == total_cols) break;

		int col_start = j*COL_ROWS;
		int x = HMARGIN + (j-first_col)*((512 - HMARGIN*2) / NUM_COLUMNS);
		int y = YSTART;

		for (int i=0;i<COL_ROWS;i++) {
			if (j == total_cols-1 && i == last_col_rows) break;

			sprintf(sbuf, "%s", songfiles[col_start+i]+5);

			sbuf[17] = '\0';
			int c = strlen(sbuf);
			while (--c >= 0) if (sbuf[c] == '.') break;
			if (c >= 0) sbuf[c] = '\0';

			if (i == menu_sel % COL_ROWS && j == menu_sel / COL_ROWS)
				graphics_draw_box(disp, x-2, y-1, 4+17*8, 9, 0x003300);

			graphics_draw_text(disp, x, y, sbuf);
			y += 10;
		}
	}

	sprintf(sbuf, "Page %d/%d", first_col/NUM_COLUMNS+1, total_cols/NUM_COLUMNS+1);
	graphics_draw_text(disp, 190, 225, sbuf);

	display_show(disp);

	while (1) {
		joypad_poll();
		joypad_buttons_t ckeys = joypad_get_buttons_pressed(JOYPAD_PORT_1);

		if (ckeys.d_up)      { menu_sel -= 1; break; }
		if (ckeys.d_down)    { menu_sel += 1; break; }
		if (ckeys.d_left)    { menu_sel -= COL_ROWS; break; }
		if (ckeys.d_right)   { menu_sel += COL_ROWS; break; }
		if (ckeys.c_up)    { menu_sel = 0; break; }
		if (ckeys.c_down)  { menu_sel = num_songs-1; break; }
		if (ckeys.c_left)  { menu_sel -= COL_ROWS*NUM_COLUMNS; break; }
		if (ckeys.c_right) { menu_sel += COL_ROWS*NUM_COLUMNS; break; }

		if (ckeys.a) {
			cur_rom = songfiles[menu_sel];
			chselect = 0;
			return PAGE_SONG;
		}
	}

	return PAGE_MENU;
}

enum Page page_song(void) {
	char sbuf[1024];
	int64_t tot_time = 0, tot_cpu = 0, tot_rsp = 0, tot_dma = 0, tot_frames = 0;
	int64_t avg_time = 0, avg_cpu = 0, avg_rsp = 0, avg_dma = 0;
	int poly_max = 0, poly_disp = 0;
	uint32_t poly_win = TICKS_READ();
	int screen_first_inst = 0;
	enum SONG_TYPE { SONG_XM, SONG_YM, SONG_MID };

	xm64player_t xm;
	ym64player_t ym; ym64player_songinfo_t yminfo;
	mid64player_t *mid = NULL;
	sf64_synth_t *synth = NULL;
	enum SONG_TYPE song_type;
	const char *song_name; int song_channels;
	int song_romsz=0, song_ramsz=0;
	static char mid_name[64];

	if (strendswith(cur_rom, ".ym64") || strendswith(cur_rom, ".YM64"))
		song_type = SONG_YM;
	else if (strendswith(cur_rom, ".mid64") || strendswith(cur_rom, ".MID64"))
		song_type = SONG_MID;
	else
		song_type = SONG_XM;

	{
		int fh = dfs_open(cur_rom+5);
		song_romsz = dfs_size(fh);
		dfs_close(fh);
	}

	debugf("Loading %s\n", cur_rom);
	if (song_type == SONG_XM) {
		xm64player_open(&xm, cur_rom);
		xm64player_play(&xm, 0);
		song_name = xm_get_module_name(xm.ctx);
		song_channels = xm64player_num_channels(&xm);	
		#if 0
		// Seek to a specific position in the song
		// Isolate a specific channel
		xm64player_seek(&xm, 8, 0, 0);
		// for (int i=0;i<32;i++) {
		// 	if (i != 0) {
		// 		mute[i] = 1;
		// 		xm_mute_channel(xm.ctx, i+1, 1);
		// 	}
		// }
		#endif

		song_ramsz = sizeof(xm64player_t) + xm.ctx->ctx_size;
		#if XM_STREAM_PATTERNS
		song_ramsz -= xm.ctx->ctx_size_all_patterns;
		song_ramsz += xm.ctx->ctx_size_stream_pattern_buf;
		#endif
		#if XM_STREAM_WAVEFORMS
		song_ramsz -= xm.ctx->ctx_size_all_samples;
		song_ramsz += xm.stream_ramsz;
		#endif
	} else if (song_type == SONG_YM) {
		ym64player_open(&ym, cur_rom, &yminfo);
		ym64player_play(&ym, 0);
		song_name = yminfo.name;
		song_channels = 3;
		wrap(yminfo.comment, 40);
		song_ramsz = sizeof(ym64player_t);
		if (ym.decoder) song_ramsz += sizeof(*ym.decoder);
	} else {
		assertf(g_sf64_bank, "MID64: SF64 bank not loaded");
		synth = sf64_synth_create(g_sf64_bank);
		sf64_synth_set_channels(synth, 0, MID_VOICES, MIXER_PRIORITY_MUSIC);
		for (int i = 0; i < MID_VOICES; i++)
			mixer_ch_set_limits(i, 0, 96000, 0);

		mid = mid64player_load(cur_rom);
		mid64player_set_loop(mid, true);
		mid64player_play(mid, sf64_synth_midi_target(synth));

		asset_basename(cur_rom, mid_name, sizeof(mid_name));
		song_name = mid_name;
		song_channels = SF64_MIDI_CHANNELS;
		// Event stream is fully resident; samples stream from the SF64 bank.
		song_ramsz = song_romsz;
	}

	// Unmute all channels
	memset(mute, 0, sizeof(mute));

	while (true) {
		display_context_t disp = display_get();
		graphics_fill_screen(disp, 0);
		draw_header(disp);

		sprintf(sbuf, "Filename: %s", cur_rom+5);
		graphics_draw_text(disp, 20, 40, sbuf);

		sprintf(sbuf, "Song: %s", song_name);
		graphics_draw_text(disp, 20, 50, sbuf);

		if (song_type == SONG_MID)
			sprintf(sbuf, "MIDI channels: %d", song_channels);
		else
			sprintf(sbuf, "Channels: %d", song_channels);
		graphics_draw_text(disp, 20, 60, sbuf);

		sprintf(sbuf, "ROM: %d KiB | RDRAM: %d KiB", (song_romsz+512)/1024, (song_ramsz+512)/1024);
		graphics_draw_text(disp, 20, 70, sbuf);

		if (song_type == SONG_XM) {
			xm_pattern_t* pat = xm.ctx->module.patterns + xm.ctx->module.pattern_table[xm.ctx->current_table_index];
			int pos, row;
			xm64player_tell(&xm, &pos, &row, NULL);
			sprintf(sbuf, "Pos: %02x/%02x Row: %02x/%02x\n", pos, xm_get_module_length(xm.ctx), row, pat->num_rows);
			graphics_draw_text(disp, 280, 50, sbuf);			
		} else if (song_type == SONG_YM) {
			int pos, len;
			ym64player_duration(&ym, &len, NULL);
			ym64player_tell(&ym, &pos, NULL);
			sprintf(sbuf, "Pos: %04x/%04x\n", pos, len);
			graphics_draw_text(disp, 280, 50, sbuf);						
		} else if (song_type == SONG_MID) {
			uint32_t cur_ms = mid64player_tell_ms(mid);
			uint32_t tot_ms = mid64player_get_duration_ms(mid);
			sprintf(sbuf, "Time: %ld:%02ld / %ld:%02ld\n",
				(long)(cur_ms / 1000 / 60), (long)((cur_ms / 1000) % 60),
				(long)(tot_ms / 1000 / 60), (long)((tot_ms / 1000) % 60));
			graphics_draw_text(disp, 280, 50, sbuf);
		}

		sprintf(sbuf, "CPU: %lldus\n", avg_time);
		graphics_draw_text(disp, 280, 60, sbuf);
		sprintf(sbuf, "Wait: DMA: %lldus RSP: %lldus", avg_dma, avg_rsp);
		graphics_draw_text(disp, 280, 70, sbuf);

		for (int i=0; i<32; i++) {
			if (i == song_channels) break;
			int x = 50+(i%16)*24, y = 90+10*(i/16);
			if (i == chselect)
				graphics_draw_box(disp, x-2, y-1, 16+2+2, 9, 0x003300);
			sprintf(sbuf, "%02d", i+1);
			graphics_draw_text(disp, x, y, sbuf);
			if (mute[i])
				graphics_draw_box(disp, x-2, y+3, 16+2+2, 2, 0x0000FF00);
		}

		if (song_type == SONG_XM) {
			// Traditionally, XM songs have their "comments" in the instrument
			// names (nobody use the instrument names as... instrument names).
			// So display those on the screen, and also allow for some scrolling
			// as they could be many lines.
			for (int i=0; i<11; i++) {
				if (screen_first_inst + i >= xm.ctx->module.num_instruments)
					break;
				graphics_draw_text(disp, 120, 120+i*10, xm.ctx->module.instruments[screen_first_inst+i].name);
			}
		} else if (song_type == SONG_YM) {
			// Display the YM song information (author and comment).
			sprintf(sbuf, "Author: %s", yminfo.author);
			graphics_draw_text(disp, 120, 120, sbuf);

			// Comment can be multiline.
			strlcpy(sbuf, yminfo.comment, sizeof(sbuf));
			char *line = strtok(sbuf, "\n");
			int ypos = 130;
			while (line) {
				graphics_draw_text(disp, 120, ypos, line);
				ypos += 10;
				line = strtok(NULL, "\n");
			}
		} else {
			char bank_name[64];
			asset_basename(bankfiles[bank_sel], bank_name, sizeof(bank_name));
			sprintf(sbuf, num_banks > 1 ? "Bank: %s  (Z: next)" : "Bank: %s", bank_name);
			graphics_draw_text(disp, 120, 120, sbuf);
			sprintf(sbuf, "PPQN: %d", mid64player_get_ppqn(mid));
			graphics_draw_text(disp, 120, 130, sbuf);
			sprintf(sbuf, "Tempo: %ld us/qn", (long)mid64player_get_tempo(mid));
			graphics_draw_text(disp, 120, 140, sbuf);
			sprintf(sbuf, "Duration: %ld ticks", (long)mid64player_get_duration_ticks(mid));
			graphics_draw_text(disp, 120, 150, sbuf);
			int poly = 0;
			for (int i = 0; i < MID_VOICES; i++)
				if (mixer_ch_playing(i)) poly++;
			if (poly > poly_max) poly_max = poly;
			if (TICKS_DISTANCE(poly_win, TICKS_READ()) >= TICKS_PER_SECOND / 2) {
				poly_disp = poly_max;
				poly_max = 0;
				poly_win = TICKS_READ();
			}
			sprintf(sbuf, "Polyphony: %d / %d", poly_disp, MID_VOICES);
			graphics_draw_text(disp, 120, 160, sbuf);
		}

		display_show(disp);
		profile_next_frame();

		uint32_t start_play_loop = TICKS_READ();
		//int audiosz = audio_get_buffer_length();
		//while (TICKS_DISTANCE(start_play_loop, TICKS_READ()) < TICKS_PER_SECOND)
		do
		{
			uint32_t t1t = get_ticks_us();
			uint32_t t1u = get_user_ticks();
			uint64_t t1rsp = acct_get_ticks(ACCT_CAT_RSP) + acct_get_ticks(ACCT_CAT_RSPQ);
			uint64_t t1dma = acct_get_ticks(ACCT_CAT_PI);

			mixer_try_play();

			uint32_t t2t = get_ticks_us();
			uint32_t t2u = get_user_ticks();
			uint64_t t2rsp = acct_get_ticks(ACCT_CAT_RSP) + acct_get_ticks(ACCT_CAT_RSPQ);
			uint64_t t2dma = acct_get_ticks(ACCT_CAT_PI);

			tot_dma += TICKS_TO_US(t2dma-t1dma);
			tot_rsp += TICKS_TO_US(t2rsp-t1rsp);
			tot_cpu += TICKS_TO_US(t2u-t1u);
			tot_time += t2t-t1t;
			tot_frames++;
			if (tot_frames >= 60) {
				avg_time = tot_time / tot_frames;
				avg_cpu = tot_cpu / tot_frames;
				avg_rsp = tot_rsp / tot_frames;
				avg_dma = tot_dma / tot_frames;
				tot_time = 0, tot_cpu = 0, tot_rsp = 0, tot_dma = 0, tot_frames = 0;
				debugf("CPU: %lldus | Wait DMA: %lldus | Wait RSP: %lldus\n", avg_time, avg_dma, avg_rsp);
			}

			joypad_poll();
			joypad_buttons_t ckeys = joypad_get_buttons_pressed(JOYPAD_PORT_1);
			if (ckeys.d_left || ckeys.d_right) {
				if (song_type == SONG_XM) {				
					int patidx;
					xm64player_tell(&xm, &patidx, NULL, NULL);
					if (ckeys.d_left && patidx > 0) patidx--;
					if (ckeys.d_right && patidx < xm_get_module_length(xm.ctx)-1) patidx++;
					xm64player_seek(&xm, patidx, 0, 0);
					break;
				} else if (song_type == SONG_YM && !ym.decoder) {
					int pos, len;
					ym64player_duration(&ym, &len, NULL);
					ym64player_tell(&ym, &pos, NULL);
					if (ckeys.d_left && pos >= 0x200) pos -= 0x200;
					if (ckeys.d_right && pos <= len-0x200) pos += 0x200;
					ym64player_seek(&ym, pos);
					break;
				}
			}

			if (song_type == SONG_XM) {			
				if (ckeys.d_up && screen_first_inst > 0) {
					screen_first_inst--;
					break;
				}
				if (ckeys.d_down && screen_first_inst < xm.ctx->module.num_instruments-1) {
					screen_first_inst++;
					break;
				}
			}

			if (ckeys.c_left && chselect > 0) { chselect--; break; }
			if (ckeys.c_right && chselect < song_channels-1) { chselect++; break; }
			if (ckeys.c_down) {
				mute[chselect] = !mute[chselect];
				if (song_type == SONG_XM)
					xm_mute_channel(xm.ctx, chselect+1, mute[chselect]);
				else if (song_type == SONG_MID)
					sf64_synth_mute_channel(synth, chselect, mute[chselect]);
				break;
			}
			if (ckeys.c_up) { 
				mute[chselect] = !mute[chselect];
				for (int i=0;i<song_channels;i++) {
					if (i != chselect)
						mute[i] = !mute[chselect];
					if (song_type == SONG_XM)
						xm_mute_channel(xm.ctx, i+1, mute[i]);
					else if (song_type == SONG_MID)
						sf64_synth_mute_channel(synth, i, mute[i]);
				}
				break;
			}

			if (ckeys.z && song_type == SONG_MID && num_banks > 1) {
				bank_sel = (bank_sel + 1) % num_banks;
				mid64player_close(mid);
				sf64_synth_close(synth);
				for (int i = 0; i < MID_VOICES; i++)
					mixer_ch_set_limits(i, 0, 0, 0);
				sf64_close(g_sf64_bank);
				g_sf64_bank = sf64_load(bankfiles[bank_sel]);
				assertf(g_sf64_bank, "cannot load %s", bankfiles[bank_sel]);
				synth = sf64_synth_create(g_sf64_bank);
				sf64_synth_set_channels(synth, 0, MID_VOICES, MIXER_PRIORITY_MUSIC);
				for (int i = 0; i < MID_VOICES; i++)
					mixer_ch_set_limits(i, 0, 96000, 0);
				mid = mid64player_load(cur_rom);
				mid64player_set_loop(mid, true);
				mid64player_play(mid, sf64_synth_midi_target(synth));
				memset(mute, 0, sizeof(mute));
				chselect = 0;
				break;
			}

			if (ckeys.b) {
				if (song_type == SONG_XM)
					xm64player_close(&xm);
				else if (song_type == SONG_YM)
					ym64player_close(&ym);
				else {
					mid64player_close(mid);
					sf64_synth_close(synth);
					for (int i = 0; i < MID_VOICES; i++)
						mixer_ch_set_limits(i, 0, 0, 0);
				}
				return PAGE_MENU;
			}
		} while (0);
	}
}

int main(void) {
	debug_init_emulog();
	debug_init_usblog();
	joypad_init();

	display_init(RESOLUTION_512x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
	dfs_init(DFS_DEFAULT_LOCATION);

	char sbuf[1024];
	dir_t dir;
	if (dir_findfirst("rom:/", &dir) == 0) do {
		if (strendswith(dir.d_name, ".xm64") || strendswith(dir.d_name, ".XM64") || 
			strendswith(dir.d_name, ".ym64") || strendswith(dir.d_name, ".YM64") ||
			strendswith(dir.d_name, ".mid64") || strendswith(dir.d_name, ".MID64")) {
			sprintf(sbuf, "rom:/%s", dir.d_name);
			songfiles[num_songs++] = strdup(sbuf);
		}
		if (strendswith(dir.d_name, ".sf64") || strendswith(dir.d_name, ".SF64")) {
			sprintf(sbuf, "rom:/%s", dir.d_name);
			bankfiles[num_banks++] = strdup(sbuf);
		}

	} while (dir_findnext("rom:/", &dir) == 0);

	xm64_set_extsampledir("rom:/samples");

	enum Page page = PAGE_INTRO;
	if (num_songs == 0)
		page = PAGE_INTRO_ERROR;

#if 0
	// Force immediately playback of a song (for mixer profiling).
	page = PAGE_SONG;
	cur_rom = "rom:/BUTTERFL.xm64";
#endif

	audio_init(44100, 4);
	mixer_init(32);

	// Default GM bank for MID64; Z cycles through every SF64 in rom:/.
	for (int i = 0; i < num_banks; i++) {
		if (strstr(bankfiles[i], "florestan-full")) {
			bank_sel = i;
			break;
		}
	}
	assertf(num_banks > 0, "no SF64 banks in rom:/");
	g_sf64_bank = sf64_load(bankfiles[bank_sel]);
	assertf(g_sf64_bank, "cannot load %s", bankfiles[bank_sel]);

	profile_parms_t pparms = {
		.num_slots = 32,
		.dump_stderr_interval = 5.0f,
	};
	profile_init(&pparms);
	__mixer_profile_init();
	profile_set_target_fps(60.0f);

	while(1) {
		switch (page) {
		case PAGE_INTRO: {
			page = page_intro();
		} break;

		case PAGE_MENU: {
			page = page_menu();
		} 	break;

		case PAGE_SONG: {
			page = page_song();
		} break;

		case PAGE_INTRO_ERROR: {
			page = page_intro_error();
		} break;
		}
	}

	return 0;
}
