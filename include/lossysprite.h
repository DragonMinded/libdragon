/**
 * @file lossysprite.h
 * @brief LSPR (lossy sprite) decoder
 */
#ifndef __LIBDRAGON_LOSSYSPRITE_H
#define __LIBDRAGON_LOSSYSPRITE_H

#include <stdbool.h>
#include "sprite.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Open and decode a LSPR file into memory.
 *
 * The H.264 intra slice is fully decoded during this call into a planar
 * 4:2:0 YUV sprite (#FMT_YUV16, with #SPRITE_FLAG_YUV_PLANAR set
 * internally). The returned sprite renders correctly via
 * #rdpq_sprite_blit, which transparently routes to #yuv_tex_blit and
 * applies the colorspace stored in the file header.
 *
 * Note: a planar YUV sprite cannot be uploaded to TMEM as a single
 * texture, so it is incompatible with #rdpq_sprite_upload and any code
 * path that expects packed UYVY pixels. Use #rdpq_sprite_blit to draw
 * it.
 *
 * #sprite_load also accepts LSPR files directly (it sniffs the magic
 * and delegates here), so calling it instead of this function lets
 * the same code path handle both regular and lossy sprites.
 *
 * @param fn    Path to the LSPR file
 * @return      The loaded sprite (free with #sprite_free)
 */
sprite_t* lossysprite_load(const char *fn);

#ifdef __cplusplus
}
#endif

#endif

