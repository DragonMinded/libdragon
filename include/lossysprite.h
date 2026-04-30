/**
 * @file lossysprite.h
 * @brief LSPR (lossy sprite) decoder
 *
 * LSPR is libdragon's lossy sprite container: an H.264 intra-only bitstream
 * (4:2:0 YUV) plus a small header recording the chroma format, colorspace,
 * and a *target memory format*. Files are produced at build time by
 * `mksprite --lossy [--format=<X>]`. At load time the H.264 slice is fully
 * decoded on RSP+CPU and the YUV reconstruction is converted into the
 * requested target format, so the returned #sprite_t looks and renders like
 * a normal sprite of that format.
 *
 * Target formats:
 *
 * - default / `--format=RGBA16` -> #FMT_RGBA16 sprite
 * - `--format=RGBA32`           -> #FMT_RGBA32 sprite
 * - `--format=UYVY`             -> #FMT_YUV16 sprite (packed 4:2:2)
 * - `--format=NV12`             -> NV12 sprite: Y plane (#FMT_I8) +
 *                                  interleaved UV plane (#FMT_IA16),
 *                                  tagged as #FMT_YUV16 with
 *                                  #SPRITE_FLAG_YUV_NV12 set internally
 *
 * The first three render through the standard sprite paths
 * (#rdpq_sprite_blit and #rdpq_sprite_upload). NV12 is a special case: it
 * routes #rdpq_sprite_blit through #yuv_tex_blit (with #YUV_NV12) using
 * the colorspace stored in the file header. Because NV12 splits the image
 * across two TMEM banks, NV12 sprites are not compatible with
 * #rdpq_sprite_upload — they must be drawn via #rdpq_sprite_blit.
 *
 * Typical usage goes through #sprite_load, which sniffs the LSPR magic and
 * dispatches here automatically once #lossysprite_init has registered the
 * decoder. Call #lossysprite_decode_buf directly only when the encoded
 * bytes are already in memory (e.g. embedded as a DSO data blob).
 */
#ifndef __LIBDRAGON_LOSSYSPRITE_H
#define __LIBDRAGON_LOSSYSPRITE_H

#include <stdbool.h>
#include "sprite.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Register the LSPR decoder with the sprite loader.
 *
 * After this call, #sprite_load recognizes the `LSPR` magic and routes
 * matching files through #lossysprite_decode_buf. Until then, loading an
 * LSPR file via #sprite_load fails. Safe to call multiple times.
 */
void lossysprite_init(void);

/**
 * @brief Unregister the LSPR decoder.
 *
 * Reverses #lossysprite_init. After this call, #sprite_load no longer
 * recognizes LSPR files; #lossysprite_decode_buf and #lossysprite_load
 * still work directly.
 */
void lossysprite_close(void);

/**
 * @brief Decode an LSPR file already loaded in memory.
 *
 * Equivalent to #lossysprite_load, but operates on a buffer the caller
 * supplies instead of reading from the filesystem. The H.264 intra slice
 * is fully decoded during this call and the YUV reconstruction is
 * converted into the target format recorded in the LSPR header (see the
 * file-level documentation above for the target formats).
 *
 * The buffer is only read during the call; the caller retains ownership
 * and may free or reuse it as soon as this function returns. The returned
 * sprite owns its pixel storage and must be released with #sprite_free.
 *
 * @param buf   Pointer to the encoded LSPR file contents
 * @param sz    Size of @p buf in bytes
 * @return      The decoded sprite (free with #sprite_free)
 */
sprite_t *lossysprite_decode_buf(const void *buf, int sz);

/**
 * @brief Open and decode an LSPR file into memory.
 *
 * Reads @p fn from the asset filesystem and decodes it into a #sprite_t
 * of the target memory format recorded in the file header (see the
 * file-level documentation above). The returned sprite owns its pixel
 * storage and must be released with #sprite_free.
 *
 * Prefer #sprite_load when the same code path needs to handle both
 * regular and lossy sprites: it sniffs the LSPR magic and delegates here
 * automatically once #lossysprite_init has registered the decoder.
 *
 * @param fn    Path to the LSPR file
 * @return      The loaded sprite (free with #sprite_free)
 */
sprite_t* lossysprite_load(const char *fn);

#ifdef __cplusplus
}
#endif

#endif
