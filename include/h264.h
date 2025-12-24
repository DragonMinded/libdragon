/**
 * @file h264.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief RSP-accelerated H.264 video player
 */
#ifndef LIBDRAGON_H264_H
#define LIBDRAGON_H264_H

#include <stdint.h>
#include <stdbool.h>
#include "yuv.h"

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct yuv_frame_s yuv_frame_t;
typedef struct h264_s h264_t;
///@endcond

/**
 * @brief Open an H.264 video file
 * 
 * This function opens a raw H.264 video file and returns a handle to it.
 * The file must be a raw H.264 stream with no audio or container. Containers
 * like MP4, MKV, AVI, etc are not supported and should not be used.
 * 
 * Raw streams often have a .h264 extension.
 * 
 * @param fn            Filename of the video to open (including filesystem prefix)
 * @return h264_t*      Handle to the video
 */
h264_t* h264_open(const char *fn);

/** @brief Get the framerate of the video (in frames per second) */
float h264_get_framerate(h264_t *v);

/** @brief Get the width of the video (in pixels) */
int h264_get_width(h264_t *v);

/** @brief Get the height of the video (in pixels) */
int h264_get_height(h264_t *v);

/** 
 * @brief Get the display aspect ratio (DAR) of the stream (eg: 4.0f/3.0f)
 * 
 * For normal video streams with square pixels, this will be identical
 * to width/height. Some streams might be anamorphic, meaning that the picture
 * has been encoded with non-square pixels. In this case, the aspect ratio
 * will be different, and must be used to correctly display the video.
 */
float h264_get_aspect_ratio(h264_t *v);

/**
 * @brief Get the YUV colorspace for the stream.
 *
 * This function extracts the colorspace information from the H.264 VUI
 * parameters, if present. The colorspace can be provided to the yuv.h library
 * to correctly display the decoded frames.
 *
 * @return A colorspace structure. Defaults to #YUV_BT601_TV if
 *         metadata is missing/unknown.
 */
yuv_colorspace_t h264_get_colorspace(h264_t *v);

/**
 * @brief Decode the next frame in the video stream
 * 
 * This function decodes the next frame in the video stream. If the frame
 * is successfully decoded, it can be retrieved with #h264_get_frame. Otherwise,
 * the stream is finished and the function will return false.
 * 
 * @param mp2           Handle to the video
 * @return true         If a frame was successfully decoded
 * @return false        If the stream is finished
 */
bool h264_next_frame(h264_t *v);

/**
 * @brief Get the last decoded frame
 * 
 * This function returns the last frame decoded by #h264_next_frame. The frame
 * is returned as a #yuv_frame_t, which can be used to display the frame on the
 * screen via the yuv.h library.
 * 
 * @param mp2               Handle to the video
 * @return yuv_frame_t      Decoded frame
 * 
 * @see #yuv_tex_blit
 * @see #yuv_new_blitter_fmv
 * @see #yuv_new_blitter
 */
yuv_frame_t h264_get_frame(h264_t *v);

/**
 * @brief Rewind the video stream to the beginning
 * 
 * This function rewinds the video stream to the beginning, so that the next
 * call to #h264_next_frame will start decoding from the first frame.
 * 
 * @param mp2               Handle to the video
 */
void h264_rewind(h264_t *v);

/**
 * @brief Close the H.264 video file
 * 
 * This function closes the H.264 video file and releases all associated
 * resources.
 * 
 * @param v             Handle to the video
 */
void h264_close(h264_t *v);

#ifdef __cplusplus
}
#endif

#endif