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

#include <stdbool.h>
#include <stddef.h>
#include "sprite.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Required alignment of the destination buffer for
 *        #lossysprite_load_into.
 */
#define LOSSYSPRITE_DECODE_ALIGN 64

/**
 * @brief File-start signature of an LSPR file (8 bytes).
 *
 * The leading four zero bytes are a safety pad: if an LSPR file reaches
 * #sprite_load without #lossysprite_init having registered the decoder,
 * the buffer is reinterpreted as a #sprite_t — the pad forces width=height=0
 * so the situation is detectable and a clear assertion can fire instead of
 * silently producing a corrupt sprite.
 */
#define LSPR_FILE_MAGIC      "\0\0\0\0LSPR"
#define LSPR_FILE_MAGIC_SIZE 8


/**
 * @brief Register the LSPR decoder with the sprite loader.
 *
 * After this call, #sprite_load recognizes the `LSPR` magic and routes
 * matching files through #lossysprite_load_buf. Until then, loading an
 * LSPR file via #sprite_load fails. Safe to call multiple times.
 */
void lossysprite_init(void);

/**
 * @brief Unregister the LSPR decoder.
 *
 * Reverses #lossysprite_init. After this call, #sprite_load no longer
 * recognizes LSPR files; #lossysprite_load_buf and #lossysprite_load
 * still work directly.
 */
void lossysprite_close(void);

/**
 * @brief Decode an LSPR file already loaded in memory.
 *
 * Equivalent to #lossysprite_load, but operates on a buffer the caller
 * supplies instead of reading from the filesystem. The H.264 intra slice
 * is fully decoded during this call and the YUV reconstruction is
 * converted into a #FMT_RGBA16 sprite.
 *
 * The buffer is only read during the call; the caller retains ownership
 * and may free or reuse it as soon as this function returns. The returned
 * sprite owns its pixel storage and must be released with #sprite_free.
 *
 * @param buf   Pointer to the encoded LSPR file contents
 * @param sz    Size of @p buf in bytes
 * @return      The decoded sprite (free with #sprite_free)
 */
sprite_t *lossysprite_load_buf(const void *buf, int sz);

/**
 * @brief Open and decode an LSPR file into memory.
 *
 * Reads @p fn from the asset filesystem and decodes it into a #FMT_RGBA16
 * #sprite_t. The returned sprite owns its pixel storage and must be released
 * with #sprite_free.
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
 * @brief Compute the size of the decoded sprite for an LSPR buffer.
 *
 * Reads only the LSPR header (no H.264 decode). The returned size is the
 * number of bytes #lossysprite_load_into needs in its output buffer to
 * hold the fully decoded #sprite_t (including its `sprite_ext_t` and
 * pixel data).
 *
 * Returns 0 if @p buf is not a valid LSPR file.
 *
 * @param buf   Pointer to the LSPR buffer
 * @param sz    Size of @p buf in bytes
 * @return      Number of bytes required for the decoded sprite
 */
size_t lossysprite_decoded_size_buf(const void *encoded_buf, int encoded_sz);

/**
 * @brief Decode an LSPR buffer into a caller-provided output buffer.
 *
 * Same decode pipeline as #lossysprite_load_buf, but writes the decoded
 * #sprite_t into @p out instead of allocating a fresh buffer with
 * `memalign`. The returned sprite does NOT have #SPRITE_FLAGS_OWNEDBUFFER
 * set — the caller owns @p out and is responsible for freeing it.
 *
 * The @p out buffer must be at least #lossysprite_decoded_size_buf bytes
 * long and aligned to #LOSSYSPRITE_DECODE_ALIGN bytes.
 *
 * @param buf       Pointer to the encoded LSPR file contents
 * @param sz        Size of @p buf in bytes
 * @param out       Pointer to the output buffer (64-byte aligned)
 * @param out_sz    Size of @p out in bytes
 * @return          The decoded sprite (lives in @p out)
 */
sprite_t *lossysprite_load_into(const void *buf, int sz, void *out, size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif
