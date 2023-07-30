#ifndef COMMON_BINOUT_H
#define COMMON_BINOUT_H

/**
 * @file binout.h
 * @brief Helper to write binary big-endian data to a file
 */

#include <stdio.h>
#include <assert.h>

#define BITCAST_F2I(f)   ({ uint32_t __i; memcpy(&__i, &(f), 4); __i; })

#define conv(type, v) ({ \
    typeof(v) _v = (v); \
    if (sizeof(type) < sizeof(_v)) { \
        int ext = (int)_v >> (sizeof(type) * 8 - 1); \
        assert(ext == 0 || ext == (unsigned)-1); \
    } \
    (type)_v; \
})

void _w8(FILE *f, uint8_t v)  { fputc(v, f); }
void _w16(FILE *f, uint16_t v) { _w8(f, v >> 8); _w8(f, v & 0xff); }
void _w32(FILE *f, uint32_t v) { _w16(f, v >> 16); _w16(f, v & 0xffff); }
#define w8(f, v) _w8(f, conv(uint8_t, v))
#define w16(f, v) _w16(f, conv(uint16_t, v))
#define w32(f, v) _w32(f, conv(uint32_t, v))
#define wf32(f, v) _w32(f, BITCAST_F2I(v))

int w32_placeholder(FILE *f) { int pos = ftell(f); w32(f, 0); return pos; }
void w32_at(FILE *f, int pos, uint32_t v)
{
    int cur = ftell(f);
    assert(cur >= 0);  // fail on pipes
    fseek(f, pos, SEEK_SET);
    w32(f, v);
    fseek(f, cur, SEEK_SET);
}
void walign(FILE *f, int align) { 
    int pos = ftell(f);
    assert(pos >= 0);  // fail on pipes
    while (pos++ % align) w8(f, 0);
}

void wpad(FILE *f, int size) { while (size--) w8(f, 0); }

#endif
