/**
 * @file lspr1.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Lossy-sprite Level 1: BC1Q decoder
 *
 * BC1Q is one of libdragon's lossy sprite formats ("Lossy-sprite Level 1"),
 * a good fit for large textures where a low decoding cost matters. It is the
 * lighter-weight alternative to #lspr3_init (Level 3, H264I), which trades a
 * heavier runtime decoder for higher quality at a given size.
 *
 * BC1Q files are produced by `mksprite --lossy=<quality> --compress 1` and
 * decode at load time into a standard #FMT_RGBA16 sprite, so the returned
 * #sprite_t behaves like any other sprite.
 *
 * To load BC1Q sprites you must first opt-in by calling #lspr1_init, which
 * registers the decoder with #sprite_load. After that, #sprite_load recognizes
 * BC1Q sprites and decodes them transparently; call #lspr1_close when no longer
 * needed.
 *
 * Internally, the format is based on BC1 / DXT1 block compression, and is
 * decoded using the RSP to accelerate the process.
 */
#ifndef __LIBDRAGON_LSPR1_H
#define __LIBDRAGON_LSPR1_H

#include "preview.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register the BC1Q (Lossy-sprite Level 1) decoder with the sprite loader.
 * @preview
 *
 * After registering, #sprite_load recognizes the `BC1Q` magic and decodes
 * matching files transparently. Until then, loading a BC1Q file via
 * #sprite_load fails with an assertion.
 *
 * Refcounted; calling this increments the refcount and will require the
 * same number of #lspr1_close calls to fully unregister.
 */
LIBDRAGON_PREVIEW_API
void lspr1_init(void);

/**
 * @brief Unregister the BC1Q (Lossy-sprite Level 1) decoder.
 * @preview
 *
 * Refcounted; calling this decrements the refcount and once it reaches zero,
 * #sprite_load no longer recognizes BC1Q files and attempts to load them will
 * fail with an assertion.
 */
LIBDRAGON_PREVIEW_API
void lspr1_close(void);

#ifdef __cplusplus
}
#endif

#endif
