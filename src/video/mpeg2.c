#include "mpeg2.h"
#include "n64sys.h"
#include "rdp.h"
#include "rdp_commands.h"
#include "yuv.h"
#include "yuvblit.h"
#include "debug.h"
#include "profile.h"
#include <assert.h>


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

void rsp_mpeg1_store_pixels(int8_t *pixels) {
	assert((PhysicalAddr(pixels) & 7) == 0);
	data_cache_hit_writeback_invalidate(pixels, 8*8);
	rspq_write(0x51, PhysicalAddr(pixels));
}

void rsp_mpeg1_idct(void) {
	rspq_write(0x52);
}

void rsp_mpeg1_set_block(uint8_t *pixels, int pitch) {
	for (int i=0;i<8;i++)
		data_cache_hit_writeback_invalidate(pixels+i*pitch, 8);
	rspq_write(0x53, PhysicalAddr(pixels), pitch);
}

void rsp_mpeg1_decode_block(int ncoeffs, bool intra) {
	rspq_write(0x54, ncoeffs, intra);
}

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg/pl_mpeg.h"

static void yuv_draw_frame(int width, int height) {
	static uint8_t interleaved_buffer[320*240*2] __attribute__((aligned(16)));
	#define YSTART 0

	// RSP YUV converts in blocks of 32x16
	yuv_set_output_buffer(interleaved_buffer, 320*2);
	for (int y=0; y < height; y += 16) {
		for (int x=0; x < width; x += 32) {
			yuv_interleave_block_32x16(x, y);
		}
		rspq_flush();
	}

	rdp_set_clipping(0, 0, 319, 219);
	rdp_set_other_modes(SOM_CYCLE_1 | SOM_RGBDITHER_NONE | SOM_TC_CONV);
	rdp_set_combine_mode(Comb1_Rgb(TEX0, K4, K5, ZERO));

    // BT.601 coefficients (Kr=0.299, Kb=0.114, TV range)
    rdp_set_convert(179,-44,-91,227,19,255);

	rdp_set_tile(RDP_TILE_FORMAT_YUV, RDP_TILE_SIZE_16BIT, BLOCK_W/8, 0, 0, 0,0,0,0,0,0,0,0,0);
	rdp_set_tile(RDP_TILE_FORMAT_YUV, RDP_TILE_SIZE_16BIT, BLOCK_W/8, 0, 1, 0,0,0,0,0,0,0,0,0);
	rdp_set_tile(RDP_TILE_FORMAT_YUV, RDP_TILE_SIZE_16BIT, BLOCK_W/8, 0, 2, 0,0,0,0,0,0,0,0,0);
	rdp_set_tile(RDP_TILE_FORMAT_YUV, RDP_TILE_SIZE_16BIT, BLOCK_W/8, 0, 3, 0,0,0,0,0,0,0,0,0);
	rdp_set_texture_image(PhysicalAddr(interleaved_buffer), RDP_TILE_FORMAT_YUV, RDP_TILE_SIZE_16BIT, 320-1);

    for (int y=0;y<height/BLOCK_H;y++) {
        for (int x=0;x<width/BLOCK_W;x++) {
            rdp_load_tile(0, 
            	(x*BLOCK_W)<<2, (y*BLOCK_H)<<2, 
            	(x*BLOCK_W+BLOCK_W-1)<<2, (y*BLOCK_H+BLOCK_H-1)<<2);
            rdp_texture_rectangle(0, 
            	(x*BLOCK_W)<<2, (y*BLOCK_H+YSTART)<<2, 
            	(x*BLOCK_W+BLOCK_W)<<2, (y*BLOCK_H+BLOCK_H+YSTART)<<2, 
            	(x*BLOCK_W)<<5, (y*BLOCK_H)<<5, 1<<10, 1<<10);
            rdp_sync_tile();
        }
		rspq_flush();
    }
}

void mpeg2_open(mpeg2_t *mp2, const char *fn) {
	memset(mp2, 0, sizeof(mpeg2_t));

	mp2->buf = plm_buffer_create_with_filename(fn);
	assertf(mp2->buf, "File not found: %s", fn);

	mp2->v = plm_video_create_with_buffer(mp2->buf, 1);
	assert(mp2->v);

	mpeg2_next_frame(mp2);
	assert(mp2->f);

	plm_frame_t *frame = mp2->f;
	debugf("Resolution: %dx%d\n", frame->width, frame->height);
	if (YUV_MODE == 1) {
		yuv_init();
		assert(frame->width % BLOCK_W == 0);
		assert(frame->height % BLOCK_H == 0);

		if (mp2->yuv_convert) {
			rspq_block_free(mp2->yuv_convert);
		}
		plm_frame_t *frame = mp2->f;
		rspq_block_begin();
		yuv_draw_frame(frame->width, frame->height);
		mp2->yuv_convert = rspq_block_end();
	}

	profile_init();
}

bool mpeg2_next_frame(mpeg2_t *mp2) {
	mp2->f = plm_video_decode(mp2->v);
	return (mp2->f != NULL);
}

void cpu_yuv_interleave(uint8_t *dst, plm_frame_t *src) {
    uint8_t *sy1 = src->y.data;
    uint8_t *sy2 = sy1+320;
    uint8_t *scb = src->cb.data;
    uint8_t *scr = src->cr.data;

    uint8_t *dst1 = (uint8_t*)dst;
    uint8_t *dst2 = dst1 + 320*2;

    for (int y=0; y<240; y+=2) {
        for (int x=0;x<320;x+=2) {
            uint16_t cb = *scb++;
            uint16_t cr = *scr++;

            *dst1++ = cb;
            *dst1++ = *sy1++;
            *dst1++ = cr;
            *dst1++ = *sy1++;

            *dst2++ = cb;
            *dst2++ = *sy2++;
            *dst2++ = cr;
            *dst2++ = *sy2++;
        }

        sy1 += 320;
        sy2 += 320;
        dst1 += 320*2;
        dst2 += 320*2;
    }
}

void mpeg2_draw_frame(mpeg2_t *mp2, display_context_t disp) {
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

    static int nframes=0;
    profile_next_frame();
    if (++nframes % 128 == 0) {
    	profile_dump();
    }

}
