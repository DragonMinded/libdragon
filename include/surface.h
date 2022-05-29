#ifndef __LIBDRAGON_SURFACE_H
#define __LIBDRAGON_SURFACE_H

#include <stdint.h>

#define TEX_FORMAT_CODE(fmt, size)        (((fmt)<<2)|(size))
#define TEX_FORMAT_BITDEPTH(fmt)          (4 << ((fmt) & 0x3))
#define TEX_FORMAT_BYTES_PER_PIXEL(fmt)   (TEX_FORMAT_BITDEPTH(fmt) >> 3)

typedef enum {
    FMT_NONE   = 0,

    FMT_RGBA16 = TEX_FORMAT_CODE(0, 2),
    FMT_RGBA32 = TEX_FORMAT_CODE(0, 3),
    FMT_YUV16  = TEX_FORMAT_CODE(1, 2),
    FMT_CI4    = TEX_FORMAT_CODE(2, 0),
    FMT_CI8    = TEX_FORMAT_CODE(2, 1),
    FMT_IA4    = TEX_FORMAT_CODE(3, 0),
    FMT_IA8    = TEX_FORMAT_CODE(3, 1),
    FMT_IA16   = TEX_FORMAT_CODE(3, 2),
    FMT_I4     = TEX_FORMAT_CODE(4, 0),
    FMT_I8     = TEX_FORMAT_CODE(4, 1),
} tex_format_t;

typedef struct surface_s
{
    uint32_t flags;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    void *buffer;
} surface_t;

#ifdef __cplusplus
extern "C" {
#endif

void surface_init(surface_t *surface, void *buffer, tex_format_t format, uint32_t width, uint32_t height, uint32_t stride);

inline tex_format_t surface_get_format(const surface_t *surface)
{
    return (tex_format_t)(surface->flags & 0x1F);
}

#ifdef __cplusplus
}
#endif

#endif
