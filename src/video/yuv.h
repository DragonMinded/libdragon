#ifndef __LIBDRAGON_YUV_H
#define __LIBDRAGON_YUV_H

#include <stdint.h>

void yuv_init(void);
void yuv_set_input_buffer(uint8_t *y, uint8_t *cb, uint8_t *cr, int y_pitch);
void yuv_set_output_buffer(uint8_t *out, int out_pitch);
void yuv_interleave_block_32x16(int x0, int y0);

#endif
