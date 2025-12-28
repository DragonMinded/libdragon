/**
 * @file video_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Video player subsystem
 * 
 * 
 */

#ifndef LIBDRAGON_VIDEO_INTERNAL_H
#define LIBDRAGON_VIDEO_INTERNAL_H

#include "video.h"

/** @brief Basic video structure */
typedef struct video_s {
    video_info_t info;          ///< Video information
    video_codec_t *codec;       ///< Codec used to decode this video
} video_t;

/** @brief Video codec structure */
typedef struct video_codec_s {
    const char *extension;                  ///< File extension handled by this codec
    video_t* (*open)(const char *fn);       ///< Open a video file
    void (*close)(video_t *v);              ///< Close a video file
    int (*poll)(video_t *v);                ///< Decode a bit
    bool (*next_frame)(video_t *v);         ///< Advance to the next frame
    yuv_frame_t (*get_frame)(video_t *v);   ///< Get the current frame
    void (*rewind)(video_t *v);             ///< Rewind the video to the beginning
    int (*seek)(video_t *v, int frame);     ///< Seek to a specific frame

    struct video_codec_s* next_codec;       ///< Next registered codec
} video_codec_t;


#endif
