/**
 * @file lspr3.h
 * @brief Lossy-sprite Level 3: H264I decoder
 *
 * H264I ("H.264 Image") is libdragon's "Lossy-sprite Level 3" container format:
 * lossy compression is useful for large images that need higher storage
 * compression at the cost of some quality loss, such as large backgrounds. The
 * quality factor can be tuned at build-time to achieve the desired size/quality
 * tradeoff. H264I files are produced by `mksprite --lossy=<quality> --compress 3`
 * and always decode to a #FMT_RGBA16 sprite.
 *
 * You must opt-in to support for Level 3 lossy sprites by calling #lspr3_init
 * to register the H264I decoder with #sprite_load before loading H264I files.
 * Once initialized, #sprite_load will recognize H264I-compressed sprites and
 * decode them transparently.
 *
 * H264I is implemented as an H.264 intra-only bitstream (4:2:0 YUV, BT.709
 * full range) plus a small header. At load-time the H.264 slice is decoded
 * and the YUV reconstruction is converted into a #FMT_RGBA16 sprite, so the
 * returned #sprite_t works like a normal sprite.
 */
#ifndef __LIBDRAGON_LSPR3_H
#define __LIBDRAGON_LSPR3_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register the H264I (Lossy-sprite Level 3) decoder with the sprite loader.
 *
 * After registering, #sprite_load recognizes the `H264` magic and decodes
 * matching files transparently. Until then, loading an H264I file via
 * #sprite_load fails with an assertion.
 * 
 * Refcounted; calling this increments the refcount and will require the
 * same number of #lspr3_close calls to fully unregister.
 */
void lspr3_init(void);

/**
 * @brief Unregister the H264I (Lossy-sprite Level 3) decoder.
 *
 * Refcounted; calling this decrements the refcount and once it reaches zero,
 * #sprite_load no longer recognizes H264I files and attempts to load them will
 * fail with an assertion.
 */
void lspr3_close(void);

#ifdef __cplusplus
}
#endif

#endif
