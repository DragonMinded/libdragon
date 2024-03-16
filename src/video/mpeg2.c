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
#include <errno.h>
#include "mpeg1_internal.h"

#define YUV_MODE   1   // 0=CPU, 1=RSP+RDP

DEFINE_RSP_UCODE(rsp_mpeg1);

static uint32_t ovl_id;
static bool rsp_mpeg1_initialized = false;

void rsp_mpeg1_init(void) {
	if (rsp_mpeg1_initialized) return;
	rspq_init();
	ovl_id = rspq_overlay_register(&rsp_mpeg1);
	rsp_mpeg1_initialized = true;
}

void rsp_mpeg1_close(void) {
	if (!rsp_mpeg1_initialized) return;
	rspq_overlay_unregister(ovl_id); ovl_id = 0;
	rsp_mpeg1_initialized = false;
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

void mpeg2_open(mpeg2_t *mp2, const char *fn, int output_width, int output_height) {
	memset(mp2, 0, sizeof(mpeg2_t));

	rsp_mpeg1_init();

	mp2->buf = plm_buffer_create_with_filename(fn);
	assertf(mp2->buf, "error opening file %s: %s\n", fn, strerror(errno));

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

		// Create a YUV blitter for this resolution
		mp2->yuv_blitter = yuv_blitter_new_fmv(
			width, height,
			output_width, output_height,
			NULL);
	}

	profile_init();
}

bool mpeg2_next_frame(mpeg2_t *mp2) {
	PROFILE_START(PS_MPEG, 0);
	mp2->f = plm_video_decode(mp2->v);
	PROFILE_STOP(PS_MPEG, 0);
	return (mp2->f != NULL);
}

void mpeg2_rewind(mpeg2_t *mp2) {
	plm_video_rewind(mp2->v);
}

void mpeg2_draw_frame(mpeg2_t *mp2, display_context_t disp) {
	PROFILE_START(PS_YUV, 0);
	if (YUV_MODE == 0) {	
	    plm_frame_to_rgba(mp2->f, disp->buffer, disp->stride);
	} else {
		plm_frame_t *frame = mp2->f;
		surface_t yp = surface_make_linear(frame->y.data, FMT_I8, frame->width, frame->height);
		surface_t cbp = surface_make_linear(frame->cb.data, FMT_I8, frame->width/2, frame->height/2);
		surface_t crp = surface_make_linear(frame->cr.data, FMT_I8, frame->width/2, frame->height/2);
		yuv_blitter_run(&mp2->yuv_blitter, &yp, &cbp, &crp);
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

void mpeg2_close(mpeg2_t *mp2) {
	plm_video_destroy(mp2->v);
	plm_buffer_destroy(mp2->buf);
	if (YUV_MODE == 1) yuv_blitter_free(&mp2->yuv_blitter);
	memset(mp2, 0, sizeof(mpeg2_t));
}
