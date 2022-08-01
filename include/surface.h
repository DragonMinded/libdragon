#ifndef __LIBDRAGON_SURFACE_H
#define __LIBDRAGON_SURFACE_H

#include <stdint.h>

#define TEX_FORMAT_CODE(fmt, size)        (((fmt)<<2)|(size))
#define TEX_FORMAT_BITDEPTH(fmt)          (4 << ((fmt) & 0x3))
#define TEX_FORMAT_BYTES_PER_PIXEL(fmt)   (TEX_FORMAT_BITDEPTH(fmt) >> 3)
#define TEX_FORMAT_GET_STRIDE(fmt, width) ((TEX_FORMAT_BITDEPTH(fmt) * width) >> 3)

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

#define SURFACE_FLAGS_TEXFORMAT    0x1F   ///< Pixel format of the surface
#define SURFACE_FLAGS_OWNEDBUFFER  0x20   ///< Set if the buffer must be freed

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

/**
 * @brief Initialize a surface_t structure, optionally allocating memory
 *
 * @param      surface  Surface to initialize
 * @param[in]  buffer   Buffer to use, or NULL to auto-allocate it
 * @param[in]  format   Pixel format of the surface
 * @param[in]  width    Width in pixels
 * @param[in]  height   Height in pixels
 * @param[in]  stride   Stride in bytes (distance between rows)
 */
void surface_new(surface_t *surface, 
    void *buffer, tex_format_t format,
    uint32_t width, uint32_t height, uint32_t stride);

/**
 * @brief Initialize a surface_t structure, pointing to a rectangular portion of another
 *        surface.
 */
void surface_new_sub(surface_t *sub, 
    surface_t *parent, uint32_t x0, uint32_t y0, uint32_t width, uint32_t height);

void surface_free(surface_t *surface);

inline tex_format_t surface_get_format(const surface_t *surface)
{
    return (tex_format_t)(surface->flags & 0x1F);
}

#ifdef __cplusplus
}
#endif

#endif
