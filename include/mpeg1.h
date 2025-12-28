/**
 * @file mpeg1.h
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
