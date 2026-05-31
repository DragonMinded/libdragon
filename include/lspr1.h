/**
 * @file lspr1.h
 * @brief Lossy-sprite Level 1: BC1Q decoder
 *
 * BC1Q is libdragon's "Lossy-sprite Level 1" container format: it wraps a
 * BC1/DXT1 block-compressed image. Like #lspr3 (Level 3, H264I), BC1Q files are
 * produced by `mksprite --lossy=<quality> --compress 1` and decode at load time
 * into a standard #FMT_RGBA16 sprite.
 *
 * The two lossy-sprite levels target different operating points:
 *
 * - Level 1 (BC1Q) is fixed-rate (4 bpp), block-based, and decodes on the CPU
 *   in a tight per-block loop. It is suitable for large textures where storage
 *   cost matters but per-block reconstruction is good enough.
 * - Level 3 (H264I) uses H.264 intra coding via the RSP and produces higher
 *   quality at variable rate, at the cost of a heavier runtime decoder.
 *
 * You must opt-in to BC1Q support by calling #lspr1_init to register the BC1Q
 * decoder with #sprite_load before loading BC1Q files. Once initialized,
 * #sprite_load recognizes BC1Q-compressed sprites and decodes them
 * transparently.
 *
 * The BC1 alpha mode is DXT1a punch-through: each 4x4 block independently
 * chooses between a 4-color opaque palette and a 3-color + transparent
 * palette, mapping naturally to RGBA16's single alpha bit. Mipmaps are not
 * supported by BC1Q v1.
 */
#ifndef __LIBDRAGON_LSPR1_H
#define __LIBDRAGON_LSPR1_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register the BC1Q (Lossy-sprite Level 1) decoder with the sprite loader.
 *
 * After registering, #sprite_load recognizes the `BC1Q` magic and decodes
 * matching files transparently. Until then, loading a BC1Q file via
 * #sprite_load fails with an assertion.
 *
 * Refcounted; calling this increments the refcount and will require the
 * same number of #lspr1_close calls to fully unregister.
 */
void lspr1_init(void);

/**
 * @brief Unregister the BC1Q (Lossy-sprite Level 1) decoder.
 *
 * Refcounted; calling this decrements the refcount and once it reaches zero,
 * #sprite_load no longer recognizes BC1Q files and attempts to load them will
 * fail with an assertion.
 */
void lspr1_close(void);

#ifdef __cplusplus
}
#endif

#endif
