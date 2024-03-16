#include "../src/video/mpeg1_internal.h"

void test_mpeg1_idct(TestContext *ctx) {
	rspq_init(); DEFER(rspq_close());
	rsp_mpeg1_init(); DEFER(rsp_mpeg1_close());

	int16_t matrix1[8*8] __attribute__((aligned(16)));
	uint8_t out1[8*8] __attribute__((aligned(16)));
	int16_t matrix2[8*8] __attribute__((aligned(16)));

	for (int nt=0;nt<256;nt++) {
		SRAND(nt+1);	
		for (int j=0;j<8;j++) {	
			for (int i=0;i<8;i++) {
				matrix1[j*8+i] = RANDN(128)-64;
				matrix2[j*8+i] = matrix1[j*8+i];
			}
		}

		data_cache_hit_writeback_invalidate(out1, sizeof(out1));
		rsp_mpeg1_block_begin(RSP_MPEG1_BLOCK_CB, out1, 8);
		rsp_mpeg1_load_matrix(matrix1);
		rsp_mpeg1_idct();
		rsp_mpeg1_store_pixels();
		rspq_wait();

		// Reference implementation
		extern void plm_video_idct(int16_t *block);
		plm_video_idct(matrix2);

		for (int j=0;j<8;j++) {	
			for (int i=0;i<8;i++) {
				ASSERT_EQUAL_SIGNED((int8_t)out1[j*8+i], matrix2[j*8+i],
					"IDCT failure at %d,%d (nt:%d)", j, i, nt);
			}
		}
	}
}

void test_mpeg1_block_decode(TestContext *ctx) {
	rspq_init(); DEFER(rspq_close());
	rsp_mpeg1_init(); DEFER(rsp_mpeg1_close());

	int16_t matrix1[8*8] __attribute__((aligned(16)));
	uint8_t pixels1[8*8] __attribute__((aligned(16)));

	int16_t matrix2[8*8] __attribute__((aligned(16)));
	uint8_t pixels2[8*8] __attribute__((aligned(16)));

	for (int intra=0;intra<2;intra++) {
		for (int ncoeffs=1;ncoeffs<3;ncoeffs++) {
			for (int nt=0;nt<256;nt++) {	
				for (int j=0;j<8;j++) {	
					for (int i=0;i<8;i++) {
						if (ncoeffs==1) {							
							// DC coefficient: already a delta
							// for pixels
							if (i==0 && j == 0)
								matrix2[j*8+i] = matrix1[j*8+i] = RANDN(65536)-32768;
							else 
								matrix2[j*8+i] = matrix1[j*8+i] = 0;
						}
						else
							// AC coefficient: must go through IDCT
							matrix2[j*8+i] = matrix1[j*8+i] = RANDN(256)-128;
						pixels2[j*8+i] = pixels1[j*8+i] = RANDN(256);
					}
				}

				data_cache_hit_writeback_invalidate(pixels1, sizeof(pixels1));
				rsp_mpeg1_block_begin(RSP_MPEG1_BLOCK_CB, pixels1, 8);
				rsp_mpeg1_load_matrix(matrix1);
				if (intra)
					rsp_mpeg1_zero_pixels();
				else
					rsp_mpeg1_load_pixels();
				rsp_mpeg1_block_decode(ncoeffs, intra!=0);
				rsp_mpeg1_store_pixels();

				extern void plm_video_decode_block_residual(int16_t *s, int si, uint8_t *d, int di, int dw, int n, int intra);
				plm_video_decode_block_residual(matrix2, 0, pixels2, 0, 8, ncoeffs, intra);
				rspq_wait();


				for (int j=0;j<8;j++) {	
					for (int i=0;i<8;i++) {
						ASSERT_EQUAL_HEX(pixels1[j*8+i], pixels2[j*8+i],
							"Block decode failure at %d,%d (intra=%d, ncoeffs=%d, nt=%d)", j, i, intra, ncoeffs, nt);
					}
				}
			}
		}
	}
}


void test_mpeg1_block_dequant(TestContext *ctx) {
	static const uint8_t PLM_VIDEO_ZIG_ZAG[] = {
		 0,  1,  8, 16,  9,  2,  3, 10,
		17, 24, 32, 25, 18, 11,  4,  5,
		12, 19, 26, 33, 40, 48, 41, 34,
		27, 20, 13,  6,  7, 14, 21, 28,
		35, 42, 49, 56, 57, 50, 43, 36,
		29, 22, 15, 23, 30, 37, 44, 51,
		58, 59, 52, 45, 38, 31, 39, 46,
		53, 60, 61, 54, 47, 55, 62, 63
	};
	static const uint8_t PLM_VIDEO_NON_INTRA_QUANT_MATRIX[] = {
		16, 16, 16, 16, 16, 16, 16, 16,
		16, 16, 16, 16, 16, 16, 16, 16,
		16, 16, 16, 16, 16, 16, 16, 16,
		16, 16, 16, 16, 16, 16, 16, 16,
		16, 16, 16, 16, 16, 16, 16, 16,
		16, 16, 16, 16, 16, 16, 16, 16,
		16, 16, 16, 16, 16, 16, 16, 16,
		16, 16, 16, 16, 16, 16, 16, 16
	};
	static const uint8_t PLM_VIDEO_INTRA_QUANT_MATRIX[] = {
		 8, 16, 19, 22, 26, 27, 29, 34,
		16, 16, 22, 24, 27, 29, 34, 37,
		19, 22, 26, 27, 29, 34, 34, 38,
		22, 22, 26, 27, 29, 34, 37, 40,
		22, 26, 27, 29, 32, 35, 40, 48,
		26, 27, 29, 32, 35, 40, 48, 58,
		26, 27, 29, 34, 38, 46, 56, 69,
		27, 29, 35, 38, 46, 56, 69, 83
	};
	static const uint8_t PLM_VIDEO_PREMULTIPLIER_MATRIX[] = {
		32, 44, 42, 38, 32, 25, 17,  9,
		44, 62, 58, 52, 44, 35, 24, 12,
		42, 58, 55, 49, 42, 33, 23, 12,
		38, 52, 49, 44, 38, 30, 20, 10,
		32, 44, 42, 38, 32, 25, 17,  9,
		25, 35, 33, 30, 25, 20, 14,  7,
		17, 24, 23, 20, 17, 14,  9,  5,
		 9, 12, 12, 10,  9,  7,  5,  2
	};

	// Reference C implementation (from pl_mpeg, slightly adjusted).
	int dequant_level(int idx, int level, int scale, int intra) {
		idx = PLM_VIDEO_ZIG_ZAG[idx];

		level <<= 1;
		if (!intra) {
			level += (level < 0 ? -1 : 1);
		}
		// debugf("   rnd: %04x\n", level);
		level = (level * scale * (intra ? PLM_VIDEO_INTRA_QUANT_MATRIX : PLM_VIDEO_NON_INTRA_QUANT_MATRIX)[idx]);
		// debugf("   dequant: %04x (scale:%x, quant:%x)\n", level, scale, (intra ? PLM_VIDEO_INTRA_QUANT_MATRIX : PLM_VIDEO_NON_INTRA_QUANT_MATRIX)[idx]);
		level >>= 4;
		// debugf("   scale: %04x\n", level);
		if ((level & 1) == 0) {
			level += level > 0 ? -1 : 1;
		}
		// debugf("   oddify: %04x\n", level);
		if (level > 2047) {
			level = 2047;
		}
		else if (level < -2048) {
			level = -2048;
		}
		// debugf("   clamp: %04x\n", level);
		level = (level * PLM_VIDEO_PREMULTIPLIER_MATRIX[idx]) >> RSP_IDCT_SCALER;
		// debugf("   premult: %04x (pf: %x)\n", level, PLM_VIDEO_PREMULTIPLIER_MATRIX[idx]);
		return level;
	}

	rspq_init(); DEFER(rspq_close());
	rsp_mpeg1_init(); DEFER(rsp_mpeg1_close());

	uint8_t pixels1[8*8] __attribute__((aligned(16)));
	int16_t matrix1[8*8] __attribute__((aligned(16)));
	int16_t matrix2[8*8] __attribute__((aligned(16)));

	rsp_mpeg1_set_quant_matrix(false, PLM_VIDEO_NON_INTRA_QUANT_MATRIX);
	rsp_mpeg1_set_quant_matrix(true, PLM_VIDEO_INTRA_QUANT_MATRIX);

	for (int nt=0;nt<1024;nt++) {
		SRAND(nt+1);
		int intra = RANDN(2);
		int ncoeffs = RANDN(64)+1;
		int scale = RANDN(31)+1;

		rsp_mpeg1_block_begin(RSP_MPEG1_BLOCK_CB, pixels1, 8);

		// debugf("----------------------\n");
		memset(matrix1, 0, sizeof(matrix1));
		for (int i=0;i<ncoeffs;i++) {
			int idx = RANDN(64);
			int16_t c = RANDN(2048)-1024;
			// Encoding level 0 does not make sense. C and RSP differs in this
			// edge case but it's not worth aligning them.
			if (c == 0)
				c = 1;
			rsp_mpeg1_block_coeff(idx, c);
			// debugf("coeff: %d->(%d,%d) = %04x\n", idx, PLM_VIDEO_ZIG_ZAG[idx]/8, PLM_VIDEO_ZIG_ZAG[idx]%8, (uint16_t)c);
			if (idx == 0 && intra)
				matrix1[idx] = c;
			else
				matrix1[PLM_VIDEO_ZIG_ZAG[idx]] = dequant_level(idx, c, scale, intra);
		}
		rsp_mpeg1_block_dequant(intra, scale);
		rsp_mpeg1_store_matrix(matrix2);
		rspq_wait();

		for (int j=0;j<8;j++) {	
			for (int i=0;i<8;i++) {
				ASSERT_EQUAL_HEX((uint16_t)matrix2[j*8+i], (uint16_t)matrix1[j*8+i],
					"Dequant failure at %d,%d (intra=%d, ncoeffs=%d, scale=%d, nt=%d)", j, i, intra, ncoeffs, scale, nt);
			}
		}		
	}
}

/****************************
 *
 * C REFERNCE IMPLEMENTATION OF BLOCK PREDICTION
 *
 ****************************/

#define PLM_BLOCK_SET(DEST, DEST_INDEX, DEST_WIDTH, SOURCE_INDEX, SOURCE_WIDTH, BLOCK_SIZE, OP) do { \
	int dest_scan = DEST_WIDTH - BLOCK_SIZE; \
	int source_scan = SOURCE_WIDTH - BLOCK_SIZE; \
	for (int y = 0; y < BLOCK_SIZE; y++) { \
		for (int x = 0; x < BLOCK_SIZE; x++) { \
			DEST[DEST_INDEX] = OP; \
			SOURCE_INDEX++; DEST_INDEX++; \
		} \
		SOURCE_INDEX += source_scan; \
		DEST_INDEX += dest_scan; \
	}} while(0)

static void plm_video_process_macroblock(
	uint8_t *s, int si, uint8_t *d, int di, int dw,
	int block_size, int odd_h, int odd_v, int interpolate
) {
	#define PLM_MB_CASE(INTERPOLATE, ODD_H, ODD_V, OP) \
		case ((INTERPOLATE << 2) | (ODD_H << 1) | (ODD_V)): \
			PLM_BLOCK_SET(d, di, dw, si, dw, block_size, OP); \
			break

	switch ((interpolate << 2) | (odd_h << 1) | (odd_v)) {
		PLM_MB_CASE(0, 0, 0, (s[si]));
		PLM_MB_CASE(0, 0, 1, (s[si] + s[si + dw] + 1) >> 1);
		PLM_MB_CASE(0, 1, 0, (s[si] + s[si + 1] + 1) >> 1);
		PLM_MB_CASE(0, 1, 1, (s[si] + s[si + 1] + s[si + dw] + s[si + dw + 1] + 2) >> 2);

		PLM_MB_CASE(1, 0, 0, (d[di] + (s[si]) + 1) >> 1);
		PLM_MB_CASE(1, 0, 1, (d[di] + ((s[si] + s[si + dw] + 1) >> 1) + 1) >> 1);
		PLM_MB_CASE(1, 1, 0, (d[di] + ((s[si] + s[si + 1] + 1) >> 1) + 1) >> 1);
		PLM_MB_CASE(1, 1, 1, (d[di] + ((s[si] + s[si + 1] + s[si + dw] + s[si + dw + 1] + 2) >> 2) + 1) >> 1);
	}

	#undef PLM_MB_CASE
}


void test_mpeg1_block_predict(TestContext *ctx) {
	rspq_init(); DEFER(rspq_close());
	rsp_mpeg1_init(); DEFER(rsp_mpeg1_close());

	enum { BUFFER_SIZE = 128 };

	uint8_t *src_buffer = malloc_uncached(BUFFER_SIZE*BUFFER_SIZE);
	DEFER(free_uncached(src_buffer));
	uint8_t *dst_buffer1 = malloc_uncached(BUFFER_SIZE*BUFFER_SIZE);
	DEFER(free_uncached(dst_buffer1));
	uint8_t *dst_buffer2 = malloc_uncached(BUFFER_SIZE*BUFFER_SIZE);
	DEFER(free_uncached(dst_buffer2));

	// Random pixel buffer
	for (int i=0;i<BUFFER_SIZE*BUFFER_SIZE;i++) {
		src_buffer[i] = RANDN(256);
		dst_buffer1[i] = dst_buffer2[i] = RANDN(256);
	}

	for (int nt=0;nt<4096;nt++) {
		SRAND(nt+1);
		int bs = RANDN(2) ? 16 : 8;
		int odd_h = RANDN(2), odd_v = RANDN(2), interpolate = RANDN(2);
		int sx = RANDN(BUFFER_SIZE-bs-1);
		int sy = RANDN(BUFFER_SIZE-bs-1);
		int dx = RANDN(BUFFER_SIZE-bs) & ~(bs-1);
		int dy = RANDN(BUFFER_SIZE-bs) & ~(bs-1);

		rsp_mpeg1_block_begin(bs == 16 ? RSP_MPEG1_BLOCK_Y0 : RSP_MPEG1_BLOCK_CB,
			dst_buffer2 + dy*BUFFER_SIZE+dx, BUFFER_SIZE);

		if (interpolate) {		
			int sx2 = RANDN(BUFFER_SIZE-bs-1);
			int sy2 = RANDN(BUFFER_SIZE-bs-1);
			int odd_h2 = RANDN(2); int odd_v2 = RANDN(2);
			rsp_mpeg1_block_predict(src_buffer + sy2*BUFFER_SIZE+sx2, BUFFER_SIZE, odd_h2, odd_v2, 0);

			plm_video_process_macroblock(
				src_buffer, sy2*BUFFER_SIZE+sx2,
				dst_buffer1, dy*BUFFER_SIZE+dx, BUFFER_SIZE,
				bs, odd_h2, odd_v2, 0);
		}

		rsp_mpeg1_block_predict(src_buffer + sy*BUFFER_SIZE+sx, BUFFER_SIZE, odd_h, odd_v, interpolate);
		if (bs == 16) {		
			// rsp_mpeg1_block_split();
			rsp_mpeg1_block_switch_partition(0); rsp_mpeg1_store_pixels();
			rsp_mpeg1_block_switch_partition(1); rsp_mpeg1_store_pixels();
			rsp_mpeg1_block_switch_partition(2); rsp_mpeg1_store_pixels();
			rsp_mpeg1_block_switch_partition(3); rsp_mpeg1_store_pixels();
		} else {
			rsp_mpeg1_store_pixels();
		}
		rspq_flush();

		plm_video_process_macroblock(
			src_buffer, sy*BUFFER_SIZE+sx, 
			dst_buffer1, dy*BUFFER_SIZE+dx, BUFFER_SIZE,
			bs, odd_h, odd_v, interpolate);

		rspq_wait();

		for (int j=dy-8;j<dy+bs+8;j++) {	
			for (int i=dx-8;i<dx+bs+8;i++) {
				if (j<0 || j>=BUFFER_SIZE || i<0 || i>=BUFFER_SIZE)
					continue;

				ASSERT_EQUAL_HEX(
					dst_buffer1[j*BUFFER_SIZE+i],
					dst_buffer2[j*BUFFER_SIZE+i],
					"Prediction failure at %d,%d (nt:%d bs:%d d:%d,%d odds:%d/%d/%d)",
					   i, j, nt, bs, dx, dy, odd_h, odd_v, interpolate);
			}
		}
	}
}
