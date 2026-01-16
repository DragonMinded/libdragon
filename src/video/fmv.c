/**
 * @file fmv.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Full-motion video playback (high-level API)
 */

#include "fmv.h"
#include "video.h"
#include "video_sync.h"
#include "yuv.h"
#include "display.h"
#include "wav64.h"
#include "mixer.h"
#include "subtitles.h"
#include "graphics.h"
#include "rdpq.h"
#include "rdpq_attach.h"
#include <sys/stat.h>

void fmv_play(const char *video_fn, const fmv_parms_t *parms)
{
    video_t *video = video_open(video_fn, &(video_parms_t){ .buffered_pics = 8 });
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
        struct stat st;
        if (stat(audio_fn, &st) == 0) {
            audio = wav64_load(audio_fn, NULL);
        }
    }

    subtitles_t* subs = NULL;
    subrenderer_t* subrenderer = parms->sub_renderer;
    if (!parms->disable_subtitles) {
        // Open subtitles
        const char *subs_fn = parms->subtitles_fn;
        if (!subs_fn) {
            // Try to find subtitle file with same name but .sub64 extension
            const char *ext = strrchr(video_fn, '.');
            assertf(ext, "Subtitle filename not specified for video playback");

            // Create subtitle filename
            subs_fn = alloca(strlen(video_fn) + 7);
            size_t base_len = ext - video_fn;
            strncpy((char*)subs_fn, video_fn, base_len);
            strcpy((char*)subs_fn + base_len, ".sub64");
        }

        struct stat st;
        if (stat(subs_fn, &st) == 0) {
            // Load subtitles
            subs = subtitles_load(subs_fn);

            // Create a default subtitle renderer if none provided
            if (!subrenderer)
                subrenderer = subrenderer_create_rdpq(
                    &(subrenderer_rdpq_parms_t){
                        .bkg_color = RGBA32(0,0,0,128)
                    }
                );
        }
    }

    int frame_idx = 0;
    bool paused = false;
    bool abort = false;
    video_sync_t *vsync = NULL;

    void ctrl_pause(fmv_control_t *ctrl, bool pause) {
        paused = pause;
    }
    void ctrl_stop(fmv_control_t *ctrl) {
        abort = true;
    }
    int ctrl_seek_frame(fmv_control_t *ctrl, int idx, bool exact) {
        frame_idx = video_seek(video, idx);
        if (frame_idx < 0) return -1;  // seeking not supported
        if (exact) {
            // Decode frames until we reach the exact frame
            while (frame_idx < idx) {
                if (!video_next_frame(video)) break;
                frame_idx++;
            }
        }
        // Sync also audio
        if (audio) {
            double time_sec = (double)frame_idx / (double)info.framerate;
            wav64_seek(audio, parms->audio_mixer_channel, time_sec);
        }
        if (subs) {
            subtitles_seek(subs, frame_idx);
        }
        if (vsync) {
            video_sync_reset(vsync, frame_idx);
        }
        return frame_idx;
    }
    float ctrl_seek_time(fmv_control_t *ctrl, float time_sec, bool exact) {
        int frame_idx = (int)(time_sec * info.framerate);
        frame_idx = ctrl_seek_frame(ctrl, frame_idx, exact);
        if (frame_idx < 0) return -1;   // seeking not supported
        return (float)frame_idx / info.framerate;
    }

    // Prepare control structure for OSD callback
    fmv_control_t ctrl = {
        .video = video,
        .audio = audio,
        .subs = subs,
        .pause = ctrl_pause,
        .stop = ctrl_stop,
        .seek_frame = ctrl_seek_frame,
        .seek_time = ctrl_seek_time,
    };
    
    if (audio) {
        mixer_ch_play(parms->audio_mixer_channel, &audio->wave);

        if (!parms->disable_frame_skipping) {
            vsync = video_sync_create(video, NULL);
        }
    }

    while (!abort) {
        bool skip_render = false;

        mixer_try_play();

        if (!paused) {
            if (vsync && mixer_ch_playing(parms->audio_mixer_channel)) {
                // Decide what to do next based on the sync controller
                double master_time_sec = mixer_ch_get_pos(parms->audio_mixer_channel) / (double)audio->wave.frequency;
                video_sync_action_t a = video_sync_step(vsync, master_time_sec, frame_idx);

                if (a.kind == VIDEO_SYNC_SKIP_NEXT) {
                    // Skip rendering this frame
                    skip_render = true;
                } else if (a.kind == VIDEO_SYNC_SEEK_AND_RENDER) {
                    // Seek to the target frame and render it.
                    // Also seek subtitles to the target frame.
                    int new_idx = video_seek(video, a.seek_frame);
                    if (new_idx >= 0) {
                        frame_idx = new_idx;
                        if (subs) subtitles_seek(subs, frame_idx);
                    }
                }
            }

            if (!video_next_frame(video)) {
                if (parms->loop) {
                    frame_idx = 0;
                    video_rewind(video);
                    if (audio) wav64_seek(audio, parms->audio_mixer_channel, 0.0);
                    if (subs) subtitles_seek(subs, 0);
                    if (vsync) video_sync_reset(vsync, frame_idx);
                    continue;
                } else {
                    break;
                }
            }

            if (subs) subtitles_next_frame(subs);
        }

        mixer_try_play();

        if (!skip_render) {
            surface_t *disp = display_try_get();
            while (disp == NULL) {
                if (!paused && !video_poll(video)) {
                    disp = display_get();
                    break;
                }
                disp = display_try_get();
            }

            rdpq_attach(disp, NULL);

            // Get the next video frame and feed it into our previously set up blitter.
            yuv_frame_t frame = video_get_frame(video);
            yuv_blitter_run(&yuv, &frame);

            // Draw subtitles
            if (subs) {
                subtitle_cue_t cues[3];
                int num_cues = subtitles_get_current_cues(subs, cues, 3);
                subrenderer_render(subrenderer, cues, num_cues);
            }

            // Call OSD callback if provided
            if (parms->osd_callback) {
                float time_sec = frame_idx / info.framerate;
                parms->osd_callback(parms->osd_ctx, frame_idx, time_sec, &ctrl);
            }

            rdpq_detach_show();
        }

        if (!paused)
            frame_idx++;
    }

    rspq_wait();

    if (audio) {
        mixer_ch_stop(parms->audio_mixer_channel);
        wav64_close(audio);
    }

    yuv_blitter_free(&yuv);
    yuv_close();
    if (vsync) video_sync_destroy(vsync);
    video_close(video);
    display_close();
}
