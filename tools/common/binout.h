#ifndef COMMON_BINOUT_H
#define COMMON_BINOUT_H

/**
 * @file binout.h
 * @brief Helper to write binary big-endian data to a file
 */

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>

#define BITCAST_F2I(f)   ({ uint32_t __i; memcpy(&__i, &(f), 4); __i; })

void placeholder_register(FILE *file, const char *name);
void placeholder_registervf(FILE *file, const char *format, va_list arg);
void placeholder_registerf(FILE *file, const char *format, ...);
void placeholder_add(FILE *file, const char *name);
void placeholder_addvf(FILE *file, const char *format, va_list arg);
void placeholder_addf(FILE *file, const char *format, ...);
void placeholder_clear();

void w8(FILE *f, uint8_t v);
void w16(FILE *f, uint16_t v);
void w32(FILE *f, uint32_t v);
#define wf32(f, v) w32(f, BITCAST_F2I(v))

int w32_placeholder(FILE *f);
void w32_at(FILE *f, int pos, uint32_t v);
void walign(FILE *f, int align);
void wpad(FILE *f, int size);

#endif
