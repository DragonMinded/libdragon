/**
 * @file rdpq_blendfx.h
 * @brief RDPQ destination-aware blend effects
 * @ingroup rdpq
 *
 * BlendFX composites a surface with the pixels already present in the active
 * color image. It provides effects such as saturated addition, subtraction,
 * multiplication, and screen, while retaining the positioning and transform
 * conventions of #rdpq_tex_blit.
 *
 * BlendFX is slower than ordinary texture blitting or effects expressible 
 * through #rdpq_mode_blender, because it must draw sprites in chunks 
 * (even if they fit TMEM). Prefer the native RDP blender whenever it can 
 * produce the required result, such as alpha blending.
 */

#ifndef LIBDRAGON_RDPQ_BLENDFX_H
#define LIBDRAGON_RDPQ_BLENDFX_H

#include "rdpq_tex.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Built-in BlendFX effect.
 */
typedef enum {
    RDPQ_BLENDFX_ADD = 0,   ///< Add a color-modulated source contribution to the destination.
    RDPQ_BLENDFX_SUBTRACT,  ///< Subtract a color-modulated source contribution from the destination.
    RDPQ_BLENDFX_MULTIPLY,  ///< Multipliy destination by the color-modulated source.
    RDPQ_BLENDFX_SCREEN,    ///< Screen the color-modulated source over the destination.
} rdpq_blendfx_t;

/**
 * @brief Optional parameters for a built-in BlendFX effect.
 *
 * These presets are optional. Applications can configure a custom combiner
 * and still draw through #rdpq_blendfx_blit. In that case TEX0 contains the
 * source surface and TEX1 contains the current destination pixels.
 *
 * Passing NULL to #rdpq_set_blendfx_parms selects a neutral white color at
 * full strength and disables transparency. A zero-initialized structure has
 * the same behavior.
 */
typedef struct {
    color_t color;      ///< Source color and effect strength. An all-zero value selects neutral full strength.
    bool transparency; ///< If true, discard source texels whose alpha is zero. Defaults to false.
} rdpq_blendfx_parms_t;

/**
 * @brief Set the parameters for a built-in BlendFX effect.
 *
 * This function configures alpha comparison, the combiner, and the color
 * registers used by the selected effect. It does not select or reset the base
 * render mode: call #rdpq_set_mode_standard before using it. Calling this
 * helper in another base mode is unsupported.
 *
 * BlendFX does not configure texture filtering. Using #rdpq_mode_filter with
 * #FILTER_BILINEAR is recommended because centered samples reduce artifacts
 * from the packed source and destination coordinates used internally.
 *
 * When @p parms is not NULL, its RGB color components modulate the source and
 * its alpha component controls effect strength. An all-zero color is interpreted
 * as opaque white. For #RDPQ_BLENDFX_ADD and #RDPQ_BLENDFX_SUBTRACT, alpha 255
 * corresponds to a maximum source contribution of 50 percent.
 * #RDPQ_BLENDFX_MULTIPLY and #RDPQ_BLENDFX_SCREEN use the full 0-100 percent
 * range.
 *
 * If `parms->transparency` is true, source texels with alpha zero are discarded
 * through alpha comparison. Add and subtract still use source alpha as
 * continuous effect coverage; multiply and screen use it only as this binary
 * mask. Alpha comparison in two-cycle mode can be shifted by one pixel because
 * of an RDP hardware bug. If transparency is false, alpha comparison is
 * disabled and every covered pixel is written.
 *
 * @param effect  Built-in effect to configure
 * @param parms   Optional color and transparency parameters, or NULL for defaults
 *
 * @see rdpq_set_mode_standard
 * @see rdpq_mode_filter
 * @see rdpq_mode_alphacompare
 * @see rdpq_blendfx_blit
 * @see rdpq_mode_combiner
 */
void rdpq_set_blendfx_parms(rdpq_blendfx_t effect,
    const rdpq_blendfx_parms_t *parms);

/**
 * @brief Begin a sequence of BlendFX blits that reuse the same source.
 *
 * Use a multi scope when drawing one compatible source repeatedly. The first
 * blit prepares the source, and later blits can reuse it without another source
 * upload. The begin and end calls themselves emit no RDP commands.
 *
 * Keep the same attachment active for the whole scope. Do not interleave other
 * texture uploads or textured draws that replace the texture state used by
 * BlendFX. Source storage must remain valid and unchanged until the queued
 * commands have consumed it.
 *
 * Multi scopes can be recorded in an #rspq_block_t. Calls may be nested; only
 * the outermost begin/end pair controls reuse.
 *
 * @see rdpq_blendfx_multi_end
 * @see rdpq_blendfx_blit
 */
void rdpq_blendfx_multi_begin(void);

/**
 * @brief Finish a BlendFX multi-blit sequence.
 *
 * Ending the outermost scope discards reusable source state. It emits no RDP
 * commands.
 *
 * @see rdpq_blendfx_multi_begin
 */
void rdpq_blendfx_multi_end(void);

/**
 * @brief Composite a surface with the active color image.
 *
 * This function follows the positioning, source-rectangle, hotspot, scaling,
 * flipping, and rotation conventions of #rdpq_tex_blit. The selected source is
 * TEX0 and the current destination is TEX1, allowing either a built-in effect
 * configured by #rdpq_set_blendfx_parms or a custom combiner.
 *
 * The active destination must be an attached RGBA16 surface. Supported source
 * formats are RGBA16, IA16, IA8, I8, IA4, and I4. CI textures are not supported.
 * Bilinear filtering is required; passing `filtering=false` is invalid.
 * `allow_xform` is not supported, and @p parms must select TILE0 through TILE6.
 *
 * Axis-aligned draws are split automatically when needed. Rotated draws, and
 * draws using #rdpq_blendfx_blit_uv_scaled, require the complete selected source
 * region to fit. The function is slower than #rdpq_tex_blit because it must draw
 * in chunks and emit separate commands for each chunk, even when the source fits
 * in TMEM.
 *
 * A standalone call is self-contained. Use #rdpq_blendfx_multi_begin when the
 * same source is drawn repeatedly. BlendFX calls can be recorded in an
 * #rspq_block_t; at playback they operate on the color image active when the
 * command executes, not the attachment used while recording.
 *
 * @param surf   Source surface
 * @param x0     X coordinate corresponding to the transformation hotspot
 * @param y0     Y coordinate corresponding to the transformation hotspot
 * @param parms  Blit parameters, or NULL for bilinear-filtered defaults
 *
 * @see rdpq_set_blendfx_parms
 * @see rdpq_blendfx_multi_begin
 * @see rdpq_tex_blit
 * @see rdpq_blitparms_t
 */
void rdpq_blendfx_blit(const surface_t *surf, float x0, float y0,
    const rdpq_blitparms_t *parms);

/**
 * @brief Composite with source coordinates scaled inside unchanged draw bounds.
 *
 * This variant has the same behavior as #rdpq_blendfx_blit, but scales source
 * coordinates around the transformation hotspot without changing destination
 * geometry. Values below 1 sample a smaller central region; this is useful for
 * excluding transparent gutters around rotating sprites.
 *
 * The complete selected source region must fit, and @p uv_scale must be
 * positive. The source surface is not modified.
 *
 * @param surf      Source surface
 * @param x0        X coordinate corresponding to the transformation hotspot
 * @param y0        Y coordinate corresponding to the transformation hotspot
 * @param uv_scale  Source-coordinate scale around the hotspot
 * @param parms     Blit parameters, or NULL for bilinear-filtered defaults
 *
 * @see rdpq_blendfx_blit
 * @see rdpq_blitparms_t
 */
void rdpq_blendfx_blit_uv_scaled(const surface_t *surf, float x0, float y0,
    float uv_scale, const rdpq_blitparms_t *parms);

#ifdef __cplusplus
}
#endif

#endif
