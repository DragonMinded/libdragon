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

void test_mpeg1_decode_block(TestContext *ctx) {
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

				rsp_mpeg1_load_matrix(matrix1);
				rsp_mpeg1_set_block(pixels1, 8);
				rsp_mpeg1_decode_block(ncoeffs, intra!=0);

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
