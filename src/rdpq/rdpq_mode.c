/**
 * @file rdpq_mode.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief RDP Command queue: mode setting
 * @ingroup rdpq
 */

#include "rdpq_mode.h"
#include "rspq.h"
#include "rdpq_internal.h"
#include "yuv.h"
#include "rdpq_macros.h"

/* ------------------------------------------------------------------------
 * Frozen-block resolver
 *
 * These tables and the __rdpq_resolve_mode() function are the C port of the
 * RSP-side RDPQ_UpdateRenderMode (rsp_rdpq.inc). They take the
 * CPU mirror of the user-input render state and produce the final RDP
 * SET_OTHER_MODES + SET_COMBINE values, exactly as the RSP would.
 *
 * The resolver is invoked from rdpq mode helpers when recording a frozen
 * block, to write fully resolved RDP commands directly into the static RDP
 * buffer (bypassing the RSP-side mode pipeline).
 *
 * Constants below must remain in sync with rsp_rdpq.inc.
 * ------------------------------------------------------------------------ */

#define _COMBINER_TEX_SHADE_NOID  (RDPQ_COMBINER_TEX_SHADE & 0x00FFFFFFFFFFFFFFULL)
#define _COMBINER_SHADE_NOID      (RDPQ_COMBINER_SHADE     & 0x00FFFFFFFFFFFFFFULL)

#define _COMBINER_TEX_SHADE_FOG   RDPQ_COMBINER1((TEX0,0,SHADE,0), (0,0,0,TEX0))
#define _COMBINER_SHADE_FOG       RDPQ_COMBINER1((0,0,0,SHADE),    (0,0,0,1))

/* Interpolated mipmap combiners (already masked by RDPQ_COMB0_MASK and tagged
 * with RDPQ_COMBINER_2PASS, matching COMBINER_MIPMAPS in rsp_rdpq.inc). */
#define _COMBINER_MIPMAP_TRILINEAR \
    ((RDPQ_COMBINER2((TEX1,TEX0,LOD_FRAC,TEX0),(TEX1,TEX0,LOD_FRAC,TEX0),(0,0,0,0),(0,0,0,0)) \
       & RDPQ_COMB0_MASK) | RDPQ_COMBINER_2PASS)
#define _COMBINER_MIPMAP_SHQ \
    ((RDPQ_COMBINER2((TEX0,TEX1,K5,0),(0,0,0,TEX1),(0,0,0,0),(0,0,0,0)) \
       & RDPQ_COMB0_MASK) | RDPQ_COMBINER_2PASS)

static const uint64_t _COMBINER_MIPMAPS[2] = {
    _COMBINER_MIPMAP_TRILINEAR,
    _COMBINER_MIPMAP_SHQ,
};

static const uint32_t _AA_BLEND_TABLE[4] = {
    (uint32_t)SOM_COVERAGE_DEST_ZAP,                                      /* AA=0 BL=0 */
    (uint32_t)SOM_COVERAGE_DEST_ZAP,                                      /* AA=0 BL=1 */
    (uint32_t)(SOM_BLALPHA_CVG | SOM_COVERAGE_DEST_CLAMP),                /* AA=1 BL=0 */
    (uint32_t)(SOM_COLOR_ON_CVG_OVERFLOW | SOM_COVERAGE_DEST_WRAP),       /* AA=1 BL=1 */
};

static const uint32_t _AA_BLEND_MASK_C =
    (uint32_t)(SOM_COVERAGE_DEST_MASK | SOM_BLEND_MASK | SOM_BLALPHA_MASK | SOM_COLOR_ON_CVG_OVERFLOW);

static const uint32_t _AA_BLEND_DEFAULT_FORMULA[2] = {
    (uint32_t)(RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, MEMORY_CVG))),
    (uint32_t)(RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, MEMORY_CVG)) & ~SOM_READ_ENABLE),
};

/* Resolved RDP mode values, ready to be written into the static RDP buffer.
 * The SOM/CC values do NOT include the RDP opcode in the top byte, the
 * emit helper adds it via rdpq_passthrough_write. */
typedef struct {
    uint64_t som;       /* Final SOM (low 56 bits meaningful; top byte 0) */
    uint64_t cc;        /* Final CC (low 56 bits meaningful; top byte 0)  */
    bool     emit_cc;   /* false in FILL/COPY mode (emit only SOM)         */
} __rdpq_resolved_t;

__rdpq_resolved_t __rdpq_resolve_mode(const rdpq_state_mirror_t *m)
{
    __rdpq_resolved_t out;
    uint64_t som = m->som;
    uint64_t cc  = m->cc;

    /* FILL/COPY cycle: emit only SOM, no CC (matches the RSP-side asm test of
     * bit 53, set when cycle is COPY or FILL). */
    if ((som & SOM_CYCLE_MASK) >= SOM_CYCLE_COPY) {
        out.som     = som & 0x00FFFFFFFFFFFFFFULL;
        out.cc      = 0;
        out.emit_cc = false;
        return out;
    }

    uint64_t cc_1cyc = cc;
    uint64_t cc_2cyc = cc;
    bool comb_2pass  = (cc & RDPQ_COMBINER_2PASS) != 0;

    if (!comb_2pass) {
        /* 1-pass combiner fog substitution? */
        if (som & SOMX_FOG) {
            uint64_t cc_noid = cc & 0x00FFFFFFFFFFFFFFULL;
            if (cc_noid == _COMBINER_TEX_SHADE_NOID)
                cc = _COMBINER_TEX_SHADE_FOG;
            else if (cc_noid == _COMBINER_SHADE_NOID)
                cc = _COMBINER_SHADE_FOG;
        }

        uint8_t lod_interp = (uint8_t)((som & SOMX_LOD_INTERP_MASK) >> SOMX_LOD_INTERP_SHIFT);
        if (lod_interp != 0) {
            /* Interpolated mipmap: apply mipmask, OR in lookup. Force 2-cycle. */
            cc       = (cc & m->cc_mipmask) | _COMBINER_MIPMAPS[lod_interp - 1];
            cc_2cyc  = cc;
            /* cc_1cyc unused. need_2cyc will be true */
        } else {
            cc_1cyc = cc;
            cc_2cyc = cc & RDPQ_COMB0_MASK;
            /* Nearest mipmapping (SOM_TEXTURE_LOD) forces 2-cycle. */
            if (som & SOM_TEXTURE_LOD)
                cc_2cyc |= RDPQ_COMBINER_2PASS;
        }
    } else {
        cc_2cyc = cc;
    }

    /* Blender step merge */
    uint32_t step0 = m->blender_steps[0];   /* fog */
    uint32_t step1 = m->blender_steps[1];   /* blender */
    bool bkg_blending = (step1 != 0);

    if (step1 == 0 && (som & SOM_AA_ENABLE)) {
        int aa_reduced_idx = (som & SOMX_AA_REDUCED) ? 1 : 0;
        step1 = _AA_BLEND_DEFAULT_FORMULA[aa_reduced_idx];
        step0 &= ~(uint32_t)SOM_BLENDING;
    }

    uint32_t blend_1cyc, blend_2cyc;
    if (step0 == 0 || step1 == 0) {
        uint32_t single = step0 | step1;
        blend_2cyc = single & (uint32_t)SOM_BLEND1_MASK;
        blend_1cyc = single & (uint32_t)SOM_BLEND0_MASK;
    } else {
        blend_1cyc = 0;
        blend_2cyc = (step0 & (uint32_t)SOM_BLEND0_MASK)
                   | (step1 & (uint32_t)SOM_BLEND1_MASK)
                   | (uint32_t)SOMX_BLEND_2PASS;
    }

    /* YUV bilinear fixup */
    if ((som & (SOM_TF_MASK | SOM_SAMPLE_MASK)) == (SOM_SAMPLE_BILINEAR | SOM_TF0_YUV | SOM_TF1_YUV))
        som |= SOM_TF1_YUVTEX0 | SOM_TF0_RGB;

    /* 1cyc vs 2cyc selection */
    bool need_2cyc = (blend_2cyc & (uint32_t)SOMX_BLEND_2PASS) || (cc_2cyc & RDPQ_COMBINER_2PASS);
    uint64_t cc_final;
    uint32_t blend_final;
    if (need_2cyc) {
        cc_final    = cc_2cyc;
        blend_final = blend_2cyc;
        som = (som & ~SOM_CYCLE_MASK) | SOM_CYCLE_2;
    } else {
        cc_final    = cc_1cyc;
        blend_final = blend_1cyc;
        som = (som & ~SOM_CYCLE_MASK) | SOM_CYCLE_1;
    }

    /* AA + coverage configuration */
    int aa_idx = ((som & SOM_AA_ENABLE) ? 2 : 0) | (bkg_blending ? 1 : 0);
    uint32_t aa_bits = _AA_BLEND_TABLE[aa_idx] | blend_final;
    som = (som & ~(uint64_t)_AA_BLEND_MASK_C) | (uint64_t)(aa_bits & _AA_BLEND_MASK_C);

    /* AA + alpha-compare interaction */
    uint64_t ac_mask = SOM_ALPHACOMPARE_THRESHOLD | SOM_BLALPHA_CVG;
    if ((som & ac_mask) == ac_mask) {
        som |= SOM_BLALPHA_CVG_TIMES_CC;
        som &= ~SOM_ALPHACOMPARE_MASK;
    }

    /* ZMODE auto-toggle: when ZMODE is STANDARD or TRANSPARENT (bit 10 clear),
     * set bit 11 iff blending is active. */
    if (!(som & (1ULL << 10))) {
        if (som & SOM_BLENDING)
            som |= (2ULL << 10);
        else
            som &= ~(2ULL << 10);
    }

    out.som     = som & 0x00FFFFFFFFFFFFFFULL;
    out.cc      = cc_final & 0x00FFFFFFFFFFFFFFULL;
    out.emit_cc = true;
    return out;
}

/* ------------------------------------------------------------------------
 * Frozen-block emission helpers
 *
 * Called from the mode-emit hot paths when recording into a frozen block.
 * They write resolved SET_OTHER_MODES (+ SET_COMBINE) directly into the
 * static RDP buffer, replacing the RSP-side mode pipeline at run time.
 * ------------------------------------------------------------------------ */

void __rdpq_frozen_emit_resolved_mode(void)
{
    __rdpq_resolved_t r = __rdpq_resolve_mode(&rdpq_state_mirror);

    /* The rdpq_passthrough_write macro packs the cmd id into the top byte of
     * the first word, producing a valid RDP command in the static buffer:
     *   RDPQ_CMD_SET_OTHER_MODES = 0x2F  -> top byte 0xC + 0x2F = 0xEF
     *   RDPQ_CMD_SET_COMBINE_MODE_RAW = 0x3C -> top byte 0xC + 0x3C = 0xFC
     */
    if (r.emit_cc)
        rdpq_passthrough_write((RDPQ_CMD_SET_COMBINE_MODE_RAW,
            (uint32_t)(r.cc >> 32) & 0x00FFFFFF, (uint32_t)r.cc));
    rdpq_passthrough_write((RDPQ_CMD_SET_OTHER_MODES,
        (uint32_t)(r.som >> 32) & 0x00FFFFFF, (uint32_t)r.som));
}

/* Emit just SET_OTHER_MODES from the current mirror SOM (no resolution).
 * Used by the raw set_other_modes / change_other_modes paths.
 *
 * Matches RSP behavior: RDPQCmd_SetOtherModes / ModifyOtherModes (without the
 * 1<<15 recalc flag) fall through to RDPQ_FinalizeOtherModes which also
 * emits SET_SCISSOR (cycle-adjusted from RDPQ_SCISSOR_RECT). We follow suit. */
/* Publish the frozen block's post-state to DMEM so the RSP-side mode tracking
 * stays consistent after the block runs. Emits a small sequence of internal
 * RSPQ_CMD_WRITE_WORD commands targeting the relevant rdpq DMEM slots.
 *
 * The RDPQ_OTHER_MODES value pushed is the resolver output (matching what
 * RSP would have produced) merged with the mirror's top byte (SOMX_* flags).
 * The other slots (RDPQ_COMBINER, BLENDER_STEPS, etc.) are user inputs, so
 * they go through verbatim from the mirror. */
void __rdpq_frozen_publish_post_state(void)
{
    __rdpq_resolved_t r = __rdpq_resolve_mode(&rdpq_state_mirror);
    /* DMEM RDPQ_OTHER_MODES keeps the user's top byte (SOMX flags); the low
     * 56 bits are the resolved SOM. */
    uint64_t som_dmem = (rdpq_state_mirror.som & 0xFF00000000000000ULL)
                      | (r.som & 0x00FFFFFFFFFFFFFFULL);

    uint32_t off_om       = offsetof(rsp_queue_t, rdp_mode.other_modes);
    uint32_t off_cc       = offsetof(rsp_queue_t, rdp_mode.combiner);
    uint32_t off_cc_mm    = offsetof(rsp_queue_t, rdp_mode.combiner_mipmapmask);
    uint32_t off_bs0      = offsetof(rsp_queue_t, rdp_mode.blend_step0);
    uint32_t off_bs1      = offsetof(rsp_queue_t, rdp_mode.blend_step1);
    uint32_t off_scissor  = offsetof(rsp_queue_t, rdp_scissor_rect);
    uint32_t off_fill     = offsetof(rsp_queue_t, rdp_fill_color);

    rspq_int_write(RSPQ_CMD_WRITE_WORD, off_om      + 0, (uint32_t)(som_dmem >> 32));
    rspq_int_write(RSPQ_CMD_WRITE_WORD, off_om      + 4, (uint32_t)som_dmem);
    rspq_int_write(RSPQ_CMD_WRITE_WORD, off_cc      + 0, (uint32_t)(rdpq_state_mirror.cc >> 32));
    rspq_int_write(RSPQ_CMD_WRITE_WORD, off_cc      + 4, (uint32_t)rdpq_state_mirror.cc);
    rspq_int_write(RSPQ_CMD_WRITE_WORD, off_cc_mm   + 0, (uint32_t)(rdpq_state_mirror.cc_mipmask >> 32));
    rspq_int_write(RSPQ_CMD_WRITE_WORD, off_cc_mm   + 4, (uint32_t)rdpq_state_mirror.cc_mipmask);
    rspq_int_write(RSPQ_CMD_WRITE_WORD, off_bs0,         rdpq_state_mirror.blender_steps[0]);
    rspq_int_write(RSPQ_CMD_WRITE_WORD, off_bs1,         rdpq_state_mirror.blender_steps[1]);
    /* DMEM RDPQ_SCISSOR_RECT stores the *full* SET_SCISSOR command, with the
     * 0xED RDP opcode byte in the high byte of word 0. The CPU mirror, however,
     * only stores the data bits (XH/YH/XL/YL), because that is what userland
     * rdpq_set_scissor() passes to __rdpq_set_scissor(). 
     * The RSP-side RDPQCmd_SetScissorEx handler ORs in 0xED before writing to DMEM. 
     * We must do the same here, otherwise we wipe the opcode byte and subsequent
     * RDPQ_FinalizeOtherModes / RDPQCmd_PopMode loads (which copy DMEM straight
     * to the staging area) end up emitting a NOP-looking command to the RDP. */
    rspq_int_write(RSPQ_CMD_WRITE_WORD, off_scissor + 0,
        0xED000000u | (uint32_t)(rdpq_state_mirror.scissor >> 32));
    rspq_int_write(RSPQ_CMD_WRITE_WORD, off_scissor + 4, (uint32_t)rdpq_state_mirror.scissor);
    rspq_int_write(RSPQ_CMD_WRITE_WORD, off_fill,        rdpq_state_mirror.fill_color);

    /* PRIM_COLOR_EX / PRIM_COLOR_RGBA live in the rdpq overlay state (rsp_rdpq.S)
     * and are not part of rsp_queue_t. Compute the offset relative to RDPQ_MODE
     * end + scissor/buffers etc. is non-trivial for phase 3 MVP we skip them.
     * Consequence: rdpq_set_prim_lod_frac / set_detail_factor called *after* a
     * frozen block will see DMEM RDPQ_PRIM_COLOR_EX from the previous non-frozen
     * call. Acceptable since these partial-update APIs are rarely interleaved
     * across frozen-block boundaries; the staleness check on the CPU mirror
     * catches genuine drift on re-record. */

    /* RDPQ_TARGET_BITDEPTH is 1 byte sharing a word with RDPQ_SYNCFULL_ONGOING;
     * not republished here (frozen blocks rarely change render target). */
}

void __rdpq_frozen_emit_scissor_adjusted(void)
{
    /* Scissor: write current mirror.scissor, adjusting bottom-right by -1
     * subpixel in FILL/COPY mode (matches RDPQ_WriteSetScissor). */
    uint64_t sc = rdpq_state_mirror.scissor;
    uint32_t sc_lo = (uint32_t)sc;
    if ((rdpq_state_mirror.som & SOM_CYCLE_MASK) >= SOM_CYCLE_COPY)
        sc_lo -= (1u << 12);    /* -1 subpixel on XL (10.2 fixed at bits 23:12) */
    rdpq_passthrough_write((RDPQ_CMD_SET_SCISSOR,
        (uint32_t)(sc >> 32) & 0x00FFFFFF, sc_lo));
}

void __rdpq_frozen_emit_raw_som_and_scissor(void)
{
    uint64_t som = rdpq_state_mirror.som & 0x00FFFFFFFFFFFFFFULL;
    rdpq_passthrough_write((RDPQ_CMD_SET_OTHER_MODES,
        (uint32_t)(som >> 32) & 0x00FFFFFF, (uint32_t)som));
    __rdpq_frozen_emit_scissor_adjusted();
}

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

extern void __rdpq_mirror_change_som(uint32_t w0, uint32_t w1, uint32_t w2);

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

    // CPU mirror: 8-byte mode fixups carry either a 2-pass combiner, a blender
    // formula, or a fog formula in w0|w1. Dispatch on cmd_id.
    switch (cmd_id) {
    case RDPQ_CMD_SET_COMBINE_MODE_2PASS:
        rdpq_state_mirror.cc = ((uint64_t)(w0 & 0x00FFFFFF) << 32) | (uint64_t)w1
                             | RDPQ_COMBINER_2PASS;
        break;
    case RDPQ_CMD_SET_BLENDING_MODE:
        rdpq_state_mirror.blender_steps[1] = w1;
        break;
    case RDPQ_CMD_SET_FOG_MODE:
        rdpq_state_mirror.blender_steps[0] = w1;
        break;
    }

    // Frozen-block recording: emit a fully-resolved SET_COMBINE+SET_OTHER_MODES
    // pair into the static RDP buffer, bypassing the RSP-side mode pipeline.
    if (rdpq_block_state.frozen) {
        __rdpq_frozen_emit_resolved_mode();
        return;
    }

    rdpq_mode_write(2, 0, RDPQ_OVL_ID, cmd_id, w0, w1);  // COMBINE+SOM
}

/** @brief Write a fixup that changes the current render mode (12-byte command) */
__attribute__((noinline))
void __rdpq_fixup_mode3(uint32_t cmd_id, uint32_t w0, uint32_t w1, uint32_t w2)
{
    __rdpq_autosync_change(AUTOSYNC_PIPE);

    // CPU mirror: 12-byte mode fixups today are only RDPQ_CMD_MODIFY_OTHER_MODES
    // with the 1<<15 recalc flag, applying a SOM partial update.
    if (cmd_id == RDPQ_CMD_MODIFY_OTHER_MODES)
        __rdpq_mirror_change_som(w0, w1, w2);

    if (rdpq_block_state.frozen) {
        __rdpq_frozen_emit_resolved_mode();
        return;
    }

    rdpq_mode_write(2, 0, RDPQ_OVL_ID, cmd_id, w0, w1, w2);  // COMBINE+SOM
}

/** @brief Write a fixup that changes the current render mode (16-byte command) */
__attribute__((noinline))
void __rdpq_fixup_mode4(uint32_t cmd_id, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    __rdpq_autosync_change(AUTOSYNC_PIPE);

    // CPU mirror: 16-byte mode fixup is the 1-pass combiner setter, carrying
    // the user combiner (w0|w1) and the mipmap mask (w2|w3).
    if (cmd_id == RDPQ_CMD_SET_COMBINE_MODE_1PASS) {
        rdpq_state_mirror.cc         = ((uint64_t)(w0 & 0x00FFFFFF) << 32) | (uint64_t)w1;
        rdpq_state_mirror.cc_mipmask = ((uint64_t)(w2 & 0x00FFFFFF) << 32) | (uint64_t)w3;
    }

    if (rdpq_block_state.frozen) {
        __rdpq_frozen_emit_resolved_mode();
        return;
    }

    rdpq_mode_write(2, 0, RDPQ_OVL_ID, cmd_id, w0, w1, w2, w3);  // COMBINE+SOM
}

/** @brief Write a fixup to reset the render mode */
__attribute__((noinline))
void __rdpq_reset_render_mode(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    __rdpq_autosync_change(AUTOSYNC_PIPE);

    // CPU mirror: ResetRenderMode rewrites both CC (w0|w1) and SOM (w2|w3),
    // and resets the mipmap mask. (It is used by rdpq_set_mode_standard /
    // _copy / _fill / _yuv, which then optionally also call rdpq_mode_combiner
    // to overwrite cc/mipmask through __rdpq_fixup_mode4.)
    rdpq_state_mirror.cc         = ((uint64_t)(w0 & 0x00FFFFFF) << 32) | (uint64_t)w1;
    rdpq_state_mirror.cc_mipmask = 0;
    rdpq_state_mirror.som        = ((uint64_t)(w2 & 0x00FFFFFF) << 32) | (uint64_t)w3;
    rdpq_state_mirror.blender_steps[0] = 0;
    rdpq_state_mirror.blender_steps[1] = 0;

    if (rdpq_block_state.frozen) {
        // Reset also emits SET_SCISSOR (with cycle adjustment) to match the RSP
        // path which goes through RDPQCmd_ResetMode -> RDPQ_WriteSetScissor.
        __rdpq_frozen_emit_raw_som_and_scissor();
        // Then also emit the resolved CC (and resolved SOM again).
        __rdpq_frozen_emit_resolved_mode();
        return;
    }

    // ResetRenderMode can generate: SCISSOR+COMBINE+SOM when not frozen,
    // or just SCISSOR when frozen.
    rdpq_mode_write(3, 1, RDPQ_OVL_ID, RDPQ_CMD_RESET_RENDER_MODE, w0, w1, w2, w3);
}

/* CPU-side shadow of the RSP mode stack (RDPQ_MODE_STACK in DMEM has 3 slots).
 * Mirrors what RSP push/pop does, so the CPU mirror stays consistent with
 * DMEM across rdpq_mode_push / rdpq_mode_pop calls (used by e.g. rdpq_clear
 * and rdpq_attach to wrap a temporary mode change). */
#define RDPQ_MIRROR_STACK_SLOTS 3
static rdpq_state_mirror_t __rdpq_mirror_stack[RDPQ_MIRROR_STACK_SLOTS];
static int __rdpq_mirror_stack_depth = 0;

void rdpq_mode_push(void)
{
    // Shadow the push: save the current mirror so the matching pop can
    // restore it. If the stack overflows, mark unknown (matches the existing
    // RSP-side assertion behavior, which would also fail).
    if (__rdpq_mirror_stack_depth < RDPQ_MIRROR_STACK_SLOTS)
        __rdpq_mirror_stack[__rdpq_mirror_stack_depth++] = rdpq_state_mirror;
    else
        rdpq_state_mirror.unknown = 1;

    // Frozen-block recording: the CPU shadow stack is the source of truth for
    // mode state across push/pop, so the RSP-side stack is irrelevant, the
    // block emits raw RDP commands directly and doesn't rely on RDPQ_MODE_STACK.
    // Skip the rspq overlay command (which would cost an overlay switch) and
    // rely on publish-post-state to resync DMEM at block end.
    if (rdpq_block_state.frozen)
        return;

    // Push is not a RDP passthrough/fixup command, it's just a standard
    // RSP command. Use rspq_write.
    rspq_write(RDPQ_OVL_ID, RDPQ_CMD_PUSH_RENDER_MODE);
}

void rdpq_mode_pop(void)
{
    __rdpq_autosync_change(AUTOSYNC_PIPE);

    // Restore the saved mirror to match what RSP-side pop will load from
    // RDPQ_MODE_STACK. Without this, cycle-type bits and other SOM state
    // diverge from DMEM after any push/pop pair (e.g. rdpq_clear wraps a
    // FILL-mode op in push/pop, so without restore the mirror stays at FILL).
    if (__rdpq_mirror_stack_depth > 0)
        rdpq_state_mirror = __rdpq_mirror_stack[--__rdpq_mirror_stack_depth];
    else
        rdpq_state_mirror.unknown = 1;

    rdpq_tracking.cycle_type_known = 0;

    // Frozen recording: re-emit the popped state (scissor + resolved mode)
    // directly as raw RDP commands. No rspq overlay command needed.
    if (rdpq_block_state.frozen) {
        __rdpq_frozen_emit_scissor_adjusted();
        __rdpq_frozen_emit_resolved_mode();
        return;
    }

    // ModePop can generate: SCISSOR+COMBINE+SOM when not frozen,
    // or just SCISSOR when frozen.
    rdpq_mode_write(3, 1, RDPQ_OVL_ID, RDPQ_CMD_POP_RENDER_MODE);
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

    som = SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | SOM_TF0_YUV | SOM_TF1_YUV;
    cc = RDPQ_COMBINER1((TEX0, K4, K5, TEX0), (0, 0, 0, 1));

    __rdpq_reset_render_mode(
        cc >> 32,   cc & 0xFFFFFFFF,
        som >> 32, som & 0xFFFFFFFF);
    if (bilinear) {
        rdpq_mode_filter(FILTER_BILINEAR);
        cc = RDPQ_COMBINER1((TEX1, K4, K5, TEX1), (0, 0, 0, 1));
    }
    rdpq_mode_combiner(cc); // FIXME: this should not be required, but we need it for the mipmap mask

    if (!rdpq_tracking.mode_freeze)
        rdpq_tracking.cycle_type_known = 1;
    else
        rdpq_tracking.cycle_type_frozen = 1;

    // Set the YUV coefficients (FIXME: make this configurable)
    const yuv_colorspace_t *cs = &YUV_BT601_TV;
    rdpq_set_yuv_parms(cs->k0, cs->k1, cs->k2, cs->k3, cs->k4, cs->k5);
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
