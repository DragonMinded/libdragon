/**
 * @file mpeg1.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#include "mpeg1.h"
#include "n64sys.h"
#include "rspq.h"
#include "rdpq.h"
#include "rdpq_rect.h"
#include "rdpq_mode.h"
#include "yuv.h"
#include "debug.h"
#include "profile.h"
#include "utils.h"
#include "video_internal.h"
#include <assert.h>
#include <errno.h>
#include "mpeg1_internal.h"

typedef struct mpeg1_s {
	video_t video;
	plm_buffer_t *buf;
	plm_video_t *v;
	void *f;
} mpeg1_t;

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

static video_t *mpeg1_open(const char *fn) {
	mpeg1_t *mp1 = malloc(sizeof(mpeg1_t));
	assertf(mp1, "Out of memory");
	memset(mp1, 0, sizeof(mpeg1_t));

	rsp_mpeg1_init();

	mp1->buf = plm_buffer_create_with_filename(fn);
	assertf(mp1->buf, "error opening file %s: %s\n", fn, strerror(errno));

	// In the common case of accessing a movie stream
	// from the ROM, disable buffering. This will allow
	// the data to flow directly from the DMA into the buffers
	// without any intervening memcpy. We keep buffering on
	// for other supports like SD cards.
	if (strncmp(fn, "rom:/", 5) == 0) {
		setvbuf(mp1->buf->fh, NULL, _IONBF, 0);
	}

	mp1->v = plm_video_create_with_buffer(mp1->buf, true);
	assert(mp1->v);

	// Force decoding of header. This would be done lazily but better do it
	// now to catch any errors early.
	if (!plm_video_has_header(mp1->v)) {
		assertf(0, "invalid header in video stream\n");
	}

	// Fill in information structure
	mp1->video.info.width = plm_video_get_width(mp1->v);
	mp1->video.info.height = plm_video_get_height(mp1->v);
	mp1->video.info.framerate = plm_video_get_framerate(mp1->v);
	mp1->video.info.aspect_ratio = (float)plm_video_get_aspect_ratio(mp1->v);

	// MPEG1 does not specify colorspace. Historically, the vast amount of MPEG1
	// streams were encoded using BT.601 standard with TV levels, and that's also
	// the default for ffmpeg when encoding MPEG1, so we assume that here.
	mp1->video.info.colorspace = YUV_BT601_TV;

	return &mp1->video;
}

static bool mpeg1_next_frame(video_t *v) {
	mpeg1_t *mp1 = (mpeg1_t *)v;
	PROFILE_START(PS_MPEG, 0);
	mp1->f = plm_video_decode(mp1->v);
	PROFILE_STOP(PS_MPEG, 0);
	return (mp1->f != NULL);
}

static void mpeg1_rewind(video_t *v) {
	mpeg1_t *mp1 = (mpeg1_t *)v;
	plm_video_rewind(mp1->v);
}

static yuv_frame_t mpeg1_get_frame(video_t *v) {
	mpeg1_t *mp1 = (mpeg1_t *)v;
	plm_frame_t *frame = mp1->f;
	surface_t yp  = surface_make_linear(frame->y.data,  FMT_I8, frame->width,   frame->height);
	surface_t cbp = surface_make_linear(frame->cb.data, FMT_I8, frame->width/2, frame->height/2);
	surface_t crp = surface_make_linear(frame->cr.data, FMT_I8, frame->width/2, frame->height/2);
	return (yuv_frame_t){ .y = yp, .u = cbp, .v = crp };
}

static void mpeg1_close(video_t *v) {
	mpeg1_t *mp1 = (mpeg1_t *)v;
	plm_video_destroy(mp1->v);
	free(mp1);
}

video_codec_t mpeg1_codec = {
	.extension = ".m1v",
	.open = mpeg1_open,
	.close = mpeg1_close,
	.next_frame = mpeg1_next_frame,
	.get_frame = mpeg1_get_frame,
	.rewind = mpeg1_rewind,
};
