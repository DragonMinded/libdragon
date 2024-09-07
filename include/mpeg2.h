#ifndef __LIBDRAGON_MPEG2_H
#define __LIBDRAGON_MPEG2_H

#include "display.h"
#include "rspq.h"
#include "yuv.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mpeg2_s mpeg2_t;

mpeg2_t* mpeg2_open(const char *fn);
float mpeg2_get_framerate(mpeg2_t *mp2);
int mpeg2_get_width(mpeg2_t *mp2);
int mpeg2_get_height(mpeg2_t *mp2);
bool mpeg2_next_frame(mpeg2_t *mp2);
yuv_frame_t mpeg2_get_frame(mpeg2_t *mp2);
void mpeg2_rewind(mpeg2_t *mp2);
void mpeg2_close(mpeg2_t *mp2);

#ifdef __cplusplus
}
#endif

#endif
