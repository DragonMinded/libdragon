/**
 * @file rdpq_mode.h
 * @brief RDP Command queue: mode setting
 * @ingroup rdp
 */
#ifndef LIBDRAGON_RDPQ_MODE_H
#define LIBDRAGON_RDPQ_MODE_H

#include "rdpq.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

typedef enum rdpq_tlut_s {
    TLUT_NONE   = 0,
    TLUT_RGBA16 = 2,
    TLUT_IA16   = 3,
} rdpq_tlut_t;

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

void rdpq_set_mode_standard(void);

/**
 * @brief Reset render mode to YUV mode.
 * 
 * This is a helper function to configure a render mode for YUV conversion.
 * In addition of setting the render mode, this funciton also configures a
 * combiner (given that YUV conversion happens also at the combiner level),
 * and set standard YUV parameters (for BT.601 TV Range).
 * 
 * After setting the YUV mode, you can load YUV textures to TMEM (using a
 * surface with #FMT_YUV16), and then draw them on the screen as part of
 * triangles or rectangles.
 */
void rdpq_set_mode_yuv(void);

inline void rdpq_mode_antialias(bool enable) 
{
    // TODO
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

#define RDPQ_BLEND_MULTIPLY       RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, INV_MUX_ALPHA))
#define RDPQ_BLEND_ADDITIVE       RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, ONE))      
#define RDPQ_FOG_STANDARD         RDPQ_BLENDER((IN_RGB, SHADE_ALPHA, FOG_RGB, INV_MUX_ALPHA))

inline void rdpq_mode_blending(rdpq_blender_t blend) {
    extern void __rdpq_fixup_mode(uint32_t cmd_id, uint32_t w0, uint32_t w1);
    if (blend) blend |= SOM_BLENDING;
    __rdpq_fixup_mode(RDPQ_CMD_SET_BLENDING_MODE, 4, blend);
}

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
        SOM_ALPHACOMPARE_MASK, enable ? SOM_ALPHA_COMPARE : 0
    );
}

inline void rdpq_mode_zoverride(bool enable, uint16_t z, int16_t deltaz) {
    if (enable) rdpq_set_prim_depth(z, deltaz);
    rdpq_change_other_modes_raw(
        SOM_Z_SOURCE_PRIM, enable ? SOM_Z_SOURCE_PRIM : 0
    );
}

inline void rdpq_mode_tlut(rdpq_tlut_t tlut) {
    rdpq_change_other_modes_raw(SOM_TLUT_MASK, (uint64_t)tlut << 46);
}

inline void rdpq_mode_sampler(rdpq_sampler_t s) {
    uint64_t samp = 0;
    switch (s) {
        case SAMPLER_POINT:    samp = SOM_SAMPLE_1X1; break;
        case SAMPLER_MEDIAN:   samp = SOM_SAMPLE_2X2 | SOM_SAMPLE_MIDTEXEL; break;
        case SAMPLER_BILINEAR: samp = SOM_SAMPLE_2X2; break;
    }
    rdpq_change_other_modes_raw(SOM_SAMPLE_MASK, samp);    
}

#ifdef __cplusplus
}
#endif

#endif
