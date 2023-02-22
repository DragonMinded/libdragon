#include "mpeg2.h"
#include "n64sys.h"
#include "rdpq.h"
#include "rdpq_rect.h"
#include "rdpq_mode.h"
#include "yuv.h"
#include "debug.h"
#include "profile.h"
#include "utils.h"
#include <assert.h>
#include "mpeg1_internal.h"

#define YUV_MODE   1   // 0=CPU, 1=RSP+RDP

#define BLOCK_W 32
#define BLOCK_H 16

DEFINE_RSP_UCODE(rsp_mpeg1);

static uint32_t ovl_id;

void rsp_mpeg1_init(void) {
	rspq_init();
	ovl_id = rspq_overlay_register(&rsp_mpeg1);
}

void rsp_mpeg1_load_matrix(int16_t *mtx) {
	assert((PhysicalAddr(mtx) & 7) == 0);
	data_cache_hit_writeback(mtx, 8*8*2);
	rspq_write(ovl_id, 0x0, PhysicalAddr(mtx));
}

void rsp_mpeg1_store_matrix(int16_t *mtx) {
	assert((PhysicalAddr(mtx) & 7) == 0);
	data_cache_hit_writeback_invalidate(mtx, 8*8*2);
	rspq_write(ovl_id, 0x7, PhysicalAddr(mtx));
}

void rsp_mpeg1_store_pixels(void) {
	rspq_write(ovl_id, 0x1);
}

void rsp_mpeg1_load_pixels(void) {
	rspq_write(ovl_id, 0xC);
}

void rsp_mpeg1_zero_pixels(void) {
	rspq_write(ovl_id, 0xD);
}

void rsp_mpeg1_idct(void) {
	rspq_write(ovl_id, 0x2);
}

void rsp_mpeg1_block_begin(int block, uint8_t *pixels, int pitch) {
	assert((PhysicalAddr(pixels) & 7) == 0);
	assert((pitch & 7) == 0);
	assert(block == RSP_MPEG1_BLOCK_Y0 || block == RSP_MPEG1_BLOCK_CR || block == RSP_MPEG1_BLOCK_CB);
	rspq_write(ovl_id, 0x3, block, PhysicalAddr(pixels), pitch);
}

void rsp_mpeg1_block_switch_partition(int partition) {
	rspq_write(ovl_id, 0xB, partition);
}

void rsp_mpeg1_block_coeff(int idx, int16_t coeff) {
	rspq_write(ovl_id, 0x4, ((idx & 0x3F) << 16) | (uint16_t)coeff);
}

void rsp_mpeg1_block_dequant(bool intra, int scale) {
	rspq_write(ovl_id, 0x5, (int)intra | (scale << 8));
}

void rsp_mpeg1_block_decode(int ncoeffs, bool intra) {
	rspq_write(ovl_id, 0x6, ncoeffs, intra);
}

void rsp_mpeg1_block_predict(uint8_t *src, int pitch, bool oddh, bool oddv, bool interpolate) {
	rspq_write(ovl_id, 0xA, PhysicalAddr(src), pitch, oddv | (oddh<<1) | (interpolate<<2));
}

void rsp_mpeg1_set_quant_matrix(bool intra, const uint8_t quant_mtx[64]) {
	uint32_t *qmtx = (uint32_t*)quant_mtx;
	rspq_write(ovl_id, 0x8, intra,
		qmtx[0],  qmtx[1],  qmtx[2],  qmtx[3],
		qmtx[4],  qmtx[5],  qmtx[6],  qmtx[7]);
	rspq_write(ovl_id, 0x9, intra,
		qmtx[8],  qmtx[9],  qmtx[10], qmtx[11],
		qmtx[12], qmtx[13], qmtx[14], qmtx[15]);
}

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg/pl_mpeg.h"

#define VIDEO_WIDTH 480
#define VIDEO_HEIGHT 272

enum ZoomMode {
	ZOOM_NONE,
	ZOOM_KEEP_ASPECT,
	ZOOM_FULL
};

static void yuv_draw_frame(int width, int height, enum ZoomMode mode) {
	static uint8_t *interleaved_buffer = NULL;

	if (!interleaved_buffer) {
		interleaved_buffer = malloc_uncached(width*height*2);
		assert(interleaved_buffer);
	}

	// Calculate initial Y to center the frame on the screen (letterboxing)
	int screen_width = display_get_width();
	int screen_height = display_get_height();
	int video_width = width;
	int video_height = height;
	float scalew = 1.0f, scaleh = 1.0f;

	if (mode != ZOOM_NONE && width < screen_width && height < screen_height) {
		scalew = (float)screen_width / (float)width;
		scaleh = (float)screen_height / (float)height;
		if (mode == ZOOM_KEEP_ASPECT)
			scalew = scaleh = MIN(scalew, scaleh);

		video_width = width * scalew;
		video_height = height *scaleh;
	}

	int xstart = (screen_width - video_width) / 2;
	int ystart = (screen_height - video_height) / 2;

	// Start clearing the screen
	if (screen_height > video_height || screen_width > video_width) {
		rdpq_set_mode_fill(RGBA32(0,0,0,0));
		if (screen_height > video_height) {		
			rdpq_fill_rectangle(0, 0, screen_width, ystart);
			rdpq_fill_rectangle(0, ystart+video_height, screen_width, screen_height);
		}
		if (screen_width > video_width) {
			rdpq_fill_rectangle(0, ystart, xstart, ystart+video_height);
			rdpq_fill_rectangle(xstart+video_width, ystart, screen_width, ystart+video_height);
		}
	}

	// RSP YUV converts in blocks of 32x16
	yuv_set_output_buffer(interleaved_buffer, width*2);
	for (int y=0; y < height; y += 16) {
		for (int x=0; x < width; x += 32) {
			yuv_interleave_block_32x16(x, y);
		}
		rspq_flush();
	}

	// Configure YUV blitting mode
	rdpq_set_mode_yuv(false);

	rdpq_set_tile(0, FMT_YUV16, 0, BLOCK_W, 0);
	rdpq_set_tile(1, FMT_YUV16, 0, BLOCK_W, 0);
	rdpq_set_tile(2, FMT_YUV16, 0, BLOCK_W, 0);
	rdpq_set_tile(3, FMT_YUV16, 0, BLOCK_W, 0);
	rdpq_set_texture_image_raw(0, PhysicalAddr(interleaved_buffer), FMT_YUV16, width, height);

	debugf("scalew:%.3f scaleh:%.3f\n", scalew, scaleh);
    for (int y=0;y<height;y+=BLOCK_H) {
        for (int x=0;x<width;x+=BLOCK_W) {
        	int sx0 = x * scalew;
        	int sy0 = y * scaleh;
        	int sx1 = (x+BLOCK_W) * scalew;
        	int sy1 = (y+BLOCK_H) * scaleh;

            rdpq_load_tile(0, x, y, x+BLOCK_W, y+BLOCK_H);
            rdpq_texture_rectangle_scaled(0,
				sx0+xstart, sy0+ystart,
				sx1+xstart, sy1+ystart,
				x, y, x+BLOCK_W, y+BLOCK_H);
        }
		rspq_flush();
    }
}

void mpeg2_open(mpeg2_t *mp2, const char *fn) {
	memset(mp2, 0, sizeof(mpeg2_t));

	rsp_mpeg1_init();

	mp2->buf = plm_buffer_create_with_filename(fn);
	assertf(mp2->buf, "File not found: %s", fn);

	// In the common case of accessing a movie stream
	// from the ROM, disable buffering. This will allow
	// the data to flow directly from the DMA into the buffers
	// without any intervening memcpy. We keep buffering on
	// for other supports like SD cards.
	if (strncmp(fn, "rom:/", 5) == 0) {
		setvbuf(mp2->buf->fh, NULL, _IONBF, 0);
	}

	mp2->v = plm_video_create_with_buffer(mp2->buf, 1);
	assert(mp2->v);

	// Fetch resolution. These calls will automatically decode enough of the
	// stream header to acquire these data.
	int width = plm_video_get_width(mp2->v);
	int height = plm_video_get_height(mp2->v);

	debugf("Resolution: %dx%d\n", width, height);

	if (YUV_MODE == 1) {
		yuv_init();
		// assert(width % BLOCK_W == 0);
		assert(height % BLOCK_H == 0);

		if (mp2->yuv_convert) {
			rspq_block_free(mp2->yuv_convert);
		}
		rspq_block_begin();
		yuv_draw_frame(width, height, ZOOM_KEEP_ASPECT);
		mp2->yuv_convert = rspq_block_end();
	}

	profile_init();
}

bool mpeg2_next_frame(mpeg2_t *mp2) {
	PROFILE_START(PS_MPEG, 0);
	mp2->f = plm_video_decode(mp2->v);
	PROFILE_STOP(PS_MPEG, 0);
	return (mp2->f != NULL);
}

void mpeg2_draw_frame(mpeg2_t *mp2, display_context_t disp) {
	PROFILE_START(PS_YUV, 0);
	if (YUV_MODE == 0) {	
	    plm_frame_to_rgba(mp2->f, disp->buffer, disp->stride);
	} else {
		plm_frame_t *frame = mp2->f;
		yuv_set_input_buffer(frame->y.data, frame->cb.data, frame->cr.data, frame->width);
		rspq_block_run(mp2->yuv_convert);
		// yuv_draw_frame(frame->width, frame->height);
    }
	PROFILE_STOP(PS_YUV, 0);

    static int nframes=0;
    profile_next_frame();
    if (++nframes % 128 == 0) {
    	profile_dump();
    	profile_init();
    }
}

float mpeg2_get_framerate(mpeg2_t *mp2) {
	return plm_video_get_framerate(mp2->v);
}
