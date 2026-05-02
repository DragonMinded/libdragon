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
 * - `--format=NV12`             -> semi-planar 4:2:0 sprite: Y plane
 *                                  (#FMT_I8) + interleaved UV plane
 *                                  (#FMT_IA16, half height), tagged as
 *                                  #FMT_YUV16 with the semi-planar layout
 *                                  recorded in #sprite_ext_t::yuv_attrs
 * - `--format=NV16`             -> semi-planar 4:2:2 sprite: Y plane
 *                                  (#FMT_I8) + interleaved UV plane
 *                                  (#FMT_IA16, full height) — chroma is
 *                                  vertically upsampled from the I420
 *                                  reconstruction at decode time
 *
 * The first three render through the standard sprite paths
 * (#rdpq_sprite_blit and #rdpq_sprite_upload). The semi-planar formats
 * (NV12 and NV16) are special cases: #rdpq_sprite_blit dispatches them
 * through #yuv_tex_blit using the colorspace stored in the file header.
 * Because semi-planar sprites split the image across two TMEM banks, they
 * are not compatible with #rdpq_sprite_upload — they must be drawn via
 * #rdpq_sprite_blit.
 *
 * Typical usage goes through #sprite_load, which sniffs the LSPR magic and
 * dispatches here automatically once #lossysprite_init has registered the
 * decoder. Call #lossysprite_decode_buf directly only when the encoded
 * bytes are already in memory (e.g. embedded as a DSO data blob).
 */
#ifndef __LIBDRAGON_LOSSYSPRITE_H
#define __LIBDRAGON_LOSSYSPRITE_H

#include <stdbool.h>
#include <stddef.h>
#include "sprite.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Required alignment of the destination buffer for
 *        #lossysprite_decode_into.
 */
#define LOSSYSPRITE_DECODE_ALIGN 64


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

/**
 * @brief Test whether @p buf holds an LSPR-encoded sprite.
 *
 * Cheap header-only check: validates the LSPR magic and version. Returns
 * false for non-LSPR data without asserting, so it is safe to use as a
 * dispatch test on a buffer that may or may not be LSPR-encoded.
 *
 * @param buf   Pointer to the buffer to inspect (may be NULL)
 * @param sz    Size of @p buf in bytes
 * @return      true if @p buf is a recognized LSPR file, false otherwise
 */
bool lossysprite_is_encoded(const void *buf, int sz);

/**
 * @brief Compute the size of the decoded sprite for an LSPR buffer.
 *
 * Reads only the LSPR header (no H.264 decode). The returned size is the
 * number of bytes #lossysprite_decode_into needs in its output buffer to
 * hold the fully decoded #sprite_t (including its `sprite_ext_t` and
 * pixel/plane data).
 *
 * Asserts if @p buf is not a valid LSPR file; call #lossysprite_is_encoded
 * first if the buffer's contents are not yet known.
 *
 * @param buf   Pointer to the LSPR buffer
 * @param sz    Size of @p buf in bytes
 * @return      Number of bytes required for the decoded sprite
 */
size_t lossysprite_get_decoded_size(const void *buf, int sz);

/**
 * @brief Decode an LSPR buffer into a caller-provided output buffer.
 *
 * Same decode pipeline as #lossysprite_decode_buf, but writes the decoded
 * #sprite_t into @p out instead of allocating a fresh buffer with
 * `memalign`. The returned sprite does NOT have #SPRITE_FLAGS_OWNEDBUFFER
 * set — the caller owns @p out and is responsible for freeing it.
 * Calling #sprite_free on the returned sprite is therefore a no-op (other
 * than dropping any internal references).
 *
 * The output buffer must be at least
 * #lossysprite_get_decoded_size bytes long and aligned to
 * #LOSSYSPRITE_DECODE_ALIGN bytes. Both conditions are asserted.
 *
 * @param buf       Pointer to the encoded LSPR file contents
 * @param sz        Size of @p buf in bytes
 * @param out       Pointer to the output buffer (64-byte aligned)
 * @param out_sz    Size of @p out in bytes
 * @return          The decoded sprite (lives in @p out)
 */
sprite_t *lossysprite_decode_into(const void *buf, int sz, void *out, size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif
