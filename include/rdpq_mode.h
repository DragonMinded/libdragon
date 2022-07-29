#ifndef LIBDRAGON_RDPQ_MODE_H
#define LIBDRAGON_RDPQ_MODE_H

#include "rdpq.h"
#include <stdint.h>

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
    SAMPLER_POINT = 0,
    SAMPLER_BILINEAR,
    SAMPLER_MEDIAN
} rdpq_sampler_t;

typedef enum rdpq_dither_s {
    DITHER_SQUARE = 0,
    DITHER_BAYER,
    DITHER_NOISE,
    DITHER_NONE
} rdpq_dither_t;

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
 * @param[in]  transparency   If true, pixels with alpha set to 0 are not drawn
 */
inline void rdpq_set_mode_copy(bool transparency) {
    if (transparency) rdpq_set_blend_color(RGBA32(0,0,0,1));
    rdpq_set_other_modes_raw(SOM_CYCLE_COPY | (transparency ? SOM_ALPHA_COMPARE : 0));
}

inline void rdpq_set_mode_standard(void) {
    // FIXME: accept structure?
    // FIXME: reset combiner?
    rdpq_set_other_modes_raw(SOM_CYCLE_1 | SOM_TC_FILTER | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE);
}

inline void rdpq_mode_combiner(rdpq_combiner_t comb) {
    extern void __rdpq_fixup_mode(uint32_t cmd_id, uint32_t w0, uint32_t w1);

    // FIXME: autosync pipe
    if (comb & RDPQ_COMBINER_2PASS)
        __rdpq_fixup_mode(RDPQ_CMD_SET_COMBINE_MODE_2PASS,
            (comb >> 32) & 0x00FFFFFF,
            comb & 0xFFFFFFFF);
    else
        __rdpq_fixup_mode(RDPQ_CMD_SET_COMBINE_MODE_1PASS,
            (comb >> 32) & 0x00FFFFFF,
            comb & 0xFFFFFFFF);
}

inline void rdpq_mode_blender(rdpq_blender_t blend) {
    extern void __rdpq_fixup_mode(uint32_t cmd_id, uint32_t w0, uint32_t w1);

    // NOTE: basically everything this function does will be constant-propagated
    // when the function is called with a compile-time constant argument, which
    // should be the vast majority of times.

    // RDPQ_CMD_SET_BLENDING_MODE accepts two blender configurations: the one
    // to use in 1cycle mode, and the one to use in 2cycle mode. MAKE_SBM_ARG
    // encodes the two configurations into a 64-bit word to be used with the command.
    #define MAKE_SBM_ARG(blend_1cyc, blend_2cyc) \
        ((((uint64_t)(blend_1cyc) >> 6) & 0x3FFFFFF) | \
         (((uint64_t)(blend_2cyc) >> 6) & 0x3FFFFFF) << 26)

    rdpq_blender_t blend_1cyc, blend_2cyc;
    if (blend & RDPQ_BLENDER_2PASS) {
        // A 2-pass blender will force 2cycle mode, so we don't care about the
        // configuration for 1cycle mode. Let's just use 0 for it, it will not
        // be used anyway.
        blend_1cyc = 0;
        blend_2cyc = blend;
    } else {
        // A single pass blender can be used as-is in 1cycle mode (the macros
        // in rdp_commands have internally configured the same settings in both
        // passes, as this is what RDP expects).
        // For 2-cycle mode, instead, it needs to be changed: the configuration
        // is valid for the second pass, but the first pass needs to changed
        // with a passthrough (IN * 0 + IN * 1). Notice that we can't do
        // the passthrough in the second pass because of the way the 2pass
        // blender formula works.
        const rdpq_blender_t passthrough = RDPQ_BLENDER1((IN_RGB, ZERO, IN_RGB, ONE));
        blend_1cyc = blend;
        blend_2cyc = (passthrough & SOM_BLEND0_MASK) | 
                     (blend       & SOM_BLEND1_MASK);
    }

    // FIXME: autosync pipe
    uint64_t cfg = MAKE_SBM_ARG(blend_1cyc, blend_2cyc);
    __rdpq_fixup_mode(RDPQ_CMD_SET_BLENDING_MODE,
        (cfg >> 32) & 0x00FFFFFF,
        cfg & 0xFFFFFFFF);
}

inline void rdpq_mode_blender_off(void) {
    extern void __rdpq_fixup_mode(uint32_t cmd_id, uint32_t w0, uint32_t w1);
    __rdpq_fixup_mode(RDPQ_CMD_SET_BLENDING_MODE, 0, 0);
}

inline void rdpq_mode_dithering(rdpq_dither_t rgb, rdpq_dither_t alpha) {
    rdpq_change_other_modes_raw(
        SOM_RGBDITHER_MASK | SOM_ALPHADITHER_MASK,
        ((uint64_t)rgb << SOM_RGBDITHER_SHIFT) | ((uint64_t)alpha << SOM_ALPHADITHER_SHIFT));
}

inline void rdpq_mode_alphacompare(bool enable, int threshold) {
    if (enable && threshold > 0) rdpq_set_blend_color(RGBA32(0,0,0,threshold));
    rdpq_change_other_modes_raw(
        SOM_ALPHACOMPARE_MASK, enable ? SOM_ALPHA_COMPARE : 0
    );
}

inline void rdpq_mode_zoverride(bool enable, uint16_t z, int16_t deltaz) {
    if (enable) rdpq_set_prim_depth(z, deltaz);
    rdpq_change_other_modes_raw(
        SOM_Z_SOURCE_PRIM, enable ? SOM_Z_SOURCE_PRIM : 0
    );
}

inline void rdpq_mode_sampler(rdpq_sampler_t s) {
    uint64_t samp;
    switch (s) {
        case SAMPLER_POINT:    samp = SOM_SAMPLE_1X1; break;
        case SAMPLER_MEDIAN:   samp = SOM_SAMPLE_2X2 | SOM_SAMPLE_MIDTEXEL; break;
        case SAMPLER_BILINEAR: samp = SOM_SAMPLE_2X2; break;
    }
    rdpq_change_other_modes_raw(SOM_SAMPLE_MASK, samp);    
}

#endif
