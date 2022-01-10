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
						if (ncoeffs==1)
							// DC coefficient: already a delta
							// for pixels
							matrix2[j*8+i] = matrix1[j*8+i] = RANDN(65536)-32768;
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

	rspq_init(); DEFER(rspq_close());
	rsp_mpeg1_init();

	uint8_t pixels1[8*8] __attribute__((aligned(16)));
	int16_t matrix1[8*8] __attribute__((aligned(16)));

	rsp_mpeg1_set_quant_matrix(true, PLM_VIDEO_NON_INTRA_QUANT_MATRIX);

	rsp_mpeg1_block_begin(pixels1, 8);
	rsp_mpeg1_block_coeff(0, 45);
	rsp_mpeg1_block_coeff(1, -45);
	rsp_mpeg1_block_coeff(8, 1024);
	rsp_mpeg1_block_coeff(9, -1024);
	rsp_mpeg1_block_dequant(true, 3);
	rsp_mpeg1_store_matrix(matrix1);
	rspq_sync();

	debugf("%d %d %d %d\n", 
		matrix1[PLM_VIDEO_ZIG_ZAG[0]], 
		matrix1[PLM_VIDEO_ZIG_ZAG[1]], 
		matrix1[PLM_VIDEO_ZIG_ZAG[8]], 
		matrix1[PLM_VIDEO_ZIG_ZAG[9]]);
}
