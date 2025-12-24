/**
 * @file mpeg2.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief RSP-accelerated MPEG video player
 * 
 * @note Even though the library is called "mpeg2.h", it only supports MPEG 1.
 * 
 * This library allows to play MPEG1 videos on the N64, accelerating a large
 * part of the decoding using the RSP. This makes up for a quite fast
 * playback of videos, that allows for a higher bitrate. Higher-level
 * information on how to encode videos and suggested settings can be found
 * in the Libdragon wiki: https://github.com/DragonMinded/libdragon/wiki/MPEG1-Player
 * 
 * The API in this file is quite simple. The main entry point is #mpeg2_open,
 * which opens a video file and returns a handle to it. The handle can be used
 * to query information about the video, such as width, height, framerate, etc.
 * 
 * To play the video, the main loop should call #mpeg2_next_frame any time
 * a new frame is needed (depending on the desired playback frequency),
 * and then #mpeg2_get_frame to get the frame to display. The frame is
 * returned as a #yuv_frame_t. You can then use the YUV library to display
 * it, either fullscreen, in a smaller portion, or even into an offscreen
 * surface to be used eg. like a texture.
 * 
 * Notice that the time required to decode a frame is not constant, and can
 * vary a lot; especially I-frames tend to be much heavier to decode, so if
 * possible allow for some buffering to avoid slowdowns.
 * 
 */
#ifndef LIBDRAGON_MPEG2_H
#define LIBDRAGON_MPEG2_H

#include <stdbool.h>
#include "yuv.h"

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct yuv_frame_s yuv_frame_t;
typedef struct mpeg2_s mpeg2_t;
///@endcond

/**
 * @brief Open an MPEG1 video file
 * 
 * This function opens an MPEG1 video file and returns a handle to it.
 * 
 * The file must be a raw MPEG-1 stream with no audio. This means that standard
 * .mpg files (using the MPEG container format) are not directly
 * supported and must be split into the raw video and audio streams. MPEG 1
 * video streams often have a .m1v extension.
 * 
 * @param fn            Filename of the video to open (including filesystem prefix)
 * @return mpeg2_t*     Handle to the video
 * 
 * @see #mpeg2_next_frame
 * @see #mpeg2_close
 */
mpeg2_t* mpeg2_open(const char *fn);

/** 
 * @brief Get the framerate of the video
 * 
 * This function returns the expected playback rate of the video in frames per
 * second, as encoded in the video header.
 * 
 * Notice that this library does not by itself enforce the framerate. Any
 * time a frame is requested, the library will decode the next frame in the
 * stream, regardless of the time elapsed since the last frame. It is up to the
 * caller to decide how to handle the timing.
 * 
 * @param mp2           Handle to the video
 * @return float        Framerate in frames per second
 */
float mpeg2_get_framerate(mpeg2_t *mp2);

/**
 * @brief Return the width of the video in pixels
 * 
 * @param mp2           Handle to the video
 * @return int          Width of the video in pixels
 */
int mpeg2_get_width(mpeg2_t *mp2);

/**
 * @brief Return the height of the video in pixels
 * 
 * @param mp2           Handle to the video
 * @return int          Height of the video in pixels
 */
int mpeg2_get_height(mpeg2_t *mp2);

/**
 * @brief Get the display aspect ratio (DAR) of the stream.
 *
 * For normal video streams with square pixels, this will be identical
 * to width/height. Some streams might be anamorphic, meaning that the picture
 * has been encoded with non-square pixels. In this case, the aspect ratio
 * will be different, and must be used to correctly display the video.
 * 
 * Notice that MPEG-1 has very limited support for aspect ratios. To allow for
 * full anamorphic support, we support a libdragon-specific user data fragment
 * embedded in the video stream, which allows to specify arbitrary aspect ratios.
 * This user-data fragment is created by videoconv64, but will be missing if
 * the video is encoded using other tools.
 */
float mpeg2_get_aspect_ratio(mpeg2_t *mp2);

/**
 * @brief Get the YUV colorspace for the stream.
 *
 * This function is provided for completeness, but all MPEG-1 videos must/should be
 * encoded as BT.601 with TV range, so this function will always return
 * #YUV_BT601_TV.
 *
 * @return A colorspace structure (by value).
 */
yuv_colorspace_t mpeg2_get_colorspace(mpeg2_t *mp2);

/**
 * @brief Decode the next frame in the video stream
 * 
 * This function decodes the next frame in the video stream. If the frame
 * is successfully decoded, it can be retrieved with #mpeg2_get_frame. Otherwise,
 * the stream is finished and the function will return false.
 * 
 * @param mp2           Handle to the video
 * @return true         If a frame was successfully decoded
 * @return false        If the stream is finished
 */
bool mpeg2_next_frame(mpeg2_t *mp2);

/**
 * @brief Get the last decoded frame
 * 
 * This function returns the last frame decoded by #mpeg2_next_frame. The frame
 * is returned as a #yuv_frame_t, which can be used to display the frame on the
 * screen via the yuv.h library.
 * 
 * @param mp2               Handle to the video
 * @return yuv_frame_t      Decoded frame
 * 
 * @see #yuv_tex_blit
 * @see #yuv_blitter_new_fmv
 * @see #yuv_blitter_new
 */
yuv_frame_t mpeg2_get_frame(mpeg2_t *mp2);

/**
 * @brief Rewind the video stream to the beginning
 * 
 * This function rewinds the video stream to the beginning, so that the next
 * call to #mpeg2_next_frame will start decoding from the first frame.
 * 
 * @param mp2               Handle to the video
 */
void mpeg2_rewind(mpeg2_t *mp2);

/**
 * @brief Close the video stream and release resources
 * 
 * @param mp2               Handle to the video
 */
void mpeg2_close(mpeg2_t *mp2);

#ifdef __cplusplus
}
#endif

#endif
