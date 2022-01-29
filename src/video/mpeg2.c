#include "mpeg2.h"
#include "n64sys.h"
#include "rdp.h"
#include "rdp_commands.h"
#include "yuv.h"
#include "yuvblit.h"
#include "debug.h"
#include "profile.h"
#include "utils.h"
#include <assert.h>
#include "mpeg1_internal.h"

#define YUV_MODE   1   // 0=CPU, 1=RSP+RDP, 2=DLAIR

#define BLOCK_W 32
#define BLOCK_H 16

DEFINE_RSP_UCODE(rsp_mpeg1);

void rsp_mpeg1_init(void) {
	rspq_init();
	rspq_overlay_register(&rsp_mpeg1, 0x5);
}

void rsp_mpeg1_load_matrix(int16_t *mtx) {
	assert((PhysicalAddr(mtx) & 7) == 0);
	data_cache_hit_writeback(mtx, 8*8*2);
	rspq_write(0x50, PhysicalAddr(mtx));
}

void rsp_mpeg1_store_matrix(int16_t *mtx) {
	assert((PhysicalAddr(mtx) & 7) == 0);
	data_cache_hit_writeback_invalidate(mtx, 8*8*2);
	rspq_write(0x57, PhysicalAddr(mtx));
}

void rsp_mpeg1_store_pixels(void) {
	rspq_write(0x51);
}

void rsp_mpeg1_load_pixels(void) {
	rspq_write(0x5C);
}

void rsp_mpeg1_zero_pixels(void) {
	rspq_write(0x5D);
}

void rsp_mpeg1_idct(void) {
	rspq_write(0x52);
}

void rsp_mpeg1_block_begin(int block, uint8_t *pixels, int pitch) {
	assert((PhysicalAddr(pixels) & 7) == 0);
	assert((pitch & 7) == 0);
	assert(block == RSP_MPEG1_BLOCK_Y0 || block == RSP_MPEG1_BLOCK_CR || block == RSP_MPEG1_BLOCK_CB);
	rspq_write(0x53, block, PhysicalAddr(pixels), pitch);
}

void rsp_mpeg1_block_switch_partition(int partition) {
	rspq_write(0x5B, partition);
}

void rsp_mpeg1_block_coeff(int idx, int16_t coeff) {
	rspq_write(0x54, ((idx & 0x3F) << 16) | (uint16_t)coeff);
}

void rsp_mpeg1_block_dequant(bool intra, int scale) {
	rspq_write(0x55, (int)intra | (scale << 8));
}

void rsp_mpeg1_block_decode(int ncoeffs, bool intra) {
	rspq_write(0x56, ncoeffs, intra);
}

void rsp_mpeg1_block_predict(uint8_t *src, int pitch, bool oddh, bool oddv, bool interpolate) {
	rspq_write(0x5A, PhysicalAddr(src), pitch, oddv | (oddh<<1) | (interpolate<<2));
}

void rsp_mpeg1_set_quant_matrix(bool intra, const uint8_t quant_mtx[64]) {
	uint32_t *qmtx = (uint32_t*)quant_mtx;
	rspq_write(0x58, intra,
		qmtx[0],  qmtx[1],  qmtx[2],  qmtx[3],
		qmtx[4],  qmtx[5],  qmtx[6],  qmtx[7]);
	rspq_write(0x59, intra,
		qmtx[8],  qmtx[9],  qmtx[10], qmtx[11],
		qmtx[12], qmtx[13], qmtx[14], qmtx[15]);
}

void rsp_mpeg1_block_split(void) {
	rspq_write(0x5E);	
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
	rdp_set_default_clipping();
	if (screen_height > video_height || screen_width > video_width) {
		rdp_sync_pipe();
		rdp_set_other_modes(SOM_CYCLE_FILL);
		rdp_set_fill_color(0);
		if (screen_height > video_height) {		
			rdp_fill_rectangle(0, 0, screen_width-1, ystart-1);
			rdp_fill_rectangle(0, ystart+video_height, screen_width-1, screen_height-1);
		}
		if (screen_width > video_width) {
			rdp_fill_rectangle(0, ystart, xstart+1, ystart+video_height-1);
			rdp_fill_rectangle(xstart+video_width, ystart, screen_width-1, ystart+video_height-1);			
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
	rdp_sync_pipe();
	rdp_set_other_modes(SOM_CYCLE_1 | SOM_RGBDITHER_NONE | SOM_TC_CONV);
	rdp_set_combine_mode(Comb1_Rgb(TEX0, K4, K5, ZERO));

    // BT.601 coefficients (Kr=0.299, Kb=0.114, TV range)
    rdp_set_convert(179,-44,-91,227,19,255);

	rdp_set_tile(RDP_TILE_FORMAT_YUV, RDP_TILE_SIZE_16BIT, BLOCK_W/8, 0, 0, 0,0,0,0,0,0,0,0,0);
	rdp_set_tile(RDP_TILE_FORMAT_YUV, RDP_TILE_SIZE_16BIT, BLOCK_W/8, 0, 1, 0,0,0,0,0,0,0,0,0);
	rdp_set_tile(RDP_TILE_FORMAT_YUV, RDP_TILE_SIZE_16BIT, BLOCK_W/8, 0, 2, 0,0,0,0,0,0,0,0,0);
	rdp_set_tile(RDP_TILE_FORMAT_YUV, RDP_TILE_SIZE_16BIT, BLOCK_W/8, 0, 3, 0,0,0,0,0,0,0,0,0);
	rdp_set_texture_image(PhysicalAddr(interleaved_buffer), RDP_TILE_FORMAT_YUV, RDP_TILE_SIZE_16BIT, width-1);

	int stepx = (int)(1024.0f / scalew);
	int stepy = (int)(1024.0f / scaleh);
	debugf("scalew:%.3f scaleh:%.3f stepx=%x stepy=%x\n", scalew, scaleh, stepx, stepy);
    for (int y=0;y<height;y+=BLOCK_H) {
        for (int x=0;x<width;x+=BLOCK_W) {
        	int sx0 = x * scalew;
        	int sy0 = y * scaleh;
        	int sx1 = (x+BLOCK_W) * scalew;
        	int sy1 = (y+BLOCK_H) * scaleh;

            rdp_load_tile(0, 
            	x<<2, y<<2, 
            	(x+BLOCK_W-1)<<2, (y+BLOCK_H-1)<<2);
            rdp_texture_rectangle(0, 
            	(sx0+xstart)<<2, (sy0+ystart)<<2,
            	(sx1+xstart)<<2, (sy1+ystart)<<2,
            	x<<5, y<<5, stepx, stepy);
            rdp_sync_tile();
        }
		rspq_flush();
    }
}

void mpeg2_open(mpeg2_t *mp2, const char *fn) {
	memset(mp2, 0, sizeof(mpeg2_t));

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

	rsp_mpeg1_init();

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
		extern void *__safe_buffer[];
		extern uint32_t __width;
		#define __get_buffer( x ) __safe_buffer[(x)-1]

		uint8_t *rgb = __get_buffer(disp);
		int stride = __width * 4;
	    plm_frame_to_rgba(mp2->f, rgb, stride);
	} else if (YUV_MODE == 1) {
		plm_frame_t *frame = mp2->f;
		yuv_set_input_buffer(frame->y.data, frame->cb.data, frame->cr.data, frame->width);
		rspq_block_run(mp2->yuv_convert);
		// yuv_draw_frame(frame->width, frame->height);

    } else if (YUV_MODE == 2) {
		plm_frame_t *frame = mp2->f;

    	rsp_yuv_blit_setup();
    	rsp_yuv_blit(frame->y.data, frame->cb.data, frame->cr.data);
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
