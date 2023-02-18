#ifndef LZH5_COMPRESS_H
#define LZH5_COMPRESS_H

#include <stdio.h>

#define LZHUFF5_METHOD_NUM      5
#define LZHUFF6_METHOD_NUM      6
#define LZHUFF7_METHOD_NUM      7

void lzh5_init(int method);

void lzh5_encode(FILE *in, FILE *out,
	unsigned int *out_crc, unsigned int *out_csize, unsigned int *out_dsize);

#endif 
