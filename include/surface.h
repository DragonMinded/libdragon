#ifndef __LIBDRAGON_SURFACE_H
#define __LIBDRAGON_SURFACE_H

#include <stdint.h>

typedef enum
{
    FMT_CI8,
    FMT_RGBA16,
    FMT_RGBA32,
} format_t;

typedef struct surface_s
{
    format_t format;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    void *buffer;
} surface_t;

#ifdef __cplusplus
extern "C" {
#endif

void surface_init(surface_t *surface, void *buffer, format_t format, uint32_t width, uint32_t height, uint32_t stride);

uint32_t surface_format_to_bitdepth(format_t format);

inline uint32_t surface_get_bitdepth(const surface_t *surface)
{
    return surface_format_to_bitdepth(surface->format);
}

#ifdef __cplusplus
}
#endif

#endif
