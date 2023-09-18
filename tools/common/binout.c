#include <stdio.h>
#include <stdint.h>
#include <assert.h>

void w8(FILE *f, uint8_t v) 
{
    fputc(v, f);
}

void w16(FILE *f, uint16_t v)
{
    w8(f, v >> 8);
    w8(f, v & 0xff);
}

void w32(FILE *f, uint32_t v)
{
    w16(f, v >> 16);
    w16(f, v & 0xffff);
}

int w32_placeholder(FILE *f)
{
    int pos = ftell(f);
    w32(f, 0);
    return pos;
}

void w32_at(FILE *f, int pos, uint32_t v)
{
    int cur = ftell(f);
    assert(cur >= 0);  // fail on pipes
    fseek(f, pos, SEEK_SET);
    w32(f, v);
    fseek(f, cur, SEEK_SET);
}

void walign(FILE *f, int align)
{ 
    int pos = ftell(f);
    assert(pos >= 0);  // fail on pipes
    while (pos++ % align) w8(f, 0);
}

void wpad(FILE *f, int size)
{
    while (size--) {
        w8(f, 0);
    }
}
