/**
 * @file utils.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Misc utilities functions and macros
 */

#include "utils.h"
#include "debug.h"

uint32_t __utf8_decode(const char **str)
{
    const uint8_t *s = (const uint8_t*)*str;
    uint32_t c = *s++;
    if (c < 0x80) {
        *str = (const char*)s;
        return c;
    }
    if (c < 0xC0) {
        *str = (const char*)s;
        return 0xFFFD;
    }
    if (c < 0xE0) {
        c = ((c & 0x1F) << 6) | (*s++ & 0x3F);
        *str = (const char*)s;
        return c;
    }
    if (c < 0xF0) {
        c = ((c & 0x0F) << 12); c |= ((*s++ & 0x3F) << 6); c |= (*s++ & 0x3F);
        *str = (const char*)s;
        return c;
    }
    if (c < 0xF8) {
        c = ((c & 0x07) << 18); c |= ((*s++ & 0x3F) << 12); c |= ((*s++ & 0x3F) << 6); c |= (*s++ & 0x3F);
        *str = (const char*)s;
        return c;
    }
    *str = (const char*)s;
    return 0xFFFD;
}

uint64_t __read_varint_u64(const uint8_t **ptr)
{
    uint64_t val = 0;
    int shift = 0;
    while (1) {
        uint8_t byte = *(*ptr)++;
        val |= (uint64_t)(byte & 0x7F) << shift;
        if (!(byte & 0x80)) break;
        shift += 7;
        assertf(shift < 64, "Varint overflow");
    }
    return val;
}

int64_t __read_varint_s64(const uint8_t **ptr)
{
    uint64_t val = __read_varint_u64(ptr);
    return (val >> 1) ^ -(val & 1);
}

uint64_t __peek_varint_u64(const uint8_t *ptr)
{
    return __read_varint_u64(&ptr);
}

int64_t __peek_varint_s64(const uint8_t *ptr)
{
    return __read_varint_s64(&ptr);
}
