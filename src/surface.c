#include "surface.h"
#include "rdp_commands.h"
#include "debug.h"

void surface_init(surface_t *surface, void *buffer, format_t format, uint32_t width, uint32_t height, uint32_t stride)
{
    surface->buffer = buffer;
    surface->format = format;
    surface->width = width;
    surface->height = height;
    surface->stride = stride;
}

uint32_t surface_format_to_bitdepth(format_t format)
{
    static uint32_t bitdepths[] = {
        1,
        2,
        4,
    };

    assertf(format < sizeof(bitdepths)/sizeof(uint32_t), "Invalid surface format: %d", format);

    return bitdepths[format];
}
