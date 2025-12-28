/**
 * @file mpeg1.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief RSP-accelerated MPEG video player
 */
#ifndef LIBDRAGON_MPEG2_H
#define LIBDRAGON_MPEG2_H

#include "video.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MPEG1 video codec
 * 
 * Register this codec via #video_register_codec to enable MPEG1 video playback
 * using the video.h API.
 * 
 * This codec supports MPEG1 video elementary streams with no container. The supported
 * extension is ".m1v". Do not use this codec with container formats such as MPG
 * or AVI.
 */
extern video_codec_t mpeg1_codec;

#ifdef __cplusplus
}
#endif

#endif
