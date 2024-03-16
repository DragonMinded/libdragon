/**
 * @file rdpq_mode.c
 * @brief RDP Command queue: mode setting
 * @ingroup rdp
 */

#include "rdpq_mode.h"
#include "rspq.h"
#include "rdpq_internal.h"

/** 
 * @brief Like #rdpq_write, but for mode commands.
 * 
 * During freeze (#rdpq_mode_begin), mode commands don't emit RDP commands
 * as they are batched instead, so we can avoid reserving space in the
 * RDP static buffer in blocks.
 */
#define rdpq_mode_write(num_rdp_commands, num_frozen_rdp_commands, ...) ({ \
    rdpq_write(rdpq_tracking.mode_freeze ? num_frozen_rdp_commands : num_rdp_commands, ##__VA_ARGS__); \
})

/** 
 * @brief Write a fixup that changes the current render mode (8-byte command)
 * 
 * All the mode fixups always need to update the RDP render mode
 * and thus generate two RDP commands: SET_COMBINE and SET_OTHER_MODES.
 */
__attribute__((noinline))
void __rdpq_fixup_mode(uint32_t cmd_id, uint32_t w0, uint32_t w1)
{
    __rdpq_autosync_change(AUTOSYNC_PIPE);
    rdpq_mode_write(2, 0, RDPQ_OVL_ID, cmd_id, w0, w1);  // COMBINE+SOM
}

/** @brief Write a fixup that changes the current render mode (12-byte command) */
__attribute__((noinline))
void __rdpq_fixup_mode3(uint32_t cmd_id, uint32_t w0, uint32_t w1, uint32_t w2)
{
    __rdpq_autosync_change(AUTOSYNC_PIPE);
    rdpq_mode_write(2, 0, RDPQ_OVL_ID, cmd_id, w0, w1, w2);  // COMBINE+SOM

}

/** @brief Write a fixup that changes the current render mode (16-byte command) */
__attribute__((noinline))
void __rdpq_fixup_mode4(uint32_t cmd_id, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    __rdpq_autosync_change(AUTOSYNC_PIPE);
    rdpq_mode_write(2, 0, RDPQ_OVL_ID, cmd_id, w0, w1, w2, w3);  // COMBINE+SOM
}

/** @brief Write a fixup to reset the render mode */
__attribute__((noinline))
void __rdpq_reset_render_mode(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    __rdpq_autosync_change(AUTOSYNC_PIPE);
    // ResetRenderMode can generate: SCISSOR+COMBINE+SOM when not frozen,
    // or just SCISSOR when frozen.
    rdpq_mode_write(3, 1, RDPQ_OVL_ID, RDPQ_CMD_RESET_RENDER_MODE, w0, w1, w2, w3);
}

void rdpq_mode_push(void)
{
    // Push is not a RDP passthrough/fixup command, it's just a standard
    // RSP command. Use rspq_write.
    rspq_write(RDPQ_OVL_ID, RDPQ_CMD_PUSH_RENDER_MODE, 0, 0);
}

void rdpq_mode_pop(void)
{
    __rdpq_fixup_mode(RDPQ_CMD_POP_RENDER_MODE, 0, 0);
}

/** @brief Like #rdpq_set_mode_fill, but without fill color configuration */
void __rdpq_set_mode_fill(void) {
    uint64_t som = (0xEFull << 56) | SOM_CYCLE_FILL;
    __rdpq_reset_render_mode(0, 0, som >> 32, som & 0xFFFFFFFF);
    if (!rdpq_tracking.mode_freeze)
        rdpq_tracking.cycle_type_known = 2;
    else
        rdpq_tracking.cycle_type_frozen = 2;
}

void rdpq_set_mode_copy(bool transparency) {
    uint64_t som = (0xEFull << 56) | SOM_CYCLE_COPY | (transparency ? SOM_ALPHACOMPARE_THRESHOLD : 0);
    __rdpq_reset_render_mode(0, 0, som >> 32, som & 0xFFFFFFFF);
    if (!rdpq_tracking.mode_freeze)
        rdpq_tracking.cycle_type_known = 2;
    else
        rdpq_tracking.cycle_type_frozen = 2;
}

void rdpq_set_mode_standard(void) {
    uint64_t cc = RDPQ_COMBINER1(
        (ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, TEX0)
    );
    uint64_t som =
        SOM_TF0_RGB | SOM_TF1_RGB |
        SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE |
        SOM_COVERAGE_DEST_ZAP;

    __rdpq_reset_render_mode(
        cc >> 32,   cc & 0xFFFFFFFF,
        som >> 32, som & 0xFFFFFFFF);
    rdpq_mode_combiner(cc); // FIXME: this should not be required, but we need it for the mipmap mask
    if (!rdpq_tracking.mode_freeze)
        rdpq_tracking.cycle_type_known = 1;
    else
        rdpq_tracking.cycle_type_frozen = 1;
}

void rdpq_set_mode_yuv(bool bilinear) {
    uint64_t cc, som;

    if (!bilinear) {
        som = SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | SOM_TF0_YUV;
        cc = RDPQ_COMBINER1((TEX0, K4, K5, TEX0), (ZERO, ZERO, ZERO, ONE));
    } else {
        som = SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | SOM_SAMPLE_BILINEAR | SOM_TF0_RGB | SOM_TF1_YUVTEX0;
        cc = RDPQ_COMBINER2((TEX1, K4, K5, TEX1), (ZERO, ZERO, ZERO, ONE),
                            (ZERO, ZERO, ZERO, COMBINED), (ZERO, ZERO, ZERO, COMBINED));
    }
    __rdpq_reset_render_mode(
        cc >> 32,   cc & 0xFFFFFFFF,
        som >> 32, som & 0xFFFFFFFF);
    if (!rdpq_tracking.mode_freeze)
        rdpq_tracking.cycle_type_known = 1;
    else
        rdpq_tracking.cycle_type_frozen = 1;

    rdpq_set_yuv_parms(179,-44,-91,227,111,43);  // BT.601 coefficients (Kr=0.299, Kb=0.114, TV range)
}

void rdpq_mode_begin(void)
{
    // Freeze render mode updates. We call rdpq_change_other_modes_raw here
    // (instead of __rdpq_mode_change_som) because there will be no RDP
    // commands emitted from this call.
    rdpq_tracking.mode_freeze = true;
    rdpq_tracking.cycle_type_frozen = 0;
    __rdpq_mode_change_som(SOMX_UPDATE_FREEZE, SOMX_UPDATE_FREEZE);
}

void rdpq_mode_end(void)
{
    // Unfreeze render mode updates and recalculate new render mode.
    rdpq_tracking.mode_freeze = false;
    rdpq_tracking.cycle_type_known = rdpq_tracking.cycle_type_frozen;
    __rdpq_mode_change_som(SOMX_UPDATE_FREEZE, 0);
}


/* Extern inline instantiations. */
extern inline void rdpq_set_mode_fill(color_t color);
extern inline void rdpq_set_mode_standard(void);
extern inline void rdpq_mode_combiner(rdpq_combiner_t comb);
extern inline void rdpq_mode_blender(rdpq_blender_t blend);
extern inline void rdpq_mode_antialias(rdpq_antialias_t mode);
extern inline void rdpq_mode_fog(rdpq_blender_t fog);
extern inline void rdpq_mode_dithering(rdpq_dither_t dither);
extern inline void rdpq_mode_alphacompare(int threshold);
extern inline void rdpq_mode_zbuf(bool compare, bool write);
extern inline void rdpq_mode_zoverride(bool enable, float z, int16_t deltaz);
extern inline void rdpq_mode_tlut(rdpq_tlut_t tlut);
extern inline void rdpq_mode_filter(rdpq_filter_t s);
extern inline void rdpq_mode_mipmap(rdpq_mipmap_t mode, int num_levels);
extern inline void rdpq_mode_persp(bool perspective);
///@cond
extern inline void __rdpq_mode_change_som(uint64_t mask, uint64_t val);
///@endcond
