/**
 * @file fmv.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Full-motion video playback (high-level API)
 * 
 * This module provides a super-high-level API to play full-motion videos with
 * audio. It builds upon the video library (video.h) and audio streaming library
 * (wav64.h) to provide a one-function-does-it-all API to play a video.
 * 
 * If you just want to drop a FMV in your application (eg: for a logo screen),
 * this is the easiest way to do it.
 */

#ifndef LIBDRAGON_VIDEO_FMV_H
#define LIBDRAGON_VIDEO_FMV_H

#include <stdbool.h>
#include "preview.h"

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct video_s video_t;
typedef struct video_info_s video_info_t;
typedef struct yuv_blitter_s yuv_blitter_t;
typedef struct wav64_s wav64_t;
typedef struct subtitles_s subtitles_t;
typedef struct subrenderer_s subrenderer_t;
///@endcond

/** @brief Structure to control FMV playback */
typedef struct fmv_control_s {
    /** Video track */
    video_t *video;
    /** Audio track */
    wav64_t *audio;
    /** Subtitle track */
    subtitles_t *subs;
    /** Pause/unpause playback */
    void (*pause)(struct fmv_control_s *ctrl, bool pause);
    /** Stop playback */
    void (*stop)(struct fmv_control_s *ctrl);
    /** Seek to a specific frame */
    int (*seek_frame)(struct fmv_control_s *ctrl, int frame_idx, bool exact);
    /** Seek to a specific time (in seconds) */
    float (*seek_time)(struct fmv_control_s *ctrl, float time_sec, bool exact);
} fmv_control_t; 

/** @brief FMV playback parameters */
typedef struct fmv_parms_s {
    /** @brief Disable audio playback (even though an audio file is present) */
    bool disable_audio;
    /** @brief Disable subtitle rendering (even though a subtitle file is present) */
    bool disable_subtitles;
    /** 
     * @brief Disable configuration of display mode.
     *
     * By default, #fmv_play will call #display_init to configure a "perfect",
     * resolution for the video playback. This will be a custom VI resolution
     * to perfectly match the video resolution *and* aspect ratio, so that any
     * required letterboxing will be be performed at zero cost by VI itself.
     *
     * Alternatively, #fmv_play can also render the video over an existing display
     * configuration. This is mostly useful for cases where you need a smooth
     * transition between the video playback and some other graphics, as calling
     * display_close() will always incur in a black flash (because of framebuffer
     * deallocation and reallocation).
     *
     * In this case, the video will be automatically scaled to the display size,
     * and aspect ratio will be preserved by actively drawing black bars into
     * the framebuffers.
     *
     * Notice that if you are using a 16bpp display, the best results quality-wise
     * are achieved with #FILTERS_RESAMPLE_ANTIALIAS_DEDITHER.
     */
    bool disable_display_init;
    /** 
     * @brief Disable frame skipping to keep audio in sync.
     * 
     * Note that disabling this can cause video and audio to go out of sync,
     * if the video player can't keep up with the audio.
     */
    bool disable_frame_skipping;

    /** 
     * @brief Filename of the audio track to play alongside the video. 
     * 
     * If not specified, the video player will try to find an audio track
     * with the same name as the video file, but with a .wav64 extension.
     */
    const char *audio_fn;

    /**
     * @brief Subtitle track filename to play alongside the video.
     * 
     * If not specified, the video player will try to find a subtitle track
     * with the same name as the video file, but with a .sub64 extension.
     */
    const char *subtitles_fn;

    /** The mixer channel to use for audio playback (default: 0) */
    int audio_mixer_channel;

    /** @brief Enable CRT overscan margins (only if display_init is not disabled) */
    bool crt_margin;

    /** @brief Continue playback from the start when the end of the video is reached */
    bool loop;

    /**
     * @brief Subtitle renderer to use.
     * 
     * This parameter allows to specify a custom subtitle renderer, that can
     * be used to customise how subtitles are rendered on screen. There are
     * two built-in renderers:
     * 
     * * RDPQ rendered (#subrenderer_create_rdpq) that draws the subtitles over
     *   the video using rdpq for high-quality text rendering.
     * * EIA-608 renderer (#subrenderer_create_eia608) that encodes subtitles
     *   using the EIA-608 standard for closed captions (on NTSC TVs).
     * 
     * If this parameter is NULL, a default RDPQ renderer will be created
     * and used, with a default builtin tool.
     * 
     * @note Ownership of the renderer is transferred to the FMV player, that
     *       will free it at the end of playback.
     */
    subrenderer_t *sub_renderer;

    /**
     * @brief Callback function to draw OSD on top of the video
     * 
     * This callback function, if provided, will be called once per frame,
     * after the video frame has been drawn but before the framebuffer
     * is presented on the screen. 
     * 
     * This allows to draw an on-screen display (OSD) on top of the video, for
     * instance to show playback controls, subtitles, or other information.
     * 
     * It also allows per-frame input control (eg: to implement pausing or
     * skipping);
     * 
     * @param osd_ctx       Context pointer passed to the OSD callback
     * @param frame_idx     Index of the current frame being played
     * @param time_sec      Current playback time in seconds
     * @param ctrl          Pointer to a control structure that can be used
     *                      to control playback (pause, stop, seek, etc.)
     */
    void (*osd_callback)(void *osd_ctx, int frame_idx, float time_sec, fmv_control_t *ctrl);
    void *osd_ctx;          ///< Context pointer passed to the OSD callback

    /**
     * @brief Custom YUV blitter factory (optional)
     *
     * If provided, replaces the default #yuv_blitter_new_fmv blitter used by
     * #fmv_play. This allows customizing how video frames are scaled and drawn
     * onto the framebuffer (eg: for mirroring or custom letterboxing).
     *
     * @param osd_ctx   Value of #fmv_parms_t::osd_ctx
     * @param info      Video metadata from the opened stream
     */
    yuv_blitter_t (*create_yuv_blitter)(void *osd_ctx, video_info_t *info);
} fmv_parms_t;


/**
 * @brief Play a full-motion video from a video file
 * @preview
 * 
 * This function plays a full-motion video from a video file, along with its
 * audio track if present. It takes care of everything: setting up the
 * display subsystem, opening the video and audio files, decoding and
 * rendering the video frames, playing back the audio, and synchronizing
 * everything. The function blocks until the video playback is complete (or is
 * aborted via the control interface, see below).
 * 
 * Playback can be customized via the #fmv_parms_t structure. In particular,
 * it is possible to register a callback function (#fmv_parms_t::osd_callback)
 * that is called once per frame, allowing to draw an on-screen display (OSD)
 * on top of the video, or to control playback (pause, stop, seek, etc.). For
 * instance, this can be used to implement skipping the video when a button is
 * pressed, like in this example:
 * 
 * \@code{.c}
 * void my_osd_callback(void *ctx, int frame_idx, float time_sec, fmv_control_t *ctrl) {
 *     joypad_poll();
 *     joypad_buttons_t btn = joypad_get_buttons_pressed(JOYPAD_PORT_1);
 *     if (btn.start) ctrl->stop(ctrl);
 * }
 * 
 * fmv_parms_t parms = {
 *    .osd_callback = my_osd_callback,
 * };
 * 
 * fmv_play("rom:/intro.h264", &parms);
 * \@endcode 
 * 
 * @param filename      Filename of the video file to play (including filesystem prefix)
 * @param parms         Additional parameters to customize playback (can be NULL)
 */
LIBDRAGON_PREVIEW_API
void fmv_play(const char *filename, const fmv_parms_t *parms);

#ifdef __cplusplus
}
#endif

#endif
