/**
 * @file fmv.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Full-motion video playback (high-level API)
 */

#include "fmv.h"
#include "video.h"
#include "yuv.h"
#include "display.h"
#include "wav64.h"
#include "display.h"
#include "mixer.h"
#include "rdpq.h"
#include "rdpq_attach.h"

void fmv_play(const char *video_fn, const fmv_parms_t *parms)
{
    video_t *video = video_open(video_fn);
    video_info_t info = video_get_info(video);
    if (!parms) {
        parms = alloca(sizeof(fmv_parms_t));
        memset((void*)parms, 0, sizeof(fmv_parms_t));
    }

    display_init((resolution_t){
        .width = info.width,
        .height = info.height,
        .aspect_ratio = info.aspect_ratio,
        .overscan_margin = parms->crt_margin ? VI_CRT_MARGIN : 0,
    }, DEPTH_32_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);

    yuv_init();
    yuv_blitter_t yuv = yuv_blitter_new_fmv(
        info.width, info.height,
        display_get_width(), display_get_height(),
        &(yuv_fmv_parms_t){ .cs = &info.colorspace }
    );

    // Engage the fps limiter to ensure proper video pacing.
    display_set_fps_limit(info.framerate);

    // Open audio if not disabled
    wav64_t* audio = NULL;
    if (!parms->disable_audio) {
        const char *audio_fn = parms->audio_fn;
        if (!audio_fn) {
            // Try to find audio file with same name but .wav64 extension
            const char *ext = strrchr(video_fn, '.');
            assertf(ext, "Audio filename not specified for video playback");

            // Create audio filename
            audio_fn = alloca(strlen(video_fn) + 7);
            size_t base_len = ext - video_fn;
            strncpy((char*)audio_fn, video_fn, base_len);
            strcpy((char*)audio_fn + base_len, ".wav64");
        }

        // Load audio
        audio = wav64_load(audio_fn, NULL);
    }

    int frame_idx = 0;
    bool paused = false;
    bool abort = false;

    void ctrl_pause(fmv_control_t *ctrl, bool pause) {
        paused = pause;
    }
    void ctrl_stop(fmv_control_t *ctrl) {
        abort = true;
    }
    int ctrl_seek_frame(fmv_control_t *ctrl, int idx) {
        frame_idx = video_seek_frame(video, idx);
        return frame_idx;
    }
    float ctrl_seek_time(fmv_control_t *ctrl, float time_sec) {
        float actual_time = video_seek_time(video, time_sec);
        frame_idx = (int)(actual_time * info.framerate);
        return actual_time;
    }

    // Prepare control structure for OSD callback
    fmv_control_t ctrl = {
        .video = video,
        .audio = audio,
        .pause = ctrl_pause,
        .stop = ctrl_stop,
        .seek_frame = ctrl_seek_frame,
        .seek_time = ctrl_seek_time,
    };
    
    if (audio)
        mixer_ch_play(parms->audio_mixer_channel, &audio->wave);

    while (!abort) {
        mixer_try_play();

        if (!paused) {
            if (!video_next_frame(video)) {
                if (parms->loop) {
                    video_rewind(video);
                    frame_idx = 0;
                    continue;
                } else {
                    break;
                }
            }
        }

        mixer_try_play();

        surface_t *disp = display_try_get();
        while (disp == NULL) {
            if (!video_poll(video)) {
                disp = display_get();
                break;
            }
            disp = display_try_get();
        }

        rdpq_attach(disp, NULL);

        // Get the next video frame and feed it into our previously set up blitter.
        yuv_frame_t frame = video_get_frame(video);
        yuv_blitter_run(&yuv, &frame);

        // Call OSD callback if provided
        if (parms->osd_callback) {
            float time_sec = frame_idx / info.framerate;
            parms->osd_callback(parms->osd_ctx, frame_idx, time_sec, &ctrl);
        }

        rdpq_detach_show();

        frame_idx++;
    }

    rspq_wait();

    if (audio) {
        mixer_ch_stop(parms->audio_mixer_channel);
        wav64_close(audio);
    }

    yuv_blitter_free(&yuv);
    yuv_close();
    video_close(video);
    display_close();
}
