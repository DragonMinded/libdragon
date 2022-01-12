#include "../src/video/mpeg1_internal.h"

void test_mpeg1_idct(TestContext *ctx) {
	rspq_init(); DEFER(rspq_close());
	rsp_mpeg1_init();

	int16_t matrix1[8*8] __attribute__((aligned(16)));
	int8_t out1[8*8] __attribute__((aligned(16)));
	int16_t matrix2[8*8] __attribute__((aligned(16)));

	for (int nt=0;nt<256;nt++) {	
		for (int j=0;j<8;j++) {	
			for (int i=0;i<8;i++) {
				matrix1[j*8+i] = RANDN(256)-128;
				matrix2[j*8+i] = matrix1[j*8+i];
			}
		}

		rsp_mpeg1_load_matrix(matrix1);
		rsp_mpeg1_idct();
		rsp_mpeg1_store_pixels(out1);
		rspq_sync();

		// Reference implementation
		extern void plm_video_idct(int16_t *block);
		plm_video_idct(matrix2);

		for (int j=0;j<8;j++) {	
			for (int i=0;i<8;i++) {
				ASSERT_EQUAL_SIGNED(out1[j*8+i], matrix2[j*8+i],
					"IDCT failure at %d,%d", j, i);
			}
		}
	}
}

void test_mpeg1_block_decode(TestContext *ctx) {
	rspq_init(); DEFER(rspq_close());
	rsp_mpeg1_init();

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

				rsp_mpeg1_block_begin(pixels1, 8);
				rsp_mpeg1_load_matrix(matrix1);
				rsp_mpeg1_block_decode(ncoeffs, intra!=0);

				extern void plm_video_decode_block_residual(int16_t *s, int si, uint8_t *d, int di, int dw, int n, int intra);
				plm_video_decode_block_residual(matrix2, 0, pixels2, 0, 8, ncoeffs, intra);
				rspq_sync();

				for (int j=0;j<8;j++) {	
					for (int i=0;i<8;i++) {
						ASSERT_EQUAL_HEX(pixels1[j*8+i], pixels2[j*8+i],
							"IDCT failure at %d,%d (intra=%d, ncoeffs=%d, nt=%d)", j, i, intra, ncoeffs, nt);
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
	rsp_mpeg1_init();

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

		rsp_mpeg1_block_begin(pixels1, 8);

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
			if (idx == 0)
				matrix1[idx] = c;
			else
				matrix1[PLM_VIDEO_ZIG_ZAG[idx]] = dequant_level(idx, c, scale, intra);
		}
		rsp_mpeg1_block_dequant(intra, scale);
		rsp_mpeg1_store_matrix(matrix2);
		rspq_sync();

		for (int j=0;j<8;j++) {	
			for (int i=0;i<8;i++) {
				ASSERT_EQUAL_HEX((uint16_t)matrix2[j*8+i], (uint16_t)matrix1[j*8+i],
					"Dequant failure at %d,%d (intra=%d, ncoeffs=%d, scale=%d, nt=%d)", j, i, intra, ncoeffs, scale, nt);
			}
		}		
	}
}
