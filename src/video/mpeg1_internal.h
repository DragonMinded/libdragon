#ifndef __LIBDRAGON_MPEG1_INTERNAL_H
#define __LIBDRAGON_MPEG1_INTERNAL_H

// The IDCT of pl_mpeg requires about 19 bits of precision
// Since RSP has 16-bit vector registers, we need to scale
// input data. This macro decides by how much.
// 3 is the minimum value that does not seem to produce
// artifacts in videos.
#define RSP_IDCT_SCALER       3

// Usage of RSP in MPEG-1 player:
//   0: None (full CPU)
//   1: IDCT+Residual
//   2: Dequant+IDCT+Residual
//   3: Dequant+IDCT+Residual+Prediction
#define RSP_MODE              3

#define ASSERT_UNDEFINED_BLOCK   0x0001
#define ASSERT_UNDEFINED_BLOCK2  0x0002
#define ASSERT_UNDEFINED_BLOCK3  0x0003
#define ASSERT_UNDEFINED_BLOCK4  0x0004
#define ASSERT_UNDEFINED_BLOCK5  0x0005
#define ASSERT_UNDEFINED_BLOCK6  0x0006
#define ASSERT_PIXELCHECK(n)     (0x0010+n)

#define RSP_MPEG1_BLOCK_Y0   0
#define RSP_MPEG1_BLOCK_Y1   1
#define RSP_MPEG1_BLOCK_Y2   2
#define RSP_MPEG1_BLOCK_Y3   3
#define RSP_MPEG1_BLOCK_CB   4
#define RSP_MPEG1_BLOCK_CR   5

#ifndef __ASSEMBLER__
#include "pl_mpeg/pl_mpeg.h"

void rsp_mpeg1_init(void);
void rsp_mpeg1_close(void);
void rsp_mpeg1_load_matrix(int16_t *mtx);
void rsp_mpeg1_store_matrix(int16_t *mtx);
void rsp_mpeg1_zero_pixels(void);
void rsp_mpeg1_load_pixels(void);
void rsp_mpeg1_store_pixels(void);
void rsp_mpeg1_idct(void);
void rsp_mpeg1_block_begin(int block, uint8_t *pixels, int pitch);
void rsp_mpeg1_block_switch_partition(int partition);
void rsp_mpeg1_block_coeff(int idx, int16_t coeff);
void rsp_mpeg1_block_dequant(bool intra, int scale);
void rsp_mpeg1_block_decode(int ncoeffs, bool intra);
void rsp_mpeg1_set_quant_matrix(bool intra, const uint8_t quant_mtx[64]);
void rsp_mpeg1_block_predict(uint8_t *src, int pitch, bool oddh, bool oddv, bool interpolate);
void rsp_mpeg1_block_split(void);

#endif

#endif
