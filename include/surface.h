#ifndef __LIBDRAGON_SURFACE_H
#define __LIBDRAGON_SURFACE_H

#include <stdint.h>

typedef enum {
    FMT_TYPE_RGBA = 0,
    FMT_TYPE_YUV  = 1,
    FMT_TYPE_CI   = 2,
    FMT_TYPE_IA   = 3,
    FMT_TYPE_I    = 4,
} tex_format_type_t;

typedef enum {
    FMT_SIZE_4    = 0,
    FMT_SIZE_8    = 1,
    FMT_SIZE_16   = 2,
    FMT_SIZE_32   = 3,
} tex_format_size_t;

typedef enum {
    FMT_NONE   = 0,

    FMT_RGBA16 = (FMT_TYPE_RGBA << 2) | FMT_SIZE_16,
    FMT_RGBA32 = (FMT_TYPE_RGBA << 2) | FMT_SIZE_32,
    FMT_YUV16  = (FMT_TYPE_YUV  << 2) | FMT_SIZE_16,
    FMT_CI4    = (FMT_TYPE_CI   << 2) | FMT_SIZE_4,
    FMT_CI8    = (FMT_TYPE_CI   << 2) | FMT_SIZE_8,
    FMT_IA4    = (FMT_TYPE_IA   << 2) | FMT_SIZE_4,
    FMT_IA8    = (FMT_TYPE_IA   << 2) | FMT_SIZE_8,
    FMT_IA16   = (FMT_TYPE_IA   << 2) | FMT_SIZE_16,
    FMT_I4     = (FMT_TYPE_I    << 2) | FMT_SIZE_4,
    FMT_I8     = (FMT_TYPE_I    << 2) | FMT_SIZE_8,
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
    return surface->flags & 0x1F;
}

inline tex_format_type_t tex_format_get_type(tex_format_t format)
{
    return format >> 2;
}

inline tex_format_size_t tex_format_get_size(tex_format_t format)
{
    return format & 0x3;
}

inline uint32_t tex_format_get_bitdepth(tex_format_size_t size)
{
    return 4 << size;
}

inline uint32_t tex_format_get_bytes_per_pixel(tex_format_size_t size)
{
    return tex_format_get_bitdepth(size) >> 3;
}

#ifdef __cplusplus
}
#endif

#endif
