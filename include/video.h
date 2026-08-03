/**
 * @file video.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Video player subsystem
 * 
 * This library allows to playback videos on the N64, using video codecs that
 * can be registered into it. It provides an uniform and flexible API for any
 * video-related tasks, including advanced uses such as showing videos are part
 * of a 3D scene, or making a game based on videos.
 * 
 * If you just want to display a full-motion video (FMV) with audio, consider using
 * the fmv.h module instead, which provides a much higher-level API to do
 * just that.
 * 
 * Currently two video codecs are supported and provided as part of libdragon:
 * 
 * * #mpeg1_codec: MPEG1
 * * #h264_codec: H.264 Baseline Profile
 * 
 * Higher-level information on how to encode videos and suggested settings can be found
 * in the Libdragon wiki: https://github.com/DragonMinded/libdragon/wiki/MPEG1-Player
 * 
 * The API in this file is quite simple. The main entry point is #video_open,
 * which opens a video file and returns a handle to it. #video_get_info can be used
 * to query information about the video, such as width, height, framerate, etc.
 * 
 * To play the video, the main loop should call #video_next_frame any time
 * a new frame is needed (depending on the desired playback frequency),
 * and then #video_get_frame to get the frame to display. The frame is
 * returned as a #yuv_frame_t. You can then use the YUV library to display
 * it, either fullscreen, in a smaller portion, or even into an offscreen
 * surface to be used eg. like a texture.
 * 
 * In other words, this library only handles video decoding into a YUV surface
 * in memory. It does not perform any display by itself; it is up to the
 * client code to display the frames using the yuv.h library or any other
 * method.
 * 
 * Notice that the time required to decode a frame is not constant, and can
 * vary a lot; especially I-frames tend to be much heavier to decode, so if
 * possible allow for some buffering to avoid slowdowns. Buffering can be
 * done by calling #video_poll when possible (eg: in idle time), so that the
 * decoder can keep decoding frames ahead of time.
 */

/**
 * @defgroup video Video Subsystem
 * @ingroup libdragon
 * @brief Compressed video playback libraries (for FMVs, etc.)
 * 
 * This group contains modules to play compressed video files, such as
 * full-motion videos (FMVs):
 * 
 * * yuv.h: Hardware-accelerated YUV to RGB conversion using RDP and RSP.
 *          It provides functions to blit YUV frames to RGB surfaces.
 * * video.h: Video player API to open and playback video files. It allows
 *          to retrieve video information, decode video frames, and seek.
 *          This can be used for building custom video players, including
 *          using videos in more complex scenes (eg: as textures in 3d scenes).
 * * fmv.h: Very High-level single-function API to playback full-motion videos,
 *          synchronized with audio. It provides a one-function-does-it-all
 *          API to play a video file with minimal code.
 */

#ifndef LIBDRAGON_VIDEO_H
#define LIBDRAGON_VIDEO_H

#include "yuv.h"

#ifdef __cplusplus
extern "C" {
#endif

///@cond
struct video_s;
struct video_codec_s;
///@endcond

/** @brief A video codec */
typedef struct video_codec_s video_codec_t;

/** @brief A video handle */
typedef struct video_s video_t;

/** @brief Video playback parameters*/
typedef struct video_parms_s {
    /**
     * @brief Number of fully decoded pictures the decoder is allowed to keep buffered
     *
     * This acts as a "cushion" for the player, allowing background decoding
     * (via #video_poll) to run ahead of time. Not all codecs use this setting.
     * This setting obviously impacts the memory usage of the decoder; decoded
     * pictures are kept in a codec-specific format, though this is normally
     * YUV 4:2:0 (16 bpp).
     *
     * If 0 (default), #video_poll is effectively disabled and cannot decode ahead.
     */
    int buffered_pics;
} video_parms_t;

/**
 * @brief Information about a video stream
 * 
 * This structure contains information about a video stream. It is returned by
 * #video_get_info.
 */
typedef struct video_info_s {
    /** @brief Width of the video in pixels */
    int width;
    /** @brief Height of the video in pixels */
    int height;
    /** 
     * @brief Display aspect ratio (DAR) of the video (eg: 4.0/3.0)
     * 
     * For normal video streams with square pixels, this will be identical
     * to width/height. Some streams might be anamorphic, meaning that the picture
     * has been encoded with non-square pixels. In this case, the aspect ratio
     * will be different, and must be used to correctly display the video.
     */
    float aspect_ratio;
    /** @brief Framerate of the video in frames per second */
    float framerate;
    /**
     * @brief YUV colorspace for the video
     * 
     * YUV pictures can be encoded with different colorspaces. This field
     * describes the colorspace used by the video stream, so that the
     * yuv.h library can correctly convert the frames to RGB.
     */
    yuv_colorspace_t colorspace;
} video_info_t;

/**
 * @brief Register a video codec
 * 
 * This function registers a video codec into the video subsystem. It is necessary
 * to call this function for each codec that the video subsystem must support,
 * before calling #video_open.
 * 
 * @param codec         Video codec to register
 */
void video_register_codec(video_codec_t *codec);

/**
 * @brief Open a video file and decode its header info
 * 
 * This function opens a video file and returns a handle to it. The video
 * file will be handled by the appropriate codec, based on the file extension.
 * 
 * @param fn            Path to the video file (including filesystem prefix)
 * @param parms         Optional parameter block (can be NULL)
 * @return              Handle to the opened video
 */
video_t* video_open(const char *fn, const video_parms_t *parms);

/**
 * @brief Get information about the video
 * 
 * This function retrieves information about the opened video, such as
 * its width, height, framerate, aspect ratio, and colorspace. It is useful
 * to configure the display subsystem before starting playback.
 * 
 * @param v                 Handle to the video
 * @return                  Information about the video
 */
video_info_t video_get_info(video_t *v);

/**
 * @brief Move to the next frame in the video stream
 * 
 * This function decodes the next frame in the video stream. If the frame
 * is successfully decoded, it can be retrieved with #video_get_frame. Otherwise,
 * the stream is finished and the function will return false.
 * 
 * @param v             Handle to the video
 * @return true         If a frame was successfully decoded
 * @return false        If the stream is finished
 */
bool video_next_frame(video_t *v);

/**
 * @brief Retrieve the last fully-decoded frame
 * 
 * This function returns the last frame decoded by #video_poll or #video_next_frame.
 * The frame is returned as a #yuv_frame_t, which can be used to display the
 * frame on the screen via the yuv.h library.
 * 
 * @param v             Handle to the video
 * @return              The current decoded frame
 * 
 * @see #yuv_tex_blit
 * @see #yuv_blitter_new_fmv
 * @see #yuv_blitter_new
 */
yuv_frame_t video_get_frame(video_t *v);

/**
 * @brief Do some work of video decoding
 * 
 * This function tells the video decoder to do a chunk of work, and keep
 * decoding the video stream ahead. It can be used for scenarios where the
 * client needs to perform other tasks while the video is being decoded.
 * 
 * Usage of this function is totally optionally. A client can simply call
 * #video_next_frame repeatedly to decode the video stream frame by frame.
 * It is however useful to make use of idle CPU time to buffer ahead.
 * Depending on the exact video encoding parameters and how the codec is
 * written, this function can decode multiple frames ahead of time.
 *
 * For #video_poll to decode ahead, video_parms_t::buffered_pics must
 * be greater than 0 (when video_open is called). In fact this is the number
 * of pictures that the decoder is allowed to keep buffered.
 * 
 * The function returns 1 if the decoder did a chunk of work, or 0
 * if no work could be done. Typically, if poll returns 0, no more work
 * will be possible until the client calls #video_next_frame to move to the
 * next frame.
 * 
 * @param v             Handle to the video
 * @return 0            Next frame is not yet ready
 * @return 1            Next frame is ready
 * @return -1           End of stream reached
 */
int video_poll(video_t *v);

/**
 * @brief Rewind the video to the beginning
 * 
 * @param v             Handle to the video
 */
void video_rewind(video_t *v);

/**
 * @brief Seek the video to a specific frame index
 * 
 * In general, video codecs only allow fast seeking to keyframes. If the
 * specified frame index is not a keyframe, the videoplayer will seek to the closest
 * previous keyframe. In that case, the client will need to call #video_next_frame
 * repeatedly until the desired frame index is reached.
 * 
 * Seeking can be done instantly if the seek index has been generated at build
 * time (using videoconv64 --seek). The seek index is stored in a file with
 * the same name as the video, but with .seek extension. If no seek index file
 * is found, seeking will be performed by linearly scanning the video, and
 * will thus be slower.
 * 
 * @param v             Handle to the video
 * @param frame_idx     Index of the frame to seek to
 * @return              Actual frame index seeked to, or -1 if the codec does not
 *                      support seeking (with or without the seektable).
 */
int video_seek(video_t *v, int frame_idx);

/**
 * @brief Check if a frame index is a seekpoint (keyframe) according to the seektable
 *
 * This function returns true only if a seek index (.seek file) has been loaded for
 * the specified video and the specified frame index is an exact entry in that table.
 * If no seek index is available, the function returns false.
 *
 * @param v             Handle to the video
 * @param frame_idx     Frame index to check
 * @return true         If the frame index is a seekpoint (keyframe)
 * @return false        Otherwise (including missing seektable)
 */
bool video_is_seekable(video_t *v, int frame_idx);

/**
 * @brief Close the video file
 * 
 * @param v             Handle to the video
 */
void video_close(video_t *v);

#ifdef __cplusplus
}
#endif

#endif