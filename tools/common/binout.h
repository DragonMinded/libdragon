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
#include <math.h>

#define BITCAST_F2I(f)   ({ uint32_t __i; float __f = (f); memcpy(&__i, &(__f), 4); __i; })

#define _wconv(type, v) ({ \
    typeof(v) _v = (v); \
    if (sizeof(type) < sizeof(_v)) { \
        int64_t ext = (int64_t)_v >> (sizeof(type) * 8); \
        if (ext != 0 && ext != (uint64_t)-1) { \
            fprintf(stderr, "fatal: truncating value %lld to %s (ext=%lld)\n", (long long)_v, #type, (long long)ext); \
            assert(ext == 0 || ext == (uint64_t)-1); \
        } \
    } \
    (type)_v; \
})

#ifdef __cplusplus
extern "C" {
#endif

void placeholder_setv(FILE *file, const char *format, va_list arg);
void placeholder_set(FILE *file, const char *format, ...);
void placeholder_setv_offset(FILE *file, int offset, const char *format, va_list arg);
void placeholder_set_offset(FILE *file, int offset, const char *format, ...);
void placeholder_clear();

void _w8(FILE *f, uint8_t v);
void _w16(FILE *f, uint16_t v);
void _w32(FILE *f, uint32_t v);
void _w64(FILE *f, uint64_t v);
#define w8(f, v) _w8(f, _wconv(uint8_t, v))
#define w16(f, v) _w16(f, _wconv(uint16_t, v))
#define w32(f, v) _w32(f, _wconv(uint32_t, v))
#define w64(f, v) _w64(f, _wconv(uint64_t, v))
#define wf32(f, v) _w32(f, BITCAST_F2I(v))
#define wf32approx(f, v, prec) wf32(f, roundf((v)/(prec))*(prec))

int w32_placeholder(FILE *f);
void w32_placeholdervf(FILE *file, const char *format, va_list arg);
void w32_placeholderf(FILE *file, const char *format, ...);
void w32_at(FILE *f, int pos, uint32_t v);
void walign(FILE *f, int align);
void wpad(FILE *f, int size);

#ifdef __cplusplus
}
#endif

#endif
