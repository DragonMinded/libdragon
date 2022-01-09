#ifndef __LIBDRAGON_MPEG1_INTERNAL_H
#define __LIBDRAGON_MPEG1_INTERNAL_H

// The IDCT of pl_mpeg requires 17 or 18 bits of precision.
// Since RSP has 16-bit vector registers, we need to scale
// input data. This macro decides by how much.
// TODO: try with 1
#define RSP_IDCT_SCALER       2

// Usage of RSP in MPEG-1 player:
//   0: None (full CPU)
//   1: IDCT+Residual
#define RSP_MODE              1

#ifndef __ASSEMBLER__
#include "pl_mpeg/pl_mpeg.h"

void rsp_mpeg1_init(void);
void rsp_mpeg1_load_matrix(int16_t *mtx);
void rsp_mpeg1_store_pixels(int8_t *mtx);
void rsp_mpeg1_idct(void);
void rsp_mpeg1_set_block(uint8_t *pixels, int pitch);
void rsp_mpeg1_decode_block(int ncoeffs, bool intra);
#endif

#endif
