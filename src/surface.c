#include "surface.h"
#include "rdp_commands.h"
#include "debug.h"

void surface_init(surface_t *surface, void *buffer, tex_format_t format, uint32_t width, uint32_t height, uint32_t stride)
{
    surface->buffer = buffer;
    surface->flags = format;
    surface->width = width;
    surface->height = height;
    surface->stride = stride;
}
