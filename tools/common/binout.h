#ifndef COMMON_BINOUT_H
#define COMMON_BINOUT_H

/**
 * @file binout.h
 * @brief Helper to write binary big-endian data to a file
 */

#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#define BITCAST_F2I(f)   ({ uint32_t __i; memcpy(&__i, &(f), 4); __i; })

void w8(FILE *f, uint8_t v);
void w16(FILE *f, uint16_t v);
void w32(FILE *f, uint32_t v);
#define wf32(f, v) w32(f, BITCAST_F2I(v))

int w32_placeholder(FILE *f);
void w32_at(FILE *f, int pos, uint32_t v);
void walign(FILE *f, int align);
void wpad(FILE *f, int size);

#endif
