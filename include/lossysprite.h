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
 * The H.264 intra slice is fully decoded during this call into a 4:2:0
 * YUV sprite stored as a Y plane (#FMT_I8) followed by an interleaved
 * UV plane (#FMT_IA16). The sprite is tagged as #FMT_YUV16 with
 * #SPRITE_FLAG_YUV_SEMIPLANAR set internally. It renders correctly via
 * #rdpq_sprite_blit, which transparently routes to
 * #yuv_tex_blit_semiplanar and applies the colorspace stored in the file
 * header.
 *
 * Note: a semi-planar YUV sprite cannot be uploaded to TMEM as a
 * single texture, so it is incompatible with #rdpq_sprite_upload and
 * any code path that expects packed UYVY pixels. Use
 * #rdpq_sprite_blit to draw it.
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

