#ifndef __LIBDRAGON_MPEG2_H
#define __LIBDRAGON_MPEG2_H

#include "display.h"
#include "rspq.h"
#include <stdbool.h>

typedef struct plm_t plm_t;
typedef struct plm_buffer_t plm_buffer_t;
typedef struct plm_video_t plm_video_t;

typedef struct {
	plm_buffer_t *buf;
	plm_video_t *v;
	void *f;
	rspq_block_t* yuv_convert;
} mpeg2_t;

void mpeg2_open(mpeg2_t *mp2, const char *fn);
bool mpeg2_next_frame(mpeg2_t *mp2);
void mpeg2_draw_frame(mpeg2_t *mp2, display_context_t disp);
void mpeg2_close(mpeg2_t *mp2);

#endif
