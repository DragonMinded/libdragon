#include "../src/video/mpeg1_internal.h"

void test_mpeg1_idct(TestContext *ctx) {
	rspq_init(); DEFER(rspq_close());
	rsp_mpeg1_init();

	int16_t matrix1[8*8] __attribute__((aligned(8)));
	int8_t out1[8*8] __attribute__((aligned(8)));
	int matrix2[8*8] __attribute__((aligned(8)));

	for (int n=0;n<256;n++) {	
		for (int j=0;j<8;j++) {	
			for (int i=0;i<8;i++) {
				matrix1[j*8+i] = matrix2[j*8+i] = RANDN(256)-128;
			}
		}

		rsp_mpeg1_load_matrix(matrix1);
		rsp_mpeg1_idct();
		rsp_mpeg1_store_pixels(out1);
		rspq_sync();

		// Reference implementation
		extern void plm_video_idct(int *block);
		plm_video_idct(matrix2);

		for (int j=0;j<8;j++) {	
			for (int i=0;i<8;i++) {
				ASSERT_EQUAL_SIGNED(out1[j*8+i], matrix2[j*8+i],
					"IDCT failure at %d,%d", j, i);
			}
		}
	}
}
