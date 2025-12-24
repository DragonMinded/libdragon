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

h264_t* h264_open(const char *fn);
float h264_get_framerate(h264_t *v);
int h264_get_width(h264_t *v);
int h264_get_height(h264_t *v);
/**
 * @brief Get the YUV colorspace for the stream.
 *
 * This function inspects the H.264 VUI fields (matrix_coefficients and
 * video_full_range_flag) when present, and maps them to one of libdragon's
 * predefined colorspaces (BT.601/BT.709, TV/Full range).
 *
 * @return A colorspace structure (by value). Defaults to #YUV_BT601_TV if
 *         metadata is missing/unknown.
 */
yuv_colorspace_t h264_get_colorspace(h264_t *v);
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