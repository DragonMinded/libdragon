#include "surface.h"
#include "n64sys.h"
#include "rdp_commands.h"
#include "debug.h"
#include <assert.h>

void surface_new(surface_t *surface, void *buffer, tex_format_t format, uint32_t width, uint32_t height, uint32_t stride)
{
    uint32_t flags = format;
    if (!buffer) {
        buffer = malloc_uncached_aligned(64, height * stride);
        flags |= SURFACE_FLAGS_OWNEDBUFFER;
    }
    else
    {
        assertf(((uint32_t)buffer & 63) == 0, "buffer must be aligned to 64 byte");
        buffer = UncachedAddr(buffer);
    }

    surface->buffer = buffer;
    surface->flags = flags;
    surface->width = width;
    surface->height = height;
    surface->stride = stride;
}

void surface_free(surface_t *surface)
{
    if (surface->buffer && surface->flags & SURFACE_FLAGS_OWNEDBUFFER) {
        free_uncached(surface->buffer);
        surface->buffer = NULL;
    }
}

void surface_new_sub(surface_t *sub, surface_t *parent, uint32_t x0, uint32_t y0, uint32_t width, uint32_t height)
{
    assert(x0 + width <= parent->width);
    assert(y0 + height <= parent->height);

    tex_format_t fmt = surface_get_format(parent);

    sub->buffer = parent->buffer + y0 * parent->stride + x0 * TEX_FORMAT_BYTES_PER_PIXEL(fmt);
    sub->width = width;
    sub->height = height;
    sub->stride = parent->stride;
    sub->flags = parent->flags & ~SURFACE_FLAGS_OWNEDBUFFER;
}
