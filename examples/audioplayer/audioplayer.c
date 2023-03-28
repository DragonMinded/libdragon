#include <libdragon.h>
#include <stdio.h>

// We need to show lots of internal details of the module which are not
// exposed via public API, so include the internal header file.
#include "../../src/audio/libxm/xm_internal.h"
#include "../../src/audio/lzh5.h"

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

static void draw_header(display_context_t disp) {
	graphics_draw_text(disp, 200-70, 10, "XM/YM Module Audio Player");
	graphics_draw_text(disp, 200-45, 20, "v2.0 - by Rasky");
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
	display_context_t disp = display_lock();
	graphics_fill_screen(disp, 0);
	draw_header(disp);

	graphics_draw_text(disp, 30, 50, "This player is capable of playing .XM/.YM modules,");
	graphics_draw_text(disp, 30, 58, "up to 32 channels and 48Khz, using an optimized");
	graphics_draw_text(disp, 30, 66, "engine that uses little CPU and RSP time. ");

	graphics_draw_text(disp, 30, 80, "XM/YM files must first be converted into XM64/YM64,");
	graphics_draw_text(disp, 30, 88, "using the audioconv64 tool. This format is");
	graphics_draw_text(disp, 30, 96, "designed for native playback on N64.");

	graphics_draw_text(disp, 30,112, "The player will stream most of the data");
	graphics_draw_text(disp, 30,120, "directly from the ROM, so also the amount of");
	graphics_draw_text(disp, 30,128, "RDRAM that will be used will be very little.");

	graphics_draw_text(disp, 30,144, "Press START to begin!");

	display_show(disp);

	while (1) {
		controller_scan();
		struct controller_data ckeys = get_keys_down();

		if (ckeys.c[0].start) {
			return PAGE_MENU;
		}
	}
}

enum Page page_intro_error(void) {
	display_context_t disp = display_lock();
	graphics_fill_screen(disp, 0);
	draw_header(disp);
	graphics_draw_text(disp, 40, 50, "No .XM64 roms found in the filesystem");
	display_show(disp);
	abort();
}

enum Page page_menu(void) {
	char sbuf[1024];
	display_context_t disp = display_lock();
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
		controller_scan();
		struct controller_data ckeys = get_keys_down();

		if (ckeys.c[0].up)      { menu_sel -= 1; break; }
		if (ckeys.c[0].down)    { menu_sel += 1; break; }
		if (ckeys.c[0].left)    { menu_sel -= COL_ROWS; break; }
		if (ckeys.c[0].right)   { menu_sel += COL_ROWS; break; }
		if (ckeys.c[0].C_up)    { menu_sel = 0; break; }
		if (ckeys.c[0].C_down)  { menu_sel = num_songs-1; break; }
		if (ckeys.c[0].C_left)  { menu_sel -= COL_ROWS*NUM_COLUMNS; break; }
		if (ckeys.c[0].C_right) { menu_sel += COL_ROWS*NUM_COLUMNS; break; }

		if (ckeys.c[0].A) {
			cur_rom = songfiles[menu_sel];
			chselect = 0;
			return PAGE_SONG;
		}
	}

	return PAGE_MENU;
}

enum Page page_song(void) {
	char sbuf[1024];
	int64_t tot_time = 0, tot_cpu = 0, tot_rsp = 0, tot_dma = 0;
	int screen_first_inst = 0;
	enum SONG_TYPE { SONG_XM, SONG_YM };

	xm64player_t xm;
	ym64player_t ym; ym64player_songinfo_t yminfo;
	enum SONG_TYPE song_type;
	const char *song_name; int song_channels;
	int song_romsz=0, song_ramsz=0;

	if (strendswith(cur_rom, ".ym64") || strendswith(cur_rom, ".YM64"))
		song_type = SONG_YM;
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

		song_ramsz = sizeof(xm64player_t) + xm.ctx->ctx_size;
		#if XM_STREAM_PATTERNS
		song_ramsz -= xm.ctx->ctx_size_all_patterns;
		song_ramsz += xm.ctx->ctx_size_stream_pattern_buf;
		#endif
		#if XM_STREAM_WAVEFORMS
		song_ramsz -= xm.ctx->ctx_size_all_samples;
		for (int i=0;i<32;i++)
			song_ramsz += xm.ctx->ctx_size_stream_sample_buf[i];
		#endif
	} else {
		ym64player_open(&ym, cur_rom, &yminfo);
		ym64player_play(&ym, 0);
		song_name = yminfo.name;
		song_channels = 3;
		wrap(yminfo.comment, 40);
		song_ramsz = sizeof(ym64player_t);
		if (ym.decoder) song_ramsz += sizeof(*ym.decoder);
	}

	// Unmute all channels
	memset(mute, 0, sizeof(mute));

	while (true) {
		display_context_t disp = display_lock();
		graphics_fill_screen(disp, 0);
		draw_header(disp);

		sprintf(sbuf, "Filename: %s", cur_rom+5);
		graphics_draw_text(disp, 20, 40, sbuf);

		sprintf(sbuf, "Song: %s", song_name);
		graphics_draw_text(disp, 20, 50, sbuf);

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
		}

		if (tot_time) {
			float pcpu = (float)tot_cpu * 100.f / (float)tot_time;
			float prsp = (float)tot_rsp * 100.f / (float)tot_time;
			float pdma = (float)tot_dma * 100.f / (float)tot_time;

			sprintf(sbuf, "CPU: %.2f%%  RSP: %.2f%%\n", pcpu, prsp);
			graphics_draw_text(disp, 280, 60, sbuf);
			sprintf(sbuf, "DMA: %.2f%%", pdma);
			graphics_draw_text(disp, 280, 70, sbuf);

			debugf("CPU: %.2f%%  RSP: %.2f%%  DMA: %.2f%%\n", pcpu, prsp, pdma);
		}

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
		} else {
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
		}

		display_show(disp);

		tot_time = 0, tot_cpu = 0, tot_rsp = 0, tot_dma = 0;

		uint32_t start_play_loop = TICKS_READ();
		bool first_loop = true;
		int audiosz = audio_get_buffer_length();
		while (TICKS_DISTANCE(start_play_loop, TICKS_READ()) < TICKS_PER_SECOND)
		{
			extern int64_t __mixer_profile_rsp, __wav64_profile_dma;
			__mixer_profile_rsp = __wav64_profile_dma = 0;

			uint32_t t0 = TICKS_READ();

			while (!audio_can_write()) {}

			uint32_t t1 = TICKS_READ();

			int16_t *out = audio_write_begin();
			mixer_poll(out, audiosz);
			audio_write_end();

			uint32_t t2 = TICKS_READ();

			if (!first_loop) {
				tot_dma += __wav64_profile_dma;	
				tot_rsp += __mixer_profile_rsp;
				tot_cpu += (t2-t1) - __mixer_profile_rsp - __wav64_profile_dma;
				tot_time += t2-t0;
			}
			first_loop = false;

			controller_scan();
			struct controller_data ckeys = get_keys_down();
			if (ckeys.c[0].left || ckeys.c[0].right) {
				if (song_type == SONG_XM) {				
					int patidx;
					xm64player_tell(&xm, &patidx, NULL, NULL);
					if (ckeys.c[0].left && patidx > 0) patidx--;
					if (ckeys.c[0].right && patidx < xm_get_module_length(xm.ctx)-1) patidx++;
					xm64player_seek(&xm, patidx, 0, 0);
					break;
				} else if (song_type == SONG_YM && !ym.decoder) {
					int pos, len;
					ym64player_duration(&ym, &len, NULL);
					ym64player_tell(&ym, &pos, NULL);
					if (ckeys.c[0].left && pos >= 0x200) pos -= 0x200;
					if (ckeys.c[0].right && pos <= len-0x200) pos += 0x200;
					ym64player_seek(&ym, pos);
					break;
				}
			}

			if (song_type == SONG_XM) {			
				if (ckeys.c[0].up && screen_first_inst > 0) {
					screen_first_inst--;
					break;
				}
				if (ckeys.c[0].down && screen_first_inst < xm.ctx->module.num_instruments-1) {
					screen_first_inst++;
					break;
				}
			}

			if (ckeys.c[0].C_left && chselect > 0) { chselect--; break; }
			if (ckeys.c[0].C_right && chselect < song_channels-1) { chselect++; break; }
			if (ckeys.c[0].C_down) {
				mute[chselect] = !mute[chselect];
				if (song_type == SONG_XM)
					xm_mute_channel(xm.ctx, chselect+1, mute[chselect]);
				break;
			}
			if (ckeys.c[0].C_up) { 
				mute[chselect] = !mute[chselect];
				for (int i=0;i<song_channels;i++) {
					if (i != chselect)
						mute[i] = !mute[chselect];
					if (song_type == SONG_XM)
						xm_mute_channel(xm.ctx, i+1, mute[i]);
				}
				break;
			}

			if (ckeys.c[0].B) {
				if (song_type == SONG_XM)
					xm64player_close(&xm);
				else
					ym64player_close(&ym);
				return PAGE_MENU;
			}
		}
	}
}

int main(void) {
	controller_init();
	debug_init_isviewer();
	debug_init_usblog();

	display_init(RESOLUTION_512x240, DEPTH_16_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);
	dfs_init(DFS_DEFAULT_LOCATION);

	char sbuf[1024];
	strcpy(sbuf, "rom:/");
	if (dfs_dir_findfirst(".", sbuf+5) == FLAGS_FILE) {
		do {
			if (strendswith(sbuf, ".xm64") || strendswith(sbuf, ".XM64") || 
				strendswith(sbuf, ".ym64") || strendswith(sbuf, ".YM64"))
				songfiles[num_songs++] = strdup(sbuf);
		} while (dfs_dir_findnext(sbuf+5) == FLAGS_FILE);
	}


	enum Page page = PAGE_INTRO;
	if (num_songs == 0)
		page = PAGE_INTRO_ERROR;

#if 0
	// Force immediately playback of a song
	page = PAGE_SONG;
	cur_rom = "rom:/Claustrophobia.xm64";
#endif

	audio_init(44100, 4);
	mixer_init(32);

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
