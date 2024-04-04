/**
 * @file surface.c
 * @brief Surface buffers used to draw images
 * @ingroup graphics
 */

#include "surface.h"
#include "n64sys.h"
#include "debug.h"
#include <assert.h>
#include <string.h>
#include <inttypes.h>

const char* tex_format_name(tex_format_t fmt)
{
    switch (fmt) {
    case FMT_NONE:   return "FMT_NONE";
    case FMT_RGBA32: return "FMT_RGBA32";
    case FMT_RGBA16: return "FMT_RGBA16";
    case FMT_YUV16:  return "FMT_YUV16";
    case FMT_CI4:    return "FMT_CI4";
    case FMT_CI8:    return "FMT_CI8";
    case FMT_IA4:    return "FMT_IA4";
    case FMT_IA8:    return "FMT_IA8";
    case FMT_IA16:   return "FMT_IA16";
    case FMT_I4:     return "FMT_I4";
    case FMT_I8:     return "FMT_I8";
    default:         return "FMT_???";
    }
}

surface_t surface_alloc(tex_format_t format, uint16_t width, uint16_t height)
{
    // A common mistake is to call surface_alloc with the wrong argument order.
    // Try to catch it by checking that the format is not valid.
    // Do not limit ourselves to tex_format_t enum values, as people might want
    // to test weird RDP formats (e.g. RGBA8) to find out what happens.
    assertf((format & ~SURFACE_FLAGS_TEXFORMAT) == 0,
        "invalid surface format: 0x%x (%d)", format, format);
    return (surface_t){ 
        .flags = format | SURFACE_FLAGS_OWNEDBUFFER,
        .width = width,
        .height = height,
        .stride = TEX_FORMAT_PIX2BYTES(format, width),
        .buffer = malloc_uncached_aligned(64, height * TEX_FORMAT_PIX2BYTES(format, width)),
    };
}

void surface_free(surface_t *surface)
{
    if (surface_has_owned_buffer(surface)) {
        free_uncached(surface->buffer);
    }
    memset(surface, 0, sizeof(surface_t));
}

surface_t surface_make_sub(surface_t *parent, uint16_t x0, uint16_t y0, uint16_t width, uint16_t height)
{
    assert(x0 + width <= parent->width);
    assert(y0 + height <= parent->height);

    tex_format_t fmt = surface_get_format(parent);
    assertf(TEX_FORMAT_BITDEPTH(fmt) != 4 || (x0 & 1) == 0,
        "cannot create a subsurface with an odd X offset (%" PRId16 ") in a 4bpp surface", x0);

    surface_t sub;
    sub.buffer = parent->buffer + y0 * parent->stride + TEX_FORMAT_PIX2BYTES(fmt, x0);
    sub.width = width;
    sub.height = height;
    sub.stride = parent->stride;
    sub.flags = parent->flags & ~SURFACE_FLAGS_OWNEDBUFFER;
    return sub;
}

extern inline surface_t surface_make(void *buffer, tex_format_t format, uint16_t width, uint16_t height, uint16_t stride);
extern inline tex_format_t surface_get_format(const surface_t *surface);
extern inline surface_t surface_make_linear(void *buffer, tex_format_t format, uint16_t width, uint16_t height);
extern inline bool surface_has_owned_buffer(const surface_t *surface);
