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

/**
 * @brief Register MPEG1 and initialize its RSP overlay.
 * Call before starting the audio mixer. Safe to call more than once.
 */
void mpeg1_init(void);

/**
 * @brief Decode the next MPEG1 frame, flushing the delayed reference at EOF.
 * @param video MPEG1 video handle.
 */
bool mpeg1_next_frame_flush(video_t *video);

/**
 * @brief Decode the reference frame after #video_seek.
 * Skips up to two leading orphan B pictures (three decode attempts).
 * @param video MPEG1 video handle.
 * @param frame Frame index returned by #video_seek.
 * @return true if a reference frame is available, false otherwise.
 */
bool mpeg1_seek_next(video_t *video, int frame);

#ifdef __cplusplus
}
#endif

#endif
