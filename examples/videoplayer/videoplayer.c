#include <libdragon.h>

// Number of frame back buffers we reserve.
// These buffers are used to render the video ahead of time.
// More buffers help ensure smooth video playback at the cost of more memory.
#define NUM_DISPLAY 8

// Maximum target audio frequency.
//
// Needs to be 48 kHz if Opus audio compression is used.
// In this example, we are using VADPCM audio compression
// which means we can use the real frequency of the audio track.
#define AUDIO_HZ 32000.0f


const char *colorspace_name(yuv_colorspace_t *cs) {
	if (memcmp(cs, &YUV_BT601_TV, sizeof(yuv_colorspace_t)) == 0) {
		return "BT.601 TV";
	} else if (memcmp(cs, &YUV_BT601_FULL, sizeof(yuv_colorspace_t)) == 0) {
		return "BT.601 Full";
	} else if (memcmp(cs, &YUV_BT709_TV, sizeof(yuv_colorspace_t)) == 0) {
		return "BT.709 TV";
	} else if (memcmp(cs, &YUV_BT709_FULL, sizeof(yuv_colorspace_t)) == 0) {
		return "BT.709 Full";
	}
	return "Unknown";
}

int main(void)
{
	joypad_init();
	debug_init_isviewer();
	debug_init_usblog();

	dfs_init(DFS_DEFAULT_LOCATION);
	rdpq_init();
	yuv_init();
	joypad_init();

	audio_init(AUDIO_HZ, 4);
	mixer_init(8);

	video_register_codec(&mpeg1_codec);
	video_register_codec(&h264_codec);

	// Initialize profiling (FIXME)
	profile_init(NULL);
	void __h264_profile_init(void);
	void __mpeg1_profile_init(void);

	// Check if the movie is present in the filesystem, so that we can provide
	// a specific error message.
	FILE *f = fopen("rom:/movie.h264", "rb");
	assertf(f, "Movie not found!\nInstall wget and ffmpeg to download and encode the sample movie\n");
	fclose(f);

	__h264_profile_init();
	// __mpeg1_profile_init();

	bool show_fps = false;
	rdpq_font_t *fnt = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO);
	rdpq_text_register_font(10, fnt);

	void osd_callback(void *ctx, int frame_idx, float time_sec, fmv_control_t *ctrl)
	{
		// Show debug info on first frame
		if (frame_idx == 0) {
			video_info_t info = video_get_info(ctrl->video);
			debugf("Video resolution: %dx%d [DAR=%.2f] @ %.2f FPS - Colorspace: %s\n", info.width, info.height, 
				info.aspect_ratio, info.framerate, colorspace_name(&info.colorspace));
			profile_set_target_fps(info.framerate);
		}

		// Poll input
		joypad_poll();
		joypad_buttons_t btn = joypad_get_buttons_pressed(JOYPAD_PORT_1);
		if (btn.z) show_fps = !show_fps;
		if (btn.d_right) ctrl->seek_time(ctrl, time_sec + 10.0f, false);
		if (btn.d_left)  ctrl->seek_time(ctrl, time_sec - 10.0f, false);
		
		// Draw FPS
		if (show_fps) {
			rdpq_text_printf(NULL, 10, display_get_width() - 80, 15,
				"FPS: %.02f\nTime: %02d:%02d", display_get_fps(), (int)(time_sec / 60), (int)time_sec % 60);
		}

		profile_next_frame();
	}

	fmv_play("rom:/movie.h264", &(fmv_parms_t){
		.osd_callback = osd_callback
	});
}
