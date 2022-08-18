/**
 * @file rdpq_mode.h
 * @brief RDP Command queue: mode setting
 * @ingroup rdp
 * 
 * The mode API is a high level API to simplify mode setting with RDP. Configuring
 * render modes is possibly the most complex task with RDP programming, as the RDP
 * is full of hardware features that interact badly between them or are in general
 * non-orthogonal. The mode API tries to hide much of the complexity between an API
 * more similar to a modern graphic API like OpenGL.
 * 
 * In general, mode setting with RDP is performed via two commands SET_COMBINE_MODE
 * and SET_OTHER_MODES. These two commands are available as "raw" commands in the
 * basic rdpq API as #rdpq_set_combiner_raw and #rdpq_set_other_modes_raw. These
 * two functions set the specified configurations into the RDP hardware registers,
 * and do nothing else, so they can always be used to do manual RDP programming.
 * 
 * Instead, the mode API follows the following pattern:
 * 
 *   * First, one of the basic render modes must be set via one of the `rdpq_set_mode_*` functions.
 *   * Afterwards, it is possible to tweak the current render mode via on of the various
 *     `rdpq_mode_*` functions.
 * 
 * The rdpq mode API currently offers the following render modes:
 * 
 *   * **Standard** (#rdpq_set_mode_standard). This is the most basic and general
 *     render mode. It allows to use all RDP features (that must be activated via the
 *     various `rdpq_set_mode_*` functions). 
 *   * **Copy** (#rdpq_set_mode_copy). This is a fast (4x) mode in which the RDP
 *     can perform fast blitting of textured rectangles (aka sprites). All texture
 *     formats are supported, and color 0 can be masked for transparency. Textures
 *     can be scaled and rotated, but not mirrored.
 *   * **Fill** (#rdpq_set_mode_fill). This is a fast (4x) mode in which the RDP
 *     is able to quickly fill a rectangular portion of the target buffer with a
 *     fixed color. It can be used to clear the screen.
 *   * **YUV** (#rdpq_set_mode_yuv). This is a render mode that can be used to
 *     blit YUV textures, converting them to RGB. Support for YUV textures in RDP
 *     does in fact require a specific render mode (you cannot use YUV textures
 *     otherwise). It is possible to decide whether to activate or not bilinear
 *     filtering, as it makes RDP 2x slow when used in this mode.
 *   
 * @note From a hardware perspective, rdpq handles automatically the "RDP cycle type".
 *       That is, it transparently switches from "1-cycle mode" to "2-cycle mode"
 *       whenever it is necessary. If you come from a RDP low-level programming
 *       background, it might be confusing at first because everything "just works"
 *       without needing to adjust settings any time you need to switch between
 *       the two modes.
 * 
 * ## Mode setting stack
 * 
 * The mode API also keeps a small (4 entry) stack of mode configurations. This
 * allows client code to temporarily switch render mode and then get back to 
 * the previous mode, which helps modularizing the code.
 * 
 * To save the current render mode onto the stack, use #rdpq_mode_push. To restore
 * the previous render mode from the stack, use #rdpq_mode_pop.
 * 
 * Notice the mode settings being part of this stack are those which are configured
 * via the mode API functions itself (`rdpq_set_mode_*` and `rdpq_mode_*`). Anything
 * that doesn't go through the mode API is not saved/restored. For instance,
 * activating blending via #rdpq_mode_blending is saved onto the stack, whilst
 * changing the BLEND color register (via #rdpq_set_blend_color) is not, and you
 * can tell by the fact that the function called to configure it is not part of
 * the mode API.
 * 
 */
#ifndef LIBDRAGON_RDPQ_MODE_H
#define LIBDRAGON_RDPQ_MODE_H

#include "rdpq.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

///@cond
// Internal helpers, not part of the public API
inline void __rdpq_mode_change_som(uint64_t mask, uint64_t val);
///@endcond

/**
 * @brief Push the current render mode into the stack
 * 
 * This function allows to push the current render mode into an internal stack.
 * It allows to temporarily modify the render mode, and later recover its value.
 * 
 * This is effective on all render mode changes that can be modified via
 * rdpq_mode_* function. It does not affect other RDP configurations such as
 * the various colors.
 * 
 * The stack has 4 slots (including the current one).
 */

void rdpq_mode_push(void);

/**
 * @brief Pop the current render mode from the stack
 * 
 * This function allows to pop a previously pushed render mode from the stack,
 * setting it as current again.
 */

void rdpq_mode_pop(void);

typedef uint64_t rdpq_combiner_t;
typedef uint32_t rdpq_blender_t;

typedef enum rdpq_sampler_s {
    SAMPLER_POINT    = SOM_SAMPLE_POINT    >> SOM_SAMPLE_SHIFT,
    SAMPLER_BILINEAR = SOM_SAMPLE_BILINEAR >> SOM_SAMPLE_SHIFT,
    SAMPLER_MEDIAN   = SOM_SAMPLE_MEDIAN   >> SOM_SAMPLE_SHIFT,
} rdpq_sampler_t;

typedef enum rdpq_dither_s {
    DITHER_SQUARE = 0,
    DITHER_BAYER,
    DITHER_NOISE,
    DITHER_NONE
} rdpq_dither_t;

typedef enum rdpq_tlut_s {
    TLUT_NONE   = 0,
    TLUT_RGBA16 = 2,
    TLUT_IA16   = 3,
} rdpq_tlut_t;

/**
 * @brief Reset render mode to standard.
 * 
 * This is the most basic and general mode reset function. It configures the RDP
 * processor in a standard and very basic way:
 * 
 *   * Basic texturing (without shading)
 *   * No dithering, antialiasing, blending, etc.
 * 
 * You can further configure the mode by calling one of the many functions
 * in the mode API (`rdpq_mode_*`).
 */
void rdpq_set_mode_standard(void);


/**
 * @brief Reset render mode to FILL type.
 * 
 * This function sets the render mode type to FILL, which is used to quickly
 * fill portions of the screens with a solid color. The specified color is
 * configured via #rdpq_set_fill_color, and can be changed later.
 * 
 * Notice that in FILL mode most of the RDP features are disabled, so all other
 * render modes settings (rdpq_mode_* functions) do not work.
 *
 * @param[in]  color  The fill color to use
 */
inline void rdpq_set_mode_fill(color_t color) {
    rdpq_set_other_modes_raw(SOM_CYCLE_FILL);
    rdpq_set_fill_color(color);
}

/**
 * @brief Reset render mode to COPY type.
 * 
 * This function sets the render mode type to COPY, which is used to quickly
 * blit bitmaps. In COPY mode, only texture rectangles (aka "sprites") can be
 * drawn and no advanced render mode features are working (rdpq_mode_* functions).
 * 
 * The only available feature is transparency: pixels with alpha set to 0 can
 * optionally be discarded during blit, so that the target buffer contents is
 * not overwritten for those pixels. This is implemented using alpha compare.
 * 
 * The COPY mode is approximately 4 times faster at drawing than the standard
 * mode, so make sure to enable it whenever it is possible.
 * 
 * @param[in]  transparency   If true, pixels with alpha set to 0 are not drawn
 * 
 * @see #rdpq_set_mode_standard
 */
void rdpq_set_mode_copy(bool transparency);

/**
 * @brief Reset render mode to YUV mode.
 * 
 * This is a helper function to configure a render mode for YUV conversion.
 * In addition of setting the render mode, this function also configures a
 * combiner (given that YUV conversion happens also at the combiner level),
 * and set standard YUV parameters (for BT.601 TV Range).
 * 
 * After setting the YUV mode, you can load YUV textures to TMEM (using a
 * surface with #FMT_YUV16), and then draw them on the screen as part of
 * triangles or rectangles.
 * 
 * @param[in] bilinear      If true, YUV textures will also be filtered with
 *                          bilinear interpolation (note: this will require
 *                          2-cycle mode so it will be twice as slow).
 */
void rdpq_set_mode_yuv(bool bilinear);

/**
 * @brief Activate antialiasing
 * 
 * This function can be used to enable/disable antialias at the RDP level.
 * There are two different kinds of antialias on N64:
 * 
 *   * Antialias on internal edges: this is fully performed by RDP.
 *   * Antialias on external edges: this is prepared by RDP but is actually
 *     performed as a post-processing filter by VI.
 * 
 * This function activates both kinds of antialias, but to display correctly
 * the second type, make sure that you did not pass #ANTIALIAS_OFF to
 * #display_init.
 * 
 * @note Antialiasing internally uses the blender unit. If you already
 *       configured a formula via #rdpq_mode_blending, antialias will just
 *       rely on that one to correctly blend pixels with the framebuffer.
 * 
 * @param enable        Enable/disable antialiasing
 */
inline void rdpq_mode_antialias(bool enable) 
{
    // Just enable/disable SOM_AA_ENABLE. The RSP will then update the render mode
    // which would trigger different other bits in SOM depending on the current mode.
    __rdpq_mode_change_som(SOM_AA_ENABLE, enable ? SOM_AA_ENABLE : 0);
}

inline void rdpq_mode_combiner(rdpq_combiner_t comb) {
    extern void __rdpq_fixup_mode(uint32_t cmd_id, uint32_t w0, uint32_t w1);

    if (comb & RDPQ_COMBINER_2PASS)
        __rdpq_fixup_mode(RDPQ_CMD_SET_COMBINE_MODE_2PASS,
            (comb >> 32) & 0x00FFFFFF,
            comb & 0xFFFFFFFF);
    else
        __rdpq_fixup_mode(RDPQ_CMD_SET_COMBINE_MODE_1PASS,
            (comb >> 32) & 0x00FFFFFF,
            comb & 0xFFFFFFFF);
}

/** @brief Blending mode: multiplicative alpha.
 * You can pass this macro to #rdpq_mode_blending. */
#define RDPQ_BLEND_MULTIPLY       RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, INV_MUX_ALPHA))
/** @brief Blending mode: additive alpha.
 * You can pass this macro to #rdpq_mode_blending. */
#define RDPQ_BLEND_ADDITIVE       RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, ONE))      

/**
 * @brief Configure the formula for the second pass of the blender unit.
 * 
 * This function can be used to configure the formula used for the
 * second pass of the blender unit. This pass is conventionally used
 * to implement the blending between the polygon being drawn and the
 * background, so the name of the function reflects that.
 * 
 * The other pass can be configured with #rdpq_mode_fog. If the other
 * pass is disabled, the pass configured via #rdpq_mode_blending will
 * be the only one to run.
 * 
 * The standard blending formulas are:
 * 
 *  * #RDPQ_BLEND_MULTIPLY: multiplicative alpha blending
 *  * #RDPQ_BLEND_ADDITIVE: additive alpha blending
 * 
 * but custom formulas can be created using the #RDPQ_BLENDER macro.
 * 
 * The following example shows how to draw a texture rectangle using
 * a fixed blending value of 0.5 (ignoring the alpha channel of the
 * texture):
 * 
 * @code{.c}
 *      // Set standard mode
 *      rdpq_set_mode_standard();
 * 
 *      // Configure the formula:
 *      //      (IN_RGB * FOG_ALPHA) + (MEMORY_RGB * (1 - FOG_ALPHA))
 *      //
 *      // where FOG_ALPHA is the fixed alpha value coming from the FOG register.
 *      rdpq_mode_blending(RDPQ_BLENDER(IN_RGB, FOG_ALPHA, MEMORY_RGB, INV_MUX_ALPHA));
 * 
 *      // Configure the FOG_ALPHA value to 128 (= 0.5). The RGB components are
 *      // not used.
 *      rdpq_set_fog_color(RGBA32(0,0,0, 128));
 * 
 *      // Load a texture into TMEM
 *      rdpq_tex_load(TILE0, texture, 0);
 * 
 *      // Draw it
 *      rdpq_texture_rectangle(TILE0,
 *          0, 0, 64, 64,   // x0,y0 - x1,y1
 *          0, 0, 1.0, 1.0  // s0,t0 - ds,dt
 *      );
 * @endcode 
 *
 * @param blend          Blending formula created with #RDPQ_BLENDER,
 *                       or 0 to disable.
 * 
 * @see #rdpq_mode_fog
 * @see #RDPQ_BLENDER
 * @see #RDPQ_BLEND_MULTIPLY
 * @see #RDPQ_BLEND_ADDITIVE
 */
inline void rdpq_mode_blending(rdpq_blender_t blend) {
    extern void __rdpq_fixup_mode(uint32_t cmd_id, uint32_t w0, uint32_t w1);
    if (blend) blend |= SOM_BLENDING;
    __rdpq_fixup_mode(RDPQ_CMD_SET_BLENDING_MODE, 4, blend);
}

/** @brief Fogging mode: standard.
 * You can pass this macro to #rdpq_mode_fog. */
#define RDPQ_FOG_STANDARD         RDPQ_BLENDER((IN_RGB, SHADE_ALPHA, FOG_RGB, INV_MUX_ALPHA))


/**
 * @brief Configure the formula for the first pass of the blender unit.
 * 
 * This function can be used to configure the formula used for the
 * first pass of the blender unit. This pass is conventionally used
 * to implement fogging, so the name of the function reflects that.
 * 
 * The other pass can be configured with #rdpq_mode_blending. If the other
 * pass is disabled, the pass configured via #rdpq_mode_fog will
 * be the only one to run.
 * 
 * A standard fog formula is #RDPQ_FOG_STANDARD, or a custom formula
 * can be created using #RDPQ_BLENDER.
 * 
 * See #rdpq_mode_blending for an example.
 * 
 * @param fog            Fog formula created with #RDPQ_BLENDER,
 *                       or 0 to disable.
 * 
 * @see #rdpq_mode_blending
 * @see #RDPQ_BLENDER
 * @see #RDPQ_FOG_STANDARD
 */
inline void rdpq_mode_fog(rdpq_blender_t fog) {
    extern void __rdpq_fixup_mode(uint32_t cmd_id, uint32_t w0, uint32_t w1);
    if (fog) fog |= SOM_BLENDING;
    __rdpq_fixup_mode(RDPQ_CMD_SET_BLENDING_MODE, 0, fog);
}

inline void rdpq_mode_dithering(rdpq_dither_t rgb, rdpq_dither_t alpha) {
    rdpq_change_other_modes_raw(
        SOM_RGBDITHER_MASK | SOM_ALPHADITHER_MASK,
        ((uint64_t)rgb << SOM_RGBDITHER_SHIFT) | ((uint64_t)alpha << SOM_ALPHADITHER_SHIFT));
}

inline void rdpq_mode_alphacompare(bool enable, int threshold) {
    if (enable && threshold > 0) rdpq_set_blend_color(RGBA32(0,0,0,threshold));
    rdpq_change_other_modes_raw(
        SOM_ALPHACOMPARE_MASK, enable ? SOM_ALPHACOMPARE_THRESHOLD : 0
    );
}

inline void rdpq_mode_zoverride(bool enable, uint16_t z, int16_t deltaz) {
    if (enable) rdpq_set_prim_depth(z, deltaz);
    rdpq_change_other_modes_raw(
        SOM_ZSOURCE_PRIM, enable ? SOM_ZSOURCE_PRIM : 0
    );
}

inline void rdpq_mode_tlut(rdpq_tlut_t tlut) {
    rdpq_change_other_modes_raw(SOM_TLUT_MASK, (uint64_t)tlut << SOM_TLUT_SHIFT);
}

inline void rdpq_mode_sampler(rdpq_sampler_t samp) {
    rdpq_change_other_modes_raw(SOM_SAMPLE_MASK, (uint64_t)samp << SOM_SAMPLE_SHIFT);
}

/********************************************************************
 * Internal functions (not part of public API)
 ********************************************************************/

///@cond
inline void __rdpq_mode_change_som(uint64_t mask, uint64_t val)
{
    // This is identical to #rdpq_change_other_modes_raw, but we also
    // set bit 1<<15 in the first word. That flag tells the RSP code
    // to recalculate the render mode, in addition to flipping the bits.
    // #rdpq_change_other_modes_raw instead just changes the bits as
    // you would expect from a raw API.
    extern void __rdpq_fixup_mode3(uint32_t cmd_id, uint32_t w0, uint32_t w1, uint32_t w2);
    if (mask >> 32)
        __rdpq_fixup_mode3(RDPQ_CMD_MODIFY_OTHER_MODES, 0 | (1<<15), ~(mask >> 32), val >> 32);
    if ((uint32_t)mask)
        __rdpq_fixup_mode3(RDPQ_CMD_MODIFY_OTHER_MODES, 4 | (1<<15), ~(uint32_t)mask, (uint32_t)val);
}
///@endcond


#ifdef __cplusplus
}
#endif

#endif
