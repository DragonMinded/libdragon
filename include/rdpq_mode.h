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
 *   * First, one of the basic **render modes** must be set via one of
 *     the `rdpq_set_mode_*` functions.
 *   * Afterwards, it is possible to tweak the render mode by chang ing
 *     one or more **render states** via  `rdpq_mode_*` functions.
 * 
 * The rdpq mode API currently offers the following render modes:
 * 
 *   * **Standard** (#rdpq_set_mode_standard). This is the most basic and general
 *     render mode. It allows to use all RDP render states (that must be activated via the
 *     various `rdpq_mode_*` functions). 
 *   * **Copy** (#rdpq_set_mode_copy). This is a fast (4x) mode in which the RDP
 *     can perform fast blitting of textured rectangles (aka sprites). All texture
 *     formats are supported, and color 0 can be masked for transparency. Textures
 *     can be scaled and rotated, but not mirrored. Blending is not supported.
 *   * **Fill** (#rdpq_set_mode_fill). This is a fast (4x) mode in which the RDP
 *     is able to quickly fill a rectangular portion of the target buffer with a
 *     fixed color. It can be used to clear the screen. Blending is not supported.
 *   * **YUV** (#rdpq_set_mode_yuv). This is a render mode that can be used to
 *     blit YUV textures, converting them to RGB. Support for YUV textures in RDP
 *     does in fact require a specific render mode (you cannot use YUV textures
 *     otherwise). It is possible to decide whether to activate or not bilinear
 *     filtering, as it makes RDP 2x slow when used in this mode.
 * 
 * After setting the render mode, you can configure the render states. An important
 * implementation effort has been made to try and make the render states orthogonal,
 * so that each one can be toggled separately without inter-dependence (a task
 * which is particularly complex on the RDP hardware). Not all render states are
 * available in all modes, refer to the documentation of each render state for
 * further information.
 * 
 *   * Antialiasing (#rdpq_mode_antialias). Activate antialiasing on both internal
 *     and external edges.
 *   * Combiner (FIXME)
 *   * Blending (FIXME)
 *   * Fog (FIXME)
 *   * Dithering (#rdpq_mode_dithering). Activate dithering on either the RGB channels,
 *     the alpha channel, or both.
 *   * Alpha compare (#rdpq_mode_alphacompare). Activate alpha compare function using
 *     a fixed threshold.
 *   * Z-Override (#rdpq_mode_zoverride): Give a fixed Z value to a whole triangle or
 *     rectangle.
 *   * TLUT (#rdpq_mode_tlut): activate usage of palettes.
 *   * Filtering (#rdpq_mode_filter): activate bilinear filtering.
 * 
 * @note From a hardware perspective, rdpq handles automatically the "RDP cycle type".
 *       That is, it transparently switches from "1-cycle mode" to "2-cycle mode"
 *       whenever it is necessary. If you come from a RDP low-level programming
 *       background, it might be confusing at first because everything "just works"
 *       without needing to adjust settings any time you need to change a render state.
 * 
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
 * activating blending via #rdpq_mode_blender is saved onto the stack, whilst
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

/**
 * @brief Texture filtering types
 */
typedef enum rdpq_filter_s {
    FILTER_POINT    = SOM_SAMPLE_POINT    >> SOM_SAMPLE_SHIFT,  ///< Point filtering (aka nearest)
    FILTER_BILINEAR = SOM_SAMPLE_BILINEAR >> SOM_SAMPLE_SHIFT,  ///< Bilinear filtering
    FILTER_MEDIAN   = SOM_SAMPLE_MEDIAN   >> SOM_SAMPLE_SHIFT,  ///< Median filtering
} rdpq_filter_t;

/**
 * @brief Dithering configuration
 * 
 * RDP can optionally perform dithering on RGB and Alpha channel of the texture.
 * The dithering is performed by the blender unit, which is also in charge of
 * adapting the pixel color depth to that of the framebuffer. Dithering is
 * a good way to reduce the mach banding effect created by color depth
 * reduction.
 * 
 * The blender in fact will reduce the RGB components of the pixel (coming
 * from the color combiner) to 5-bit when the framebuffer is 16-bit. If the
 * framebuffer is 32-bit, the blender formula will be calculated with 8-bit
 * per channel, so no dithering is required.
 * 
 * On the other hand, the alpha channels (used as multiplicative factors
 * in the blender formulas) will always be reduced to 5-bit depth, even if
 * the framebuffer is 32-bit. If you see banding artifacts in transparency levels
 * of blended polygons, you may want to activate dithering on the alpha channel.
 * 
 * It is important to notice that the VI can optionally run an "dither filter"
 * on the final image, while sending it to the video output. This
 * algorithm tries to recover color depth precision by averaging lower bits
 * in neighborhood pixels, and reducing the small noise created by dithering.
 * #display_init currently activates it by default on all 16-bit display modes,
 * if passed #FILTERS_DEDITHER or #FILTERS_RESAMPLE_ANTIALIAS_DEDITHER.
 * 
 * If you are using an emulator, make sure it correctly emulates the VI
 * dither filter to judge the quality of the final image. For instance,
 * the RDP plugin parallel-RDP (based on Vulkan) emulates it very accurately,
 * so emulators like Ares, dgb-n64 or simple64 will produce a picture closer to
 * real hardware.
 * 
 * The supported dither algorithms are:
 * 
 *   * `SQUARE` (aka "magic square"). This is a custom dithering
 *     algorithm, designed to work best with the VI dither filter. When
 *     using it, the VI will reconstruct a virtually perfect 32-bit image
 *     even though the framebuffer is only 16-bit.
 *   * `BAYER`: standard Bayer dithering. This algorithm looks
 *     better than the magic square when the VI dither filter is disabled,
 *     or in some specific scenarios like large blended polygons. Make
 *     sure to test it as well.
 *   * `INVSQUARE` and `INVBAYER`: these are the same algorithms, but using
 *     an inverse (symmetrical) pattern. They can be selected for alpha
 *     channels to avoid making transparency phase with color dithering,
 *     which is sometimes awkward.
 *   * `NOISE`: random noise dithering. The dithering is performed
 *     by perturbing the lower bit of each pixel with random noise.
 *     This will create a specific visual effect as it changes from frame to
 *     frame even on still images; it is especially apparent when used on
 *     alpha channel as it can affect transparency. It is more commonly used
 *     as a graphic effect rather than an actual dithering.
 *   * `NONE`: disable dithering.
 * 
 * While the RDP hardware allows to configure different dither algorithms
 * for RGB and Alpha channels, unfortunately not all combinations are
 * available. This enumerator defines the available combinations. For
 * instance, #DITHER_BAYER_NOISE selects the Bayer dithering for the
 * RGB channels, and the noise dithering for alpha channel.
 */

typedef enum rdpq_dither_s {
    DITHER_SQUARE_SQUARE       = (SOM_RGBDITHER_SQUARE | SOM_ALPHADITHER_SAME)   >> SOM_ALPHADITHER_SHIFT, ///< Dithering: RGB=Square, Alpha=Square
    DITHER_SQUARE_INVSQUARE    = (SOM_RGBDITHER_SQUARE | SOM_ALPHADITHER_INVERT) >> SOM_ALPHADITHER_SHIFT, ///< Dithering: RGB=Square, Alpha=InvSquare
    DITHER_SQUARE_NOISE        = (SOM_RGBDITHER_SQUARE | SOM_ALPHADITHER_NOISE)  >> SOM_ALPHADITHER_SHIFT, ///< Dithering: RGB=Square, Alpha=Noise
    DITHER_SQUARE_NONE         = (SOM_RGBDITHER_SQUARE | SOM_ALPHADITHER_NONE)   >> SOM_ALPHADITHER_SHIFT, ///< Dithering: RGB=Square, Alpha=None

    DITHER_BAYER_BAYER         = (SOM_RGBDITHER_BAYER  | SOM_ALPHADITHER_SAME)   >> SOM_ALPHADITHER_SHIFT, ///< Dithering: RGB=Bayer, Alpha=Bayer
    DITHER_BAYER_INVBAYER      = (SOM_RGBDITHER_BAYER  | SOM_ALPHADITHER_INVERT) >> SOM_ALPHADITHER_SHIFT, ///< Dithering: RGB=Bayer, Alpha=InvBayer
    DITHER_BAYER_NOISE         = (SOM_RGBDITHER_BAYER  | SOM_ALPHADITHER_NOISE)  >> SOM_ALPHADITHER_SHIFT, ///< Dithering: RGB=Bayer, Alpha=Noise
    DITHER_BAYER_NONE          = (SOM_RGBDITHER_BAYER  | SOM_ALPHADITHER_NONE)   >> SOM_ALPHADITHER_SHIFT, ///< Dithering: RGB=Bayer, Alpha=None

    DITHER_NOISE_SQUARE        = (SOM_RGBDITHER_NOISE  | SOM_ALPHADITHER_SAME)   >> SOM_ALPHADITHER_SHIFT, ///< Dithering: RGB=Noise, Alpha=Square
    DITHER_NOISE_INVSQUARE     = (SOM_RGBDITHER_NOISE  | SOM_ALPHADITHER_INVERT) >> SOM_ALPHADITHER_SHIFT, ///< Dithering: RGB=Noise, Alpha=InvSquare
    DITHER_NOISE_NOISE         = (SOM_RGBDITHER_NOISE  | SOM_ALPHADITHER_NOISE)  >> SOM_ALPHADITHER_SHIFT, ///< Dithering: RGB=Noise, Alpha=Noise
    DITHER_NOISE_NONE          = (SOM_RGBDITHER_NOISE  | SOM_ALPHADITHER_NONE)   >> SOM_ALPHADITHER_SHIFT, ///< Dithering: RGB=Noise, Alpha=None

    DITHER_NONE_BAYER          = (SOM_RGBDITHER_NONE   | SOM_ALPHADITHER_SAME)   >> SOM_ALPHADITHER_SHIFT, ///< Dithering: RGB=None, Alpha=Bayer
    DITHER_NONE_INVBAYER       = (SOM_RGBDITHER_NONE   | SOM_ALPHADITHER_INVERT) >> SOM_ALPHADITHER_SHIFT, ///< Dithering: RGB=None, Alpha=InvBayer
    DITHER_NONE_NOISE          = (SOM_RGBDITHER_NONE   | SOM_ALPHADITHER_NOISE)  >> SOM_ALPHADITHER_SHIFT, ///< Dithering: RGB=None, Alpha=Noise
    DITHER_NONE_NONE           = (SOM_RGBDITHER_NONE   | SOM_ALPHADITHER_NONE)   >> SOM_ALPHADITHER_SHIFT, ///< Dithering: RGB=None, Alpha=None
} rdpq_dither_t;

/**
 * @brief Types of palettes supported by RDP
 */
typedef enum rdpq_tlut_s {
    TLUT_NONE   = 0,     ///< No palette
    TLUT_RGBA16 = 2,     ///< Palette made of #FMT_RGBA16 colors
    TLUT_IA16   = 3,     ///< Palette made of #FMT_IA16 colors
} rdpq_tlut_t;

/**
 * @brief Converts the specified texture format to the TLUT mode that is needed to draw a texture of this format
 */
inline rdpq_tlut_t rdpq_tlut_from_format(tex_format_t format) {
    switch (format) {
    case FMT_CI4:
    case FMT_CI8:
        return TLUT_RGBA16;
    default:
        return TLUT_NONE;
    }
}

/**
 * @brief Types of mipmap supported by RDP
 */
typedef enum rdpq_mipmap_s {
    MIPMAP_NONE        = 0,                                                ///< Mipmap disabled
    MIPMAP_NEAREST     = SOM_TEXTURE_LOD >> 32,                            ///< Choose the nearest mipmap level
    MIPMAP_INTERPOLATE = (SOM_TEXTURE_LOD | SOMX_LOD_INTERPOLATE) >> 32,   ///< Interpolate between the two nearest mipmap levels (also known as "trilinear")
    MIPMAP_INTERPOLATE_SHARPEN = (SOM_TEXTURE_LOD | SOMX_LOD_INTERPOLATE | SOM_TEXTURE_SHARPEN) >> 32,   ///< Interpolate between the two nearest mipmap levels (also known as "trilinear") with sharpening enabled
    MIPMAP_INTERPOLATE_DETAIL = (SOM_TEXTURE_LOD | SOMX_LOD_INTERPOLATE | SOM_TEXTURE_DETAIL) >> 32,   ///< Interpolate between the two nearest mipmap levels (also known as "trilinear") with detail texture enabled
} rdpq_mipmap_t;

/**
 * @brief Types of antialiasing supported by RDP
 */
typedef enum rdpq_antialias_s {
    AA_NONE = 0,            ///< No antialiasing
    AA_STANDARD = 1,        ///< Standard antialiasing
    AA_REDUCED = 2,         ///< Reduced antialiasing
} rdpq_antialias_t;


/**
 * @name Render modes
 * 
 * These functions set a new render mode from scratch. Every render state is 
 * reset to some value (or default), so no previous state is kept valid.
 * 
 * @{
 */

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
    extern void __rdpq_set_mode_fill(void);
    __rdpq_set_mode_fill();
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
 * @note The COPY mode only works with 16-bpp framebuffers. It will trigger a
 *       hardware crash (!) on 32-bpp framebuffers, so avoid using it. The
 *       validator will warn you about this anyway.
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

/** @} */

/**
 * @name Render states
 * 
 * These functions allow to tweak individual render states. They should be called
 * after one of the render mode reset functions to configure the render states.
 * 
 * @{
 */


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
 * the second type, make sure that you did pass #FILTERS_RESAMPLE_ANTIALIAS or
 * #FILTERS_RESAMPLE_ANTIALIAS_DEDITHER to #display_init.
 * 
 * On the other hand, if you want to make sure that no antialias is performed,
 * disable antialias with `rdpq_mode_antialias(false)` (which is the default
 * for #rdpq_set_mode_standard), and that will make sure that the VI will not
 * do anything to the image, even if #display_init was called with
 * #FILTERS_RESAMPLE_ANTIALIAS or #FILTERS_RESAMPLE_ANTIALIAS_DEDITHER.
 *
 * @note Antialiasing internally uses the blender unit. If you already
 *       configured a formula via #rdpq_mode_blender, antialias will just
 *       rely on that one to correctly blend pixels with the framebuffer. It is
 *       thus important that a custom formula configured via #rdpq_mode_blender
 *       does blend with the background somehow.
 * 
 * @param mode        Antialiasing mode to use (or AA_NONE to disable)
 */
inline void rdpq_mode_antialias(rdpq_antialias_t mode) 
{
    // Just enable/disable SOM_AA_ENABLE. The RSP will then update the render mode
    // which would trigger different other bits in SOM depending on the current mode.
    __rdpq_mode_change_som(SOM_AA_ENABLE | SOMX_AA_REDUCED, 
        (mode ? SOM_AA_ENABLE : 0) | (mode == AA_REDUCED ? SOMX_AA_REDUCED : 0));
}

/**
 * @brief Configure the color combiner
 * 
 * This function allows to configure the color combiner formula to be used.
 * The color combiner is the internal RDP hardware unit that mixes inputs
 * from textures, colors and other sources and produces a RGB/Alpha value,
 * that is then sent to the blender unit. If the blender is disabled (eg:
 * the polygon is solid), the value produced by the combiner is the one
 * that will be written into the framebuffer.
 * 
 * For common use cases, rdpq offers ready-to-use macros that you can pass
 * to #rdpq_mode_combiner: #RDPQ_COMBINER_FLAT, #RDPQ_COMBINER_SHADE,
 * #RDPQ_COMBINER_TEX, #RDPQ_COMBINER_TEX_FLAT, #RDPQ_COMBINER_TEX_SHADE.
 * 
 * For example, to draw a texture rectangle modulated with a flat color:
 * 
 * @code{.c}
 *      // Reset to standard rendering mode.
 *      rdpq_set_mode_standard();
 * 
 *      // Configure the combiner
 *      rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
 * 
 *      // Configure the flat color that will modulate the texture
 *      rdpq_set_prim_color(RGBA32(192, 168, 74, 255));
 *
 *      // Upload a texture into TMEM (tile descriptor #4)
 *      rdpq_tex_upload(TILE4, &texture, 0);
 * 
 *      // Draw the rectangle
 *      rdpq_texture_rectangle(TILE4,
 *          0, 0, 32, 16,     // x0, y0, x1, y1
 *          0, 0, 1.0, 1.0f   // s, t, ds, dt
 *      );
 * @endcode
 * 
 * Alternatively, you can use your own combiner formulas, created with either
 * #RDPQ_COMBINER1 (one pass) or #RDPQ_COMBINER2 (two passes). See the respective
 * documentation for all the details on how to create a custom formula.
 * 
 * When using a custom formula, you must take into account that some render states
 * also rely on the combiner to work. Specifically:
 * 
 *  * Mipmap (#rdpq_mode_mipmap): when activating interpolated mipmapping
 *    (#MIPMAP_INTERPOLATE, also known as "trilinear filterig"), a dedicated
 *    color combiner pass is needed, so if you set a custom formula, it has to be
 *    a one-pass formula. Otherwise, a RSP assertion will trigger.
 *  * Fog (#rdpq_mode_fog): fogging is generally made by substituting the alpha
 *    component of the shade color with a depth value, which is then used in
 *    the blender formula (eg: #RDPQ_FOG_STANDARD). The only interaction with the
 *    color combiner is that the SHADE alpha component should not be used as
 *    a modulation factor in the combiner, otherwise you get wrong results
 *    (if you then use the alpha for blending). rdpq automatically adjusts
 *    standard combiners using shade (#RDPQ_COMBINER_SHADE and #RDPQ_COMBINER_TEX_SHADE)
 *    when fog is enabled, but for custom combiners it is up to the user to
 *    take care of that.
 * 
 * @param comb      The combiner formula to configure
 * 
 * @see #RDPQ_COMBINER1
 * @see #RDPQ_COMBINER2
 * 
 * @note For programmers with previous RDP programming experience: this function
 *       makes sure that the current cycle type can work correctly with the
 *       specified combiner formula. Specifically, it switches automatically
 *       between 1-cycle and 2-cycle depending on the formula being set and the
 *       blender unit configuration, and also automatically adapts combiner
 *       formulas to the required cycle mode. See the documentation in rdpq.c
 *       for more information.
 */
inline void rdpq_mode_combiner(rdpq_combiner_t comb) {
    extern void __rdpq_fixup_mode(uint32_t cmd_id, uint32_t w0, uint32_t w1);
    extern void __rdpq_fixup_mode4(uint32_t cmd_id, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3);

    if (comb & RDPQ_COMBINER_2PASS)
        __rdpq_fixup_mode(RDPQ_CMD_SET_COMBINE_MODE_2PASS,
            (comb >> 32) & 0x00FFFFFF,
            comb & 0xFFFFFFFF);
    else {
        rdpq_combiner_t comb1_mask = RDPQ_COMB1_MASK;
        if (((comb >> 0 ) &  7) == 1) comb1_mask ^= 1ull << 0;
        if (((comb >> 3 ) &  7) == 1) comb1_mask ^= 1ull << 3;
        if (((comb >> 6 ) &  7) == 1) comb1_mask ^= 1ull << 6;
        if (((comb >> 18) &  7) == 1) comb1_mask ^= 1ull << 18;
        if (((comb >> 21) &  7) == 1) comb1_mask ^= 1ull << 21;
        if (((comb >> 24) &  7) == 1) comb1_mask ^= 1ull << 24;
        if (((comb >> 32) & 31) == 1) comb1_mask ^= 1ull << 32;
        if (((comb >> 37) & 15) == 1) comb1_mask ^= 1ull << 37;

        __rdpq_fixup_mode4(RDPQ_CMD_SET_COMBINE_MODE_1PASS,
            (comb >> 32) & 0x00FFFFFF,
            comb & 0xFFFFFFFF,
            (comb1_mask >> 32) & 0x00FFFFFF,
            comb1_mask & 0xFFFFFFFF);
    }
}

/** @brief Blending mode: multiplicative alpha.
 * 
 * This is standard multiplicative blending between the color being
 * drawn and the framebuffer color.
 * 
 * You can pass this macro to #rdpq_mode_blender.
 */
#define RDPQ_BLENDER_MULTIPLY       RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, INV_MUX_ALPHA))

/** @brief Blending mode: multiplicative alpha with a constant value.
 * 
 * This is similar to #RDPQ_BLENDER_MULTIPLY, but instead of using the alpha
 * value from the texture (or rather, the one coming out of the color combiner),
 * it uses a constant value that must be programmed via #rdpq_set_fog_color:
 * 
 * You can pass this macro to #rdpq_mode_blender:
 * 
 * @code{.c}
 *    float alpha = 0.5f;
 *    rdpq_set_fog_color(RGBA32(0, 0, 0, alpha * 255));
 *    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY_CONST);
 * @endcode
 * 
 * Notice that the alpha value coming out of the combiner is ignored. This
 * means that you can use this blender formula even for blending textures without
 * alpha channel.
 */
#define RDPQ_BLENDER_MULTIPLY_CONST     RDPQ_BLENDER((IN_RGB, FOG_ALPHA, MEMORY_RGB, INV_MUX_ALPHA))

/** @brief Blending mode: additive alpha.
 * You can pass this macro to #rdpq_mode_blender.
 * 
 * NOTE: additive blending is broken on RDP because it can overflow. Basically,
 *       if the result of the sum is larger than 1.5 (in scale 0..1), instead
 *       of being clamped to 1, it overflows back to 0, which makes the
 *       mode almost useless. It is defined it for completeness.
 */
#define RDPQ_BLENDER_ADDITIVE       RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, ONE))      

/**
 * @brief Configure the formula to use for blending.
 * 
 * This function can be used to configure the formula used
 * in the blender unit.
 * 
 * The standard blending formulas are:
 * 
 *  * #RDPQ_BLENDER_MULTIPLY: multiplicative alpha blending
 *  * #RDPQ_BLENDER_ADDITIVE: additive alpha blending
 * 
 * It is possible to also create custom formulas. The blender unit
 * allows for up to two passes. Use #RDPQ_BLENDER to create a one-pass
 * blending formula, or #RDPQ_BLENDER2 to create a two-pass formula.
 * 
 * Please notice that two-pass formulas are not compatible with fogging
 * (#rdpq_mode_fog). Also notice that rdpq_mode assumes that any formula
 * that you set here (either one-pass or two-passes) does blend with the
 * background. If you want to use a formula that does not blend with the
 * background, set it via #rdpq_mode_fog, otherwise you might get incorrect
 * results when using anti-alias (see #rdpq_mode_antialias).
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
 *      // Notice that the FOG register is not necessarily about fogging... it is
 *      // just one of the two registers that can be used in blending formulas.
 *      rdpq_mode_blender(RDPQ_BLENDER(IN_RGB, FOG_ALPHA, MEMORY_RGB, INV_MUX_ALPHA));
 * 
 *      // Configure the FOG_ALPHA value to 128 (= 0.5). The RGB components are
 *      // not used.
 *      rdpq_set_fog_color(RGBA32(0,0,0, 128));
 * 
 *      // Load a texture into TMEM
 *      rdpq_tex_upload(TILE0, texture, 0);
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
 * @see #RDPQ_BLENDER_MULTIPLY
 * @see #RDPQ_BLENDER_ADDITIVE
 */
inline void rdpq_mode_blender(rdpq_blender_t blend) {
    extern void __rdpq_fixup_mode(uint32_t cmd_id, uint32_t w0, uint32_t w1);
    if (blend) blend |= SOM_BLENDING;
    __rdpq_fixup_mode(RDPQ_CMD_SET_BLENDING_MODE, 0, blend);
}

/** @brief Fogging mode: standard.
 * You can pass this macro to #rdpq_mode_fog. */
#define RDPQ_FOG_STANDARD         RDPQ_BLENDER((IN_RGB, SHADE_ALPHA, FOG_RGB, INV_MUX_ALPHA))

/**
 * @brief Enable or disable fog 
 * 
 * This function enables fog on RDP. Fog on RDP is simulated in the
 * following way:
 *   
 *  * The T&L pipeline must calculate a depth information for each
 *    vertex of the primitive and put it into the alpha channel of
 *    the per-vertex color. This is outside of the scope of rdpq,
 *    so rdpq assumes that this has already been done when
 *    #rdpq_mode_fog is called.
 *  * The RDP blender unit is programmed to modulate a "fog color"
 *    with the polygon pixel, using SHADE_ALPHA as interpolation
 *    factor. Since SHADE_ALPHA contains a depth information, the
 *    farther the object, the stronger it will assume the fog color.
 * 
 * To enable fog, pass #RDPQ_FOG_STANDARD to this function, and
 * call #rdpq_set_fog_color to configure the fog color. This is
 * the standard fogging formula. 
 * 
 * If you want, you can instead build a custom fogging formula
 * using #RDPQ_BLENDER. Notice that rdpq_mode assumes that the formula
 * that you set with rdpq_mode_fog does not blend with the background; for
 * that, use #rdpq_mode_blender.
 * 
 * To disable fog, call #rdpq_mode_fog passing 0.
 * 
 * @note Fogging uses one pass of the blender unit (the first),
 *       so this can coexist with a blending formula (#rdpq_mode_blender)
 *       as long as it's a single pass one (created via #RDPQ_BLENDER).
 *       If a two-pass blending formula (#RDPQ_BLENDER2) was set with
 *       #rdpq_mode_blender, fogging cannot be used.
 * 
 * @param fog            Fog formula created with #RDPQ_BLENDER,
 *                       or 0 to disable.
 * 
 * @see #RDPQ_FOG_STANDARD
 * @see #rdpq_set_fog_color
 * @see #RDPQ_BLENDER
 * @see #rdpq_mode_blender
 */
inline void rdpq_mode_fog(rdpq_blender_t fog) {
    extern void __rdpq_fixup_mode(uint32_t cmd_id, uint32_t w0, uint32_t w1);
    if (fog) fog |= SOM_BLENDING;
    if (fog) assertf((fog & SOMX_BLEND_2PASS) == 0, "Fogging cannot be used with two-pass blending formulas");
    __rdpq_mode_change_som(SOMX_FOG, fog ? SOMX_FOG : 0);
    __rdpq_fixup_mode(RDPQ_CMD_SET_FOG_MODE, 0, fog);
}

/**
 * @brief Change dithering mode
 * 
 * This function allows to change the dithering algorithm performed by 
 * RDP on RGB and alpha channels. Note that by default, #rdpq_set_mode_standard
 * disables any dithering.
 * 
 * See #rdpq_dither_t for an explanation of how RDP applies dithering and 
 * how the different dithering algorithms work.
 * 
 * @param dither    Dithering to perform
 * 
 * @see #rdpq_dither_t
 */
inline void rdpq_mode_dithering(rdpq_dither_t dither) {
    __rdpq_mode_change_som(
        SOM_RGBDITHER_MASK | SOM_ALPHADITHER_MASK, ((uint64_t)dither << SOM_ALPHADITHER_SHIFT));
}

/**
 * @brief Activate alpha compare feature
 * 
 * This function activates the alpha compare feature. It allows to do per-pixel
 * rejection (masking) depending on the value of the alpha component of the pixel.
 * The value output from the combiner is compared with a configured threshold
 * and if the value is lower, the pixel is not written to the framebuffer.
 * 
 * Moreover, RDP also support a random noise alpha compare mode, where the threshold
 * value is calculated as a random number for each pixel. This can be used for special
 * graphic effects.
 * 
 * @note Alpha compare becomes more limited if antialiasing is enabled (both full and reduced,
 *       see #rdpq_mode_antialias). In that case, any threshold value not equal to 0 will
 *       internally be treated as if 255 was specified. This implies that noise-based
 *       alpha compare is not supported under this condition.
 * 
 * @param threshold          Threshold value. All pixels whose alpha is less than this threshold
 *                           will not be drawn. Use 0 to disable. Use a negative value for
 *                           activating the noise-based alpha compare.
 */
inline void rdpq_mode_alphacompare(int threshold) {
    if (threshold == 0) {
        __rdpq_mode_change_som(SOM_ALPHACOMPARE_MASK, 0);
    } else if (threshold > 0) {
        __rdpq_mode_change_som(SOM_ALPHACOMPARE_MASK, SOM_ALPHACOMPARE_THRESHOLD);
        rdpq_set_blend_color(RGBA32(0,0,0,(uint8_t)threshold));
    } else {
        __rdpq_mode_change_som(SOM_ALPHACOMPARE_MASK, SOM_ALPHACOMPARE_NOISE);
    }
}

/**
 * @brief Activate z-buffer usage
 * 
 * Activate usage of Z-buffer. The Z-buffer surface must be configured
 * via #rdpq_set_z_image.
 * 
 * It is possible to separately activate the depth comparison
 * (*reading* from the Z-buffer) and the Z update (*writing* to
 * the Z-buffer).
 * 
 * @param compare     True if per-pixel depth test must be performed
 * @param update      True if per-pixel depth write must be performed
 * 
 * @see #rdpq_set_z_image
 */
inline void rdpq_mode_zbuf(bool compare, bool update) {
    __rdpq_mode_change_som(
        SOM_Z_COMPARE | SOM_Z_WRITE, 
        (compare ? SOM_Z_COMPARE : 0) |
        (update  ? SOM_Z_WRITE   : 0)
    );
}

/**
 * @brief Set a fixed override of Z value
 * 
 * This function activates a special mode in which RDP will use a fixed value
 * of Z for the next drawn primitives. This works with both rectangles
 * (#rdpq_fill_rectangle and #rdpq_texture_rectangle) and triangles
 * (#rdpq_triangle). 
 * 
 * If a triangle is drawn with per-vertex Z while the Z-override is active,
 * the per-vertex Z will be ignored.
 * 
 * @param enable    Enable/disable the Z-override mode
 * @param z         Z value to use (range 0..1)
 * @param deltaz    DeltaZ value to use.
 * 
 * @see #rdpq_set_prim_depth_raw
 */
inline void rdpq_mode_zoverride(bool enable, float z, int16_t deltaz) {
    if (enable) rdpq_set_prim_depth_raw(z * 0x7FFF, deltaz);
    __rdpq_mode_change_som(
        SOM_ZSOURCE_PRIM, enable ? SOM_ZSOURCE_PRIM : 0
    );
}


/**
 * @brief Activate palette lookup during drawing
 * 
 * This function allows to enable / disable palette lookup during
 * drawing. To draw using a texture with palette, it is necessary
 * to first load the texture into TMEM (eg: via #rdpq_tex_upload), 
 * then load the palette (eg: via #rdpq_tex_upload_tlut),
 * and finally activate the palette drawing mode via #rdpq_mode_tlut.
 * 
 * @param tlut     Palette type, or 0 to disable.
 * 
 * @see #rdpq_tex_upload
 * @see #rdpq_tex_upload_tlut
 * @see #rdpq_tlut_t
 */
inline void rdpq_mode_tlut(rdpq_tlut_t tlut) {
    // This assert is useful to catch the common mistake of rdpq_mode_tlut(true)
    assertf(tlut == TLUT_NONE || tlut == TLUT_RGBA16 || tlut == TLUT_IA16, "invalid TLUT type");
    __rdpq_mode_change_som(SOM_TLUT_MASK, (uint64_t)tlut << SOM_TLUT_SHIFT);
}

/**
 * @brief Activate texture filtering
 *
 * This function allows to configure the kind of texture filtering that will be used
 * while sampling textures.
 * 
 * Available in render modes: standard, copy.
 * 
 * @param filt      Texture filtering type
 * 
 * @see #rdpq_filter_t
 */
inline void rdpq_mode_filter(rdpq_filter_t filt) {
    __rdpq_mode_change_som(SOM_SAMPLE_MASK, (uint64_t)filt << SOM_SAMPLE_SHIFT);
}

/**
 * @brief Activate mip-mapping.
 * 
 * This function can be used to turn on mip-mapping.
 * 
 * TMEM must have been loaded with multiple level of details (LOds) of the texture
 * (a task for which rdpq is currently missing a helper, so it has to be done manually).
 * Also, multiple consecutive tile descriptors (one for each LOD) must have been configured.
 * 
 * If you call #rdpq_triangle when mipmap is active via #rdpq_mode_mipmap, pass 0
 * to the number of mipmaps in #rdpq_trifmt_t, as the number of levels set here
 * will win over it.
 * 
 * @param mode          Mipmapping mode (use #MIPMAP_NONE to disable)
 * @param num_levels    Number of mipmap levels to use. Pass 0 when setting MIPMAP_NONE.
 */
inline void rdpq_mode_mipmap(rdpq_mipmap_t mode, int num_levels) {
    if (mode == MIPMAP_NONE)
        num_levels = 0;
    if (num_levels)
        num_levels -= 1;
    __rdpq_mode_change_som(SOM_TEXTURE_LOD | SOMX_LOD_INTERPOLATE | SOMX_NUMLODS_MASK | SOM_TEXTURE_SHARPEN | SOM_TEXTURE_DETAIL, 
        ((uint64_t)mode << 32) | ((uint64_t)num_levels << SOMX_NUMLODS_SHIFT));
};

/**
 * @brief Activate perspective correction for textures
 * 
 * This function enables or disables the perspective correction for texturing.
 * Perspective correction does not slow down rendering, and thus it is basically
 * free.
 * 
 * To be able to use perspective correction, make sure to pass the Z and W values 
 * in the triangle vertices.
 * 
 * @param perspective       True to activate perspective correction, false to disable it.
 */
inline void rdpq_mode_persp(bool perspective)
{
    __rdpq_mode_change_som(SOM_TEXTURE_PERSP, perspective ? SOM_TEXTURE_PERSP : 0);
}

/** @} */

/**
 * @brief Start a batch of RDP mode changes
 * 
 * This function can be used as an optimization when changing render mode
 * and/or multiple render states. It allows to batch the changes, so that
 * RDP hardware registers are updated only once.
 * 
 * To use it, put a call to #rdpq_mode_begin and #rdpq_mode_end around
 * the mode functions that you would like to batch. For instance:
 * 
 * @code{.c}
 *      rdpq_mode_begin();
 *          rdpq_set_mode_standard();
 *          rdpq_mode_mipmap(MIPMAP_INTERPOLATE, 2);
 *          rdpq_mode_dithering(DITHER_SQUARE_SQUARE);
 *          rdpq_mode_blender(RDPQ_BLENDING_MULTIPLY);
 *      rdpq_mode_end();
 * @endcode
 * 
 * The only effect of using #rdpq_mode_begin is more efficient RSP
 * and RDP usage, there is no semantic change in the way RDP is
 * programmed when #rdpq_mode_end is called.
 * 
 * @note The functions affected by #rdpq_mode_begin / #rdpq_mode_end
 *       are just those that are part of the mode API (that is,
 *       `rdpq_set_mode_*` and `rdpq_mode_*`). Any other function
 *       is not batched and will be issued immediately.
 */
void rdpq_mode_begin(void);

/**
 * @brief Finish a batch of RDP mode changes
 * 
 * This function completes a batch of changes started with #rdpq_mode_begin.
 * 
 * @see #rdpq_mode_begin
 */
void rdpq_mode_end(void);

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
