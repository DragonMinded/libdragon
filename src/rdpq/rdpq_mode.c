#include "rdpq_mode.h"
#include "rspq.h"
#include "rdpq_internal.h"

__attribute__((noinline))
void __rdpq_fixup_mode(uint32_t cmd_id, uint32_t w0, uint32_t w1)
{
    __rdpq_autosync_change(AUTOSYNC_PIPE);
    rdpq_fixup_write(
        (cmd_id, w0, w1),
        (RDPQ_CMD_SET_COMBINE_MODE_RAW, w0, w1), (RDPQ_CMD_SET_OTHER_MODES, w0, w1)
    );
}

void rdpq_mode_push(void)
{
    __rdpq_write8(RDPQ_CMD_PUSH_RENDER_MODE, 0, 0);
}

void rdpq_mode_pop(void)
{
    __rdpq_fixup_mode(RDPQ_CMD_POP_RENDER_MODE, 0, 0);
}

void rdpq_set_mode_standard(void) {
    rdpq_set_other_modes_raw(SOM_CYCLE_1 | SOM_TF0_RGB | SOM_TF1_RGB | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE);
    rdpq_mode_combiner(RDPQ_COMBINER1((ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, TEX0)));
    rdpq_mode_blending(0);
    rdpq_mode_fog(0);
}

void rdpq_set_mode_copy(bool transparency) {
    if (transparency) rdpq_set_blend_color(RGBA32(0,0,0,1));
    rdpq_set_other_modes_raw(SOM_CYCLE_COPY | (transparency ? SOM_ALPHACOMPARE_THRESHOLD : 0));
}

void rdpq_set_mode_yuv(void) {
	rdpq_set_other_modes_raw(SOM_CYCLE_1 | SOM_RGBDITHER_NONE | SOM_TF0_YUV | SOM_TF1_YUV);
	rdpq_set_combiner_raw(RDPQ_COMBINER1((TEX0, K4, K5, ZERO), (ZERO, ZERO, ZERO, ONE)));
    rdpq_set_yuv_parms(179,-44,-91,227,19,255);  // BT.601 coefficients (Kr=0.299, Kb=0.114, TV range)
}


/* Extern inline instantiations. */
extern inline void rdpq_set_mode_fill(color_t color);
extern inline void rdpq_mode_combiner(rdpq_combiner_t comb);
extern inline void rdpq_mode_blending(rdpq_blender_t blend);
extern inline void rdpq_mode_fog(rdpq_blender_t fog);
extern inline void rdpq_mode_dithering(rdpq_dither_t rgb, rdpq_dither_t alpha);
extern inline void rdpq_mode_alphacompare(bool enable, int threshold);
extern inline void rdpq_mode_zoverride(bool enable, uint16_t z, int16_t deltaz);
extern inline void rdpq_mode_tlut(rdpq_tlut_t tlut);
extern inline void rdpq_mode_sampler(rdpq_sampler_t s);
