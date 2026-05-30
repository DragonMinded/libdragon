/**
 * @file bcsprite.h
 * @brief BCSP (BC1/DXT1 lossy sprite) decoder
 *
 * BCSP is a lossy sprite container format that wraps a BC1/DXT1 block-
 * compressed image. Like #lossysprite (LSPR), BCSP files are produced by
 * `mksprite --lossy=<quality> --compress 1` and decode at load time into a
 * standard #FMT_RGBA16 sprite.
 *
 * BCSP and LSPR target different operating points:
 *
 * - BCSP is fixed-rate (4 bpp), block-based, and decodes on the CPU in a
 *   tight per-block loop. It is suitable for large textures where storage
 *   cost matters but per-block reconstruction is good enough.
 * - LSPR uses H.264 intra coding via the RSP and produces higher quality at
 *   variable rate, at the cost of a heavier runtime decoder.
 *
 * You must opt-in to BCSP support by calling #bcsprite_init to register the
 * BCSP decoder with #sprite_load before loading BCSP files. Once initialized,
 * #sprite_load recognizes BCSP-compressed sprites and decodes them
 * transparently.
 *
 * The BC1 alpha mode is DXT1a punch-through: each 4x4 block independently
 * chooses between a 4-color opaque palette and a 3-color + transparent
 * palette, mapping naturally to RGBA16's single alpha bit. Mipmaps are not
 * supported by BCSP v1.
 */
#ifndef __LIBDRAGON_BCSPRITE_H
#define __LIBDRAGON_BCSPRITE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register the BCSP decoder with the sprite loader.
 *
 * After registering, #sprite_load recognizes the `BCSP` magic and decodes
 * matching files transparently. Until then, loading a BCSP file via
 * #sprite_load fails with an assertion.
 *
 * Refcounted; calling this increments the refcount and will require the
 * same number of #bcsprite_close calls to fully unregister.
 */
void bcsprite_init(void);

/**
 * @brief Unregister the BCSP decoder.
 *
 * Refcounted; calling this decrements the refcount and once it reaches zero,
 * #sprite_load no longer recognizes BCSP files and attempts to load them will
 * fail with an assertion.
 */
void bcsprite_close(void);

#ifdef __cplusplus
}
#endif

#endif
