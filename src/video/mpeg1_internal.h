#ifndef __LIBDRAGON_MPEG1_INTERNAL_H
#define __LIBDRAGON_MPEG1_INTERNAL_H

#include "pl_mpeg/pl_mpeg.h"

void rsp_mpeg1_init(void);
void rsp_mpeg1_load_matrix(int16_t *mtx);
void rsp_mpeg1_store_pixels(int8_t *mtx);
void rsp_mpeg1_idct(void);
void rsp_mpeg1_set_block(uint8_t *pixels, int pitch);
void rsp_mpeg1_decode_block(int ncoeffs, bool intra);

#endif
