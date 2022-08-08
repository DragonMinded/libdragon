#include "surface.h"
#include "n64sys.h"
#include "rdp_commands.h"
#include "debug.h"
#include <assert.h>
#include <string.h>

const char* tex_format_name(tex_format_t fmt)
{
    switch (fmt) {
    case FMT_NONE:   return "FMT_NONE";
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

void surface_free(surface_t *surface)
{
    if (surface->buffer && surface->flags & SURFACE_FLAGS_OWNEDBUFFER) {
        free_uncached(surface->buffer);
        surface->buffer = NULL;
    }
    memset(surface, 0, sizeof(surface_t));
}

void surface_new_sub(surface_t *sub, surface_t *parent, uint32_t x0, uint32_t y0, uint32_t width, uint32_t height)
{
    assert(x0 + width <= parent->width);
    assert(y0 + height <= parent->height);

    tex_format_t fmt = surface_get_format(parent);

    sub->buffer = parent->buffer + y0 * parent->stride + TEX_FORMAT_PIX2BYTES(fmt, x0);
    sub->width = width;
    sub->height = height;
    sub->stride = parent->stride;
    sub->flags = parent->flags & ~SURFACE_FLAGS_OWNEDBUFFER;
}

extern inline surface_t surface_make(void *buffer, tex_format_t format, uint32_t width, uint32_t height, uint32_t stride);
extern inline surface_t surface_alloc(tex_format_t format, uint32_t width, uint32_t height);
extern inline tex_format_t surface_get_format(const surface_t *surface);
