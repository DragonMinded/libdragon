/**
 * @file h264.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief RSP-accelerated H.264 video player
 */
#ifndef LIBDRAGON_H264_H
#define LIBDRAGON_H264_H

#include "video.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief H.264 video codec
 * 
 * Register this codec via #video_register_codec to enable H.264 video playback
 * using the video.h API.
 */
extern video_codec_t h264_codec;

#ifdef __cplusplus
}
#endif

#endif