/**
 * @file lossysprite.h
 * @brief LSPR (lossy sprite) decoder
 *
 * LSPR is libdragon's lossy sprite container format: lossy compression
 * is useful for large images that need higher storage compression at the
 * cost of some quality loss, such as large backgrounds. The quality factor
 * can be tuned at build-time to achieve the desired size/quality tradeoff.
 * LSPR files are produced by `mksprite --lossy=<quality>` and always decode
 * to a #FMT_RGBA16 sprite.
 *
 * You must opt-in to support for lossy sprites by calling #lossysprite_init
 * to register the LSPR decoder with #sprite_load before loading LSPR files.
 * Once initialized, #sprite_load will recognize LSPR-compressed sprites and
 * decode them transparently.
 *
 * LSPR is implemented as an H.264 intra-only bitstream (4:2:0 YUV, BT.709
 * full range) plus a small header. At load-time the H.264 slice is decoded
 * and the YUV reconstruction is converted into a #FMT_RGBA16 sprite, so the
 * returned #sprite_t works like a normal sprite.
 */
#ifndef __LIBDRAGON_LOSSYSPRITE_H
#define __LIBDRAGON_LOSSYSPRITE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register the LSPR decoder with the sprite loader.
 *
 * After registering, #sprite_load recognizes the `LSPR` magic and decodes
 * matching files transparently. Until then, loading an LSPR file via
 * #sprite_load fails with an assertion.
 * 
 * Refcounted; calling this increments the refcount and will require the
 * same number of #lossysprite_close calls to fully unregister.
 */
void lossysprite_init(void);

/**
 * @brief Unregister the LSPR decoder.
 *
 * Refcounted; calling this decrements the refcount and once it reaches zero,
 * #sprite_load no longer recognizes LSPR files and attempts to load them will
 * fail with an assertion.
 */
void lossysprite_close(void);

#ifdef __cplusplus
}
#endif

#endif
