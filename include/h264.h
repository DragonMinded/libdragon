#ifndef LIBDRAGON_H264_H
#define LIBDRAGON_H264_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct yuv_frame_s yuv_frame_t;
typedef struct h264_s h264_t;
///@endcond

h264_t* h264_open(const char *fn);
float h264_get_framerate(h264_t *v);
int h264_get_width(h264_t *v);
int h264_get_height(h264_t *v);
/**
 * @brief Get the display aspect ratio (DAR) of the stream.
 *
 * This uses the H.264 VUI Sample Aspect Ratio (SAR) if present; otherwise it
 * falls back to square pixels (width/height).
 *
 * @return DAR (eg: 16.0f/9.0f). Returns 0 on error.
 */
float h264_get_aspect_ratio(h264_t *v);
bool h264_next_frame(h264_t *v);
yuv_frame_t h264_get_frame(h264_t *v);
void h264_rewind(h264_t *v);
void h264_close(h264_t *v);

#ifdef __cplusplus
}
#endif

#endif