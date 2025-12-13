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
bool h264_next_frame(h264_t *v);
yuv_frame_t h264_get_frame(h264_t *v);
void h264_rewind(h264_t *v);
void h264_close(h264_t *v);

#ifdef __cplusplus
}
#endif

#endif