
##############################################################################
#  RDPQ library
##############################################################################
#
# This library allows rspq overlays to emit RDP commands.
#
# If you want to write a rspq overlay that emits RDP commands, do the following:
#
#   * Include rsp_rdpq.inc (this file) at the *end* of your overlay source
#     code.
#   * In your code, prepare the RDP command in a0/a1 (+ a2/a3 for 16 bytes
#     commands) and then call RDPQ_Write8 or RDPQ_Write16 to store it into
#     a temporary DMEM buffer (RDP_CMD_STAING). You can do this as many times
#     as you need.
#   * Call RDPQ_Finalize to send the commands to RDP for drawing. This must
#     currently be the last thing your command does, as that function
#     doesn't return but go back to processing next command (RSPQ_Loop).
#
##############################################################################


#include "rdpq_macros.h"
#include "rdpq_constants.h"

#########################################################################
#
#  RDPQ_Send: send commands from DMEM to RDP
#
##########################################################################

    .section .data.rdpq_send

# TODO: get rid of the constant offset
RDPQ_CMD_PTR:           .word RDPQ_CMD_STAGING - 0xA4000000

    .section .bss.rdpq_send


    .align 4
    # Enough for a full triangle command
RDPQ_CMD_STAGING:       .ds.b 0xB0

    .section .text.rdpq_send

    #############################################################
    # RDPQ_Send
    #
    # Internal API for overlays that generate RDP commands. It
    # runs a DMA transfer from DMEM to the RDP ouput buffer in RDRAM
    # to copy some new RDP commands, and tell RDP to run them.
    #
    # ARGS:
    #   s4:                 buffer in DMEM containing RDP commands to send to RDP
    #   s3:                 pointer to the end of the buffer in DMEM  (s3-s4 = size)
    #############################################################
    .func RDPQ_Send
RDPQ_Send:
    #define rdram_cur   s0
    #define rdram_next  a0
    #define sentinel    a2
    #define buf_idx     t4
    #define next_func   t5

    # Calculate buffer size and DMA transfer length
    #ifndef NDEBUG
    andi s3, 0xFFF
    andi s4, 0xFFF
    assert_ge s3, s4, RDPQ_ASSERT_SEND_INVALID_SIZE
    #endif
    sub rspq_cmd_size, s3, s4
    beqz rspq_cmd_size, JrRa   # Exit if s3==s4 (0 byte transfer)
    move ra2, ra

    # Fetch current pointer in RDRAM where to write the RDP command
    # This is normally the same value that's in DP_END (unless we
    # are holding writes because there is a SYNC_FULL pending).
    lw rdram_cur, %lo(RDPQ_CURRENT)
    and rdram_cur, 0xFFFFFF

    # Fetch the sentinel (end of buffer). Check whether there is
    # enough room to add the new command. If so, run the DMA transfer,
    # and then call RSPQCmd_RdpAppendBuffer to update DP_END to include
    # the new commands.
    lw sentinel, %lo(RDPQ_SENTINEL)
    sub sentinel, rspq_cmd_size
    bge sentinel, rdram_cur, do_dma
    li next_func, RSPQCmd_RdpAppendBuffer

    # There is not enough space in the current buffer, so we will need to switch
    # to the next. Since the RDP DMA can hold two buffers in total, and we have two
    # buffers, we need to make sure that we are not overwriting the buffer that
    # is currently playing. To do so, wait for the END_VALID to become 0, which means
    # that only one buffer might be peInding.
    # Also, in case there is a SYNC_FULL ongoing, we need to wait for it to finish before
    # enqueuing a new buffer. RDPQ_SYNCFULL_ONGOING is set to DP_STATUS_BUSY in this case,
    # so using that bit in the RSPQ_RdpWait wait mask will make sure we wait for the RDP
    # to be idle.
    lbu t3, %lo(RDPQ_SYNCFULL_ONGOING)
    jal RSPQ_RdpWait
    ori t3, DP_STATUS_END_VALID

    # Switch to the next dynamic buffer.
    # Since there are two of them, also switch between
    # them so next time we will pick the other one.
    lw rdram_cur, %lo(RDPQ_DYNAMIC_BUFFERS) + 4
    lw t1, %lo(RDPQ_DYNAMIC_BUFFERS) + 0
    sw rdram_cur, %lo(RDPQ_DYNAMIC_BUFFERS) + 0
    sw t1, %lo(RDPQ_DYNAMIC_BUFFERS) + 4

    # Calculate new sentinel (end of buffer)
    add sentinel, rdram_cur, RDPQ_DYNAMIC_BUFFER_SIZE

    # Run the DMA transfer now, and after that, run RSPQCmd_RdpSetBuffer via
    # tailcall. Prepare a1 for it, containing the pointer to the new buffer,
    # which will be written into DP_START.
    move a1, rdram_cur
    li next_func, RSPQCmd_RdpSetBuffer

do_dma:
    # Start the transfer. Will tail-call to either RSPQCmd_RdpSetBuffer or
    # RSPQCmd_RdpAppendBuffer (see above). For both, we need to prepare
    # rdram_next (aka a0) with the updated pointer to RDRAM that will be 
    # written to DP_END to run the newly written commands.
    add t0, rspq_cmd_size, -1
    jal DMAOut
    add rdram_next, rdram_cur, rspq_cmd_size

    # Jump to continuation function (either RSPQCmd_RdpSetBuffer or
    # RSPQCmd_RdpAppendBuffer), after recovering original return address.
    jr next_func
    move ra, ra2
    .endfunc

    #undef rdram_cur
    #undef rdram_next
    #undef sentinel
    #undef buf_idx 
    #undef next_func

    .section .text.rdpq_send_helpers

    #############################################################
    # RDPQ_Finalize
    #
    # Submits the RDP command(s) in RDPQ_CMD_STAGING to the RDP.
    #############################################################
    .func RDPQ_Finalize
RDPQ_Finalize:
    li s4, %lo(RDPQ_CMD_STAGING)
    lw s3, %lo(RDPQ_CMD_PTR)
    sw s4, %lo(RDPQ_CMD_PTR)
    jal_and_j RDPQ_Send, RSPQ_Loop
    .endfunc

    #############################################################
    # RDPQ_Write8
    #
    # Appends 8 bytes from a0-a1 to the staging area (RDPQ_CMD_STAGING).
    #############################################################
    .func RDPQ_Write8
RDPQ_Write8:
    lw s0, %lo(RDPQ_CMD_PTR)
    sw a0, 0x00(s0)
    sw a1, 0x04(s0)
    addi s0, 8
    jr ra
    sw s0, %lo(RDPQ_CMD_PTR)
    .endfunc

    #############################################################
    # RDPQ_Write16
    #
    # Appends 16 bytes from a0-a3 to the staging area (RDPQ_CMD_STAGING).
    #############################################################
    .func RDPQ_Write16
RDPQ_Write16:
    lw s0, %lo(RDPQ_CMD_PTR)
    sw a0, 0x00(s0)
    sw a1, 0x04(s0)
    sw a2, 0x08(s0)
    sw a3, 0x0C(s0)
    addi s0, 16
    jr ra
    sw s0, %lo(RDPQ_CMD_PTR)
    .endfunc


#########################################################################
#########################################################################
#
#  RDPQ Mode API: smart, assisted render-mode changes
#
# These functions implement the mode API. They can be useful
# for overlays that want to change RDP render mode, using the
# mode API for simplicity and interoperability.
#
##########################################################################
#########################################################################


    .section .data.rdpq_mode_api

AA_BLEND_MASK:
    #  MASK
    .word SOM_COVERAGE_DEST_MASK | SOM_BLEND_MASK | SOM_BLALPHA_MASK | SOM_COLOR_ON_CVG_OVERFLOW
AA_BLEND_TABLE:
    #  AA=0 / BLEND=0
    .word SOM_COVERAGE_DEST_ZAP
    #  AA=0 / BLEND=1
    .word SOM_COVERAGE_DEST_ZAP
    #  AA=1 / BLEND=0
    .word SOM_BLALPHA_CVG | SOM_COVERAGE_DEST_CLAMP
    #  AA=1 / BLEND=1
    .word SOM_COLOR_ON_CVG_OVERFLOW | SOM_COVERAGE_DEST_WRAP

AA_BLEND_DEFAULT_FORMULA:
    .word RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, MEMORY_CVG))                      # Standard AA
    .word RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, MEMORY_CVG)) & ~SOM_READ_ENABLE   # Reduced AA

#define RDPQ_COMB_MIPMAP2       RDPQ_COMBINER2((TEX1, TEX0, LOD_FRAC, TEX0), (TEX1, TEX0, LOD_FRAC, TEX0), (0,0,0,0), (0,0,0,0))
#define RDPQ_COMB_SHADE_FOG     RDPQ_COMBINER1((0,0,0,SHADE),      (0,0,0,1))
#define RDPQ_COMB_TEX_SHADE_FOG RDPQ_COMBINER1((TEX0,0,SHADE,0),   (0,0,0,TEX0))

COMB0_MASK:             .quad RDPQ_COMB0_MASK
COMBINER_SHADE:         .quad RDPQ_COMBINER_SHADE
COMBINER_SHADE_FOG:     .quad RDPQ_COMB_SHADE_FOG
COMBINER_TEX_SHADE:     .quad RDPQ_COMBINER_TEX_SHADE
COMBINER_TEX_SHADE_FOG: .quad RDPQ_COMB_TEX_SHADE_FOG

COMBINER_MIPMAP2:       .quad (RDPQ_COMB_MIPMAP2 & RDPQ_COMB0_MASK) | RDPQ_COMBINER_2PASS

    .section .bss.rdpq_mode_api

# Temporary combiner memory location for RDPQ_UpdateRenderMode
RDPQ_MODE_COMBINER_1CYC:     .quad  0
RDPQ_MODE_COMBINER_2CYC:     .quad  0


    .section .text.rdpq_mode_api

    .func RDPQ_SetBlendingMode
RDPQ_SetBlendingMode:
    # Set the blending mode formula in the second step. Then:
    #  * If the formula is empty, clear also the first step if it was
    #    part of a previous two-step blending (SOMX_BLEND_2PASS).
    #  * If the formula is not empty and it is two-steps (SOMX_BLEND_2PASS),
    #    put it also in the first step.
    #  We cover both conditision in one single codeflow by putting into t0
    #  the value to check against SOMX_BLEND_2PASS.
    lw t0, %lo(RDPQ_MODE_BLENDER_STEPS) + 0
    beqz a1, setblending_check
    sw a1, %lo(RDPQ_MODE_BLENDER_STEPS) + 4
    move t0, a1
setblending_check:
    andi t0, SOMX_BLEND_2PASS
    beqz t0, RDPQ_UpdateRenderMode
    nop
    # fallthrough!
    .endfunc

    .func RDPQCmd_SetFogMode
RDPQ_SetFogMode:
    # Set the fog mode formula in the first step
    j RDPQ_UpdateRenderMode
    sw a1, %lo(RDPQ_MODE_BLENDER_STEPS) + 0
    .endfunc

    .func RDPQ_SetCombineMode_1Pass
RDPQ_SetCombineMode_1Pass:
    # Turn off RDPQ_COMBINER_2PASS (bit 63). This is set by default
    # because the overlay is regisred in slots 0xC0-0xF0.
    # We need to remember that this combiner does not require 2 passes
    xor a0, RDPQ_COMBINER_2PASS >> 32
    sw a2, %lo(RDPQ_COMBINER_MIPMAPMASK) + 0
    sw a3, %lo(RDPQ_COMBINER_MIPMAPMASK) + 4
    # fallthrough!
    .endfunc

    .func RDPQ_SetCombineMode_2Pass
RDPQ_SetCombineMode_2Pass:
    # Set correct SET_COMBINE opcode (0xFC). The opcode can be anything of
    # the other 4 (1pass/2pass dynamic/static).
    or a0, 0x7F000000
    xor a0, 0x7F000000 ^ 0x7C000000
    # Save the input combiner
    sw a0, %lo(RDPQ_COMBINER) + 0
    sw a1, %lo(RDPQ_COMBINER) + 4
    # fallthrough!
    .endfunc

    ###########################################################
    # RDPQ_UpdateRenderMode
    #
    # This function is the core of the rdpq mode API.
    #
    # It performs several calculations and finally emit a
    # new render mode (with SET_COMBINE_MODE + SET_OTHER_MODES).
    #
    # It handles:
    #
    #   * If fog is enabled, tweak standard combiners to avoid
    #     passing SHADE_ALPHA to the blender as IN_ALPHA.
    #   * If interpolated mipmap is enabled, modify the color
    #     combiner adding the mipmap formula.
    #   * Merge the two blender steps (fogging / blending), taking
    #     care of adjustments if either is active or not.
    #   * Decide whether to use 1cycle or 2cycle mode, depending
    #     on color combiner, blender and mipmapping.
    #   * Adjust coverage modes depending on antialias and
    #     blending settings.
    #
    ###########################################################

    .func RDPQ_UpdateRenderMode
RDPQ_UpdateRenderMode:
    #define comb_hi      a0
    #define comb_lo      a1
    #define som_hi       a2
    #define som_lo       a3
    #define comb_hi_noid t5
    #define blend_1cyc   v0
    #define blend_2cyc   v1
    #define blend_final  v1
    #define passthrough  t7
    #define cycle_type   t6
    #define bkg_blending t8

    # If updates are frozen, do nothing
    # We just call RDPQ_Finalize just in case we come
    # from RDPQCmd_ResetMode and we have a pending
    # scissor change to send.
    lw som_hi, %lo(RDPQ_OTHER_MODES) + 0
    andi t0, som_hi, SOMX_UPDATE_FREEZE >> 32
    bnez t0, RDPQ_Finalize
    lw som_lo, %lo(RDPQ_OTHER_MODES) + 4

    # If we are in fill/copy mode, we just need to emit SOM
    sll t0, som_hi, 63 - (SOM_CYCLE_SHIFT+1)
    bltz t0, rdpq_update_fillcopy

    # If the input combiner is 1-pass, proceed working on it
    lw comb_hi, %lo(RDPQ_COMBINER) + 0
    bgez comb_hi, calc_comb_1cyc
    lw comb_lo, %lo(RDPQ_COMBINER) + 4

    # This is a 2-pass combiner. It is not compatible with interpolated mipmaps.
    and t0, som_hi, SOMX_LOD_INTERPOLATE >> 32
    assert_eq t0, zero, RDPQ_ASSERT_MIPMAP_COMB2
    j store_comb_2cyc

calc_comb_1cyc:    
    # Check if fogging is active
    andi t0, som_hi, SOMX_FOG >> 32
    beqz t0, check_mipmap_interp

    # Create a copy of comb_hi without the cmd ID in the top MSB.
    # The ID is kept sort of "random" for the whole computation,
    # because it is reset to SET_COMBINE_MODE just at the end.
    # So we cannot use it for the next comparisons.
    sll comb_hi_noid, comb_hi, 8
    srl comb_hi_noid, 8

    # When fogging is active, we need to adapt the standard color combiners to avoid
    # using SHADE alpha, because it contains depth. We currently have two of them:
    # COMBINER_TEX_SHADE and COMBINER_SHADE.
check_fog_tex_shade:
    lw t0, %lo(COMBINER_TEX_SHADE) + 0
    bne t0, comb_hi_noid, check_fog_shade
    lw t0, %lo(COMBINER_TEX_SHADE) + 4
    beq t0, comb_lo, fog_change
    li s0, %lo(COMBINER_TEX_SHADE_FOG)

check_fog_shade:
    lw t0, %lo(COMBINER_SHADE) + 0
    bne t0, comb_hi_noid, check_mipmap_interp
    lw t0, %lo(COMBINER_SHADE) + 4
    bne t0, comb_lo, check_mipmap_interp
    li s0, %lo(COMBINER_SHADE_FOG)

fog_change:
    lw comb_hi, 0(s0)
    lw comb_lo, 4(s0)

check_mipmap_interp:
    and t0, som_hi, SOMX_LOD_INTERPOLATE >> 32
    beqz t0, store_comb_1cyc

    # Interpolated mipmapping is active. We want to add RDPQ_COMB_MIPMAP as step0
    # and use only step 1 of the incoming formula. Unfortunately, this
    # also means that all TEX0 slots must be converted into COMBINED slots.
    # We do this by using the mask already loaded in a2/a3
    lw t0, %lo(RDPQ_COMBINER_MIPMAPMASK) + 0
    lw t1, %lo(RDPQ_COMBINER_MIPMAPMASK) + 4
    and comb_hi, t0
    and comb_lo, t1
    # Since this combiner now requires two-cycle mode, we can simply store in the
    # 2-cycle mode slot. No need to touch the 1-cycle mode slot as it will not
    # be used anyway.
    lw t0, %lo(COMBINER_MIPMAP2) + 0
    lw t1, %lo(COMBINER_MIPMAP2) + 4
    or comb_hi, t0
    j store_comb_2cyc
    or comb_lo, t1

store_comb_1cyc:
    # The combiner settings is 1 pass. Store it as-is for 1cycle mode.
    sw comb_hi, %lo(RDPQ_MODE_COMBINER_1CYC) + 0
    sw comb_lo, %lo(RDPQ_MODE_COMBINER_1CYC) + 4

    # For 2 cycle mode, we need to adjust it changing the second pass
    # to be a pure passthrough. We can do this by simply setting to 0
    # all fields of the second pass, as that corresponds to:
    #   (COMBINED - COMBINED) * COMBINED + COMBINED  =  COMBINED
    lw t0, %lo(COMB0_MASK) + 0
    lw t1, %lo(COMB0_MASK) + 4
    and comb_hi, t0
    and comb_lo, t1

    # Normallly at this point we don't need to set the 2PASS flag, as this
    # combiner does not require 2cycles. The only exception is nearest mipmapping:
    # in this case, we must force 2-cycle mode otherwise the RDP will not switch LOD.
    srl t0, som_hi, SOM_TEXTURE_LOD_SHIFT - 32
    sll t0, 31
    or comb_hi, t0

store_comb_2cyc:
    sw comb_hi, %lo(RDPQ_MODE_COMBINER_2CYC) + 0
    sw comb_lo, %lo(RDPQ_MODE_COMBINER_2CYC) + 4

    ######################################
    #
    # BLENDER STEPS
    #
    ######################################
    #
    # Let's recap the meaning of SOM blending-related flags:
    #
    #   SOM_BLENDING: process al pixels of the triangle through the blending unit
    #   SOM_AA_ENABLE: process edge pixels of the triangle through the blending unit
    #
    # So in general SOM_BLENDING is a superset of SOM_AA_ENABLE.
    # Also notice that in 2cyc mode, SOM_BLENDING/SOM_AA only gate the
    # *second cycle*, as the first cycle is always run for all pixels(!).
    #
    # This is the expected configuration for each combination of blending,
    # fog and AA. Notice that in any case where SOM_BLENDING is set, setting
    # SOM_AA_ENABLE is redundant, but it doesn't hurt.
    #
    # Blending     |   1cyc    | SOM_BLENDING
    # Fog          |   1cyc    | SOM_BLENDING
    # AA           |   1cyc    | SOM_AA_ENABLE
    # Fog+Blending |   2cyc    | SOM_BLENDING
    # Fog+AA       |   2cyc    | SOM_AA_ENABLE
    # Blending+AA  |   1cyc    | SOM_BLENDING (same BL config of "Blending")
    # Fog+Blend+AA |   2cyc    | SOM_BLENDING (same BL config of "Fog+Blending")
    #
    # Our input data:
    #  * RDPQ_MODE_BLENDER_STEPS+0: fog configuration if any, or 0.
    #  * RDPQ_MODE_BLENDER_STEPS+4: blender configuration if any, or 0.
    #  * SOM_AA_ENABLE: turned on if the user requested AA.
    #
    # Notice that the blender steps always include the SOM_BLENDING flag, if
    # they are not zero.

    lw t0, %lo(RDPQ_MODE_BLENDER_STEPS) + 0   # Load step0
    lw t1, %lo(RDPQ_MODE_BLENDER_STEPS) + 4   # Load step1

    # Check if step 1 contains a blending formula (before antialias).
    sne bkg_blending, t1, zero

    # If step 1 is empty, check if antialias is active. If so, we need
    # to merge in a default formula. Moreover, in this case, we don't want
    # or need the SOM_BLENDING anymore (see the table above).
    bnez t1, blender_check_merge
    andi t2, som_lo, SOM_AA_ENABLE
    beqz t2, blender_check_merge
    nop
    #if (SOMX_AA_REDUCED >> 32) != 4
    #error Adjust this if SOMX_AA_REDUCED changes
    #endif
    andi t1, som_hi, SOMX_AA_REDUCED >> 32
    lw t1, %lo(AA_BLEND_DEFAULT_FORMULA)(t1)
    and t0, ~SOM_BLENDING

    # Merge the two blender steps (fogging + blending). If either
    # is not set (0), we just configure the other one as follows:
    #
    # 1cyc: we turn off the second step (and'ing with SOM_BLEND0_MASK).
    #       This is strictly not necessary as the second step is ignored.
    # 2cyc: we change the first step into a passthrough (all values 0),
    #       and keep the formula in the second step.
    #
    # If both steps are configured, we need to merge them: we keep fogging
    # in the first step, and blending in the second. We also set SOMX_BLEND_2PASS
    # to remember that we must force 2cycle mode.
    #
    # We also set the bkg_blending flag to 1 if the step1 formula is configured.
    # This is an assumption documented in rdpq_mode.h: we assume that any step1
    # formula is a background blending formula. This assumption will be used
    # later to configure the antialias, if requested.

    #define blend0_mask t2
    #define blend1_mask t3
blender_check_merge:
    li blend0_mask, SOM_BLEND0_MASK
    li blend1_mask, SOM_BLEND1_MASK
    
    beqz t0, blender_1pass
    move blend_1cyc, t1

    beqz t1, blender_1pass
    move blend_1cyc, t0

blender_2pass:
    and t0, blend0_mask
    and t1, blend1_mask
    or blend_2cyc, t0, t1
    j 1f
    or blend_2cyc, SOMX_BLEND_2PASS
blender_1pass:
    and blend_2cyc, blend_1cyc, blend1_mask
    and blend_1cyc, blend0_mask
1:
    #undef blend0_mask
    #undef blend1_mask

    ######################################
    #
    # 1 CYCLE / 2 CYCLE MODE SELECTION
    #
    ######################################

    # Automatic configuration of 1cycle / 2cycle.
    #
    # Check if either the current blender and combiner configuration require
    # 2cycle mode:
    #   * Blender: bit 15 is set if 2cyc mode is required (SOMX_BLEND_2PASS)
    #   * Combiner: bit 63 is set if 2cyc mode is required (RDPQ_COMBINER_2PASS)
    #
    # First, we align both bits in bit 31 and we OR them together.
    sll t2, blend_2cyc, 16
    lw t1, %lo(RDPQ_MODE_COMBINER_2CYC)  # Fetch high word
    or t1, t2
    # Point to either the 2cyc or 1cyc configuration, depending on what we need
    # to load.
    li s0, %lo(RDPQ_MODE_COMBINER_2CYC)
    bltz t1, set_cycle_type
    li cycle_type, ((SOM_CYCLE_MASK ^ SOM_CYCLE_2) >> 32) | 0x10000000
set_1cyc:
    li s0, %lo(RDPQ_MODE_COMBINER_1CYC)
    move blend_final, blend_1cyc
    li cycle_type, ((SOM_CYCLE_MASK ^ SOM_CYCLE_1) >> 32)  | 0x10000000
set_cycle_type:
    # Set cycle type bits in other modes high word. Also put the correct
    # command (0xEF) in the top byte: we achieve this by first setting the
    # top byte to 0xFF, and then xoring with 0x10 (which is included in
    # cycle_type).
    or som_hi, (SOM_CYCLE_MASK >> 32) | 0xFF000000
    xor som_hi, cycle_type


    ######################################
    #
    # ANTI_ALIASING & COVERAGE CONFIGURATION
    #
    ######################################

    # We need to configure the various mode bits depending
    # on the AA (SOM_AA_ENABLE) and blender-to-background settings (bkg_blending).
    # The bits to set are written in the AA_BLEND_TABLE.
    #
    # bkg_blending is set to 1 iff the blender step1 formula is configured. This
    # is an assumption documented in rdpq_mode: in fact, we need bkg_blending=0
    # when just fogging is enabled (as that doesn't count as background blending),
    # and in that case we need to force a second blender step to do the antialiasing.
    and t0, som_lo, SOM_AA_ENABLE      # Bit 3
    sll t1, bkg_blending, 2            # Bit 2
    or t0, t1
    lw t0, %lo(AA_BLEND_TABLE)(t0)    # Load values to set
    lw t1, %lo(AA_BLEND_MASK)         # Load mask
    or t0, blend_final                # Merge blend_final formula into the coverage bits

    # Apply changes to SOM lower bits. These changes in t0 are the combination
    # of blender settings and coverage bits.
    and t0, t1
    not t1, t1
    and som_lo, t1
    or som_lo, t0

    ######################################
    #
    # AA + ALPHA COMPARE TWEAKING
    #
    ######################################

    # If we use both AA and alpha compare, AA is ineffective because it uses
    # the pixel coverage as blend factor (SOM_BLALPHA_CVG), but that works only
    # on polygon edges (where coverage is not 1.0).
    # With alpha compare, we would like to smooth on the alpha compare edges,
    # not the polygon edges. So we should instead switch to SOM_BLALPHA_CVG_TIMES_CC,
    # so that we modulate the coverage with the actual pixel alpha.
    # Additionally we need to disable the actual alpha compare feature since it would
    # compare the threshold with the alpha multiplied by coverage in this case, which would
    # lead to visible seams at polygon edges.
    li t0, SOM_ALPHACOMPARE_THRESHOLD | SOM_BLALPHA_CVG
    and t1, som_lo, t0
    bne t0, t1, rdpq_update_finish
    nop
    or som_lo, SOM_BLALPHA_CVG_TIMES_CC
    and som_lo, ~SOM_ALPHACOMPARE_MASK
    
    ######################################
    #
    # SAVE SETTINGS & APPLY TO RDP
    #
    ######################################

rdpq_update_finish:
    lw comb_hi, 0(s0)
    lw comb_lo, 4(s0)

    # Set correct SET_COMBINE opcode (0xFC). The opcode can be anything of
    # the other 4 (1pass/2pass dynamic/static).
    or comb_hi, 0xFF000000
    xor comb_hi, 0xFF000000 ^ 0xFC000000

    # Store calculated SOM into RDPQ_OTHER_MODES for debugging purposes
    # (to implemented rdpq_get_other_modes_raw). Notice that we don't
    # overwrite the MSB with 0xEF: it contains extended flags tha we don't
    # want to lose
    lbu t0, %lo(RDPQ_OTHER_MODES) + 0
    sw som_hi, %lo(RDPQ_OTHER_MODES) + 0
    sw som_lo, %lo(RDPQ_OTHER_MODES) + 4
    sb t0, %lo(RDPQ_OTHER_MODES) + 0

    jal_and_j RDPQ_Write16, RDPQ_Finalize

rdpq_update_fillcopy:
    # We are in copy/fill mode. It is sufficient to emit a SET_OTHER_MODES
    # to configure it.
    or a0, som_hi, 0xFF000000
    xor a0, 0xFF000000 ^ 0xEF000000
    move a1, som_lo
    jal_and_j RDPQ_Write8, RDPQ_Finalize

    .endfunc

    #undef comb_hi
    #undef comb_lo
    #undef som_hi
    #undef som_lo
    #undef comb_hi_noid
    #undef blend_1cyc
    #undef blend_2cyc
    #undef blend_final
    #undef passhthrough
    #undef cycle_type


    .section .text.rdpq_scissor

    #############################################################
    # RDPQ_WriteSetScissor
    #
    # Given a SET_SCISSOR command in a0/a1, writes it into RDPQ_SCISSOR_RECT
    # as-is (exclusive), and then sends it to RDP after optionally adjusting
    # the extents to match the current SOM cycle type.
    #############################################################
    .func RDPQ_WriteSetScissor
RDPQ_WriteSetScissor:
    sw a0, %lo(RDPQ_SCISSOR_RECT) + 0x0
    lb t6, %lo(RDPQ_OTHER_MODES) + 0x1
    # Bit 21 of the first word is set if FILL or COPY mode is active
    andi t6, 0x1 << 5
    # Leave unchanged when not in FILL or COPY mode
    beqz t6, 1f
    sw a1, %lo(RDPQ_SCISSOR_RECT) + 0x4

    # Subtract 1 subpixel from XL (bits 23:12, as 10.2 fixed point)
    addiu a1, -(1 << 12)

1:
    j RDPQ_Write8
    nop
    .endfunc


    .section .text.rdpq_fillcolor

    #############################################################
    # RDPQ_WriteSetFillColor
    # 
    # Given a 32-bit RGBA color in a1, writes it into RDPQ_FILL_COLOR
    # as-is, and then sends it to RDP after optionally converting it
    # into 16-bit, depending on the current target bitdepth.
    #############################################################
    .func RDPQ_WriteSetFillColor
RDPQ_WriteSetFillColor:
    sw a1, %lo(RDPQ_FILL_COLOR)
    lbu t0, %lo(RDPQ_TARGET_BITDEPTH)
    beq t0, 3, RDPQ_Write8
    lui a0, 0xF700   # SET_FILL_COLOR
    srl t0, a1, 24 + (8-5) - 11
    srl t1, a1, 16 + (8-5) - 6
    srl t2, a1, 8  + (8-5) - 1
    srl t3, a1, 0  + (8-1) - 0
    andi t0, 0x1F << 11
    andi t1, 0x1F << 6
    andi t2, 0x1F << 1
    andi t3, 0x01 << 0
    or t4, t0, t1
    or t5, t2, t3
    or a1, t4, t5
    sll t0, a1, 16
    j RDPQ_Write8
    or a1, t0
    .endfunc

#########################################################################
#
#  RDPQ_Triangle_Send_Async: assemble a RDP triangle command (optimized version)
#
##########################################################################

#include "rsp_rdpq_tri.inc"

#########################################################################
#
#  RDPQ_Triangle: assemble a RDP triangle command
#
##########################################################################

    .section .data.rdpq_triangle
    .align 4
TRICONST1: .half 0xFFFF,0,0,0,0x200,0x200,0x200,0x200


    .section .text.rdpq_triangle

    #####################################################################
    # RDPQ_Triangle
    #
    # INPUT:
    # * a0: high 32-bit word of the triangle command. This will be
    #       completed with the left/right flag and the mipmap level.
    # * a1,a2,a3: pointer to the triangle structures in DMEM
    # * v0: 0=cull front, 1=cull back, any other value = culling disabled
    # * s3: output buffer pointer
    #####################################################################

    # Implementation limits of the RSP version. These are all edge cases that are probably
    # not necessary to get 100% right as they are really degenerate situations. Notice that
    # most extreme/degenerated/saturated cases are still handled correctly, as verified
    # by the fuzzing performed by test_rdpq_triangle; these are just the three leftovers.
    #
    # * Texture coordinates are accepted in s10.5 format, but a subtraction between two of them
    #   must not overflow a 16-bit number. This is a limit of the attribute calculation where the
    #   edges MA/HA are calculated with 16-bit numbers. It looks like it's not a real problem as
    #   it would mean having a triangle where either S or T spans more than 1024 texels within it.
    #   Fixing it wuold require changing MA/HA into 32-bit numbers, which has other annoying fallouts.
    # * In case of quasi-degenerate triangles (lines), when the Y difference between two vertices
    #   is just 0.25 (0x00000001), the correct normal reciprocal would be 1.0, but it is calculated
    #   as 0x7FFF8000 which is 0.5 (because it's basically saturating s15.16). This means that the calculated
    #   edge is twice as big. Again, it doesn't matter as it can't really be seen within a 0.25 slope.
    #   test_rdpq_triangle has a triangle that triggers this, commented out.
    # * In some cases, Z/W-related derivates (DwDx, DwDy, DzDx, DzDy) can saturate during calculation.
    #   in this case, the dependent D*De derivates will be wrong (how much it will depend on how far
    #   the real result is from the saturated number). In any case, much better than an overflow.
    #   test_rdpq_triangle checks if there's a saturation and skip checks for known-wrong values.

    .func RDPQ_Triangle
RDPQ_Triangle:
    #define tricmd a0
    #define vtx1   a1
    #define vtx2   a2
    #define vtx3   a3
    #define cull   v0

    #define y1     t4
    #define y2     t5
    #define y3     t6
    #define x1     t7
    #define x2     t8
    #define x3     v0

    # r, g, b, a, s, t, w, z
    #define vfinal_i         $v01
    #define vfinal_f         $v02
    #define vdx_i            $v03
    #define vdx_f            $v04
    #define vde_i            $v05
    #define vde_f            $v06
    #define vdy_i            $v07
    #define vdy_f            $v08

    #define vattr1           $v09
    #define vattr2           $v10
    #define vattr3           $v11
    #define vma              $v12
    #define vha              $v13

    #define vinvw_i          $v14
    #define vinvw_f          $v15

    #define vedges_i         $v16
    #define vedges_f         $v17
    #define vnz_i            $v18
    #define vnz_f            $v19
    #define vslope_i         $v20
    #define vslope_f         $v21
    #define vxy32            $v22
    #define vxy21            $v23
    #define vhml             $v24
    #define vfy_i            $v25
    #define vfy_f            $v26

    #define vtmp             $v28
    #define v__              $v29
    #define invn_i           $v31.e4
    #define invn_f           $v31.e5
    #define invsh_i          $v31.e6
    #define invsh_f          $v31.e7

    #define VTX_ATTR_X      0
    #define VTX_ATTR_Y      2
    #define VTX_ATTR_Z      4
    #define VTX_ATTR_CLIPCODE 6         // Clipping codes
    #define VTX_ATTR_TRCODE   7         // Negated trivial-reject codes
    #define VTX_ATTR_RGBA   8
    #define VTX_ATTR_S      12
    #define VTX_ATTR_T      14
    #define VTX_ATTR_W      16
    #define VTX_ATTR_INVWi  20
    #define VTX_ATTR_INVWf  22

#if 0
    mfc0 fp, COP0_DP_CLOCK
    emux_trace_start
#endif

    j half_swap
    li t0, 1

swap_loop:
    lh y2, VTX_ATTR_Y(vtx2)
    lh y3, VTX_ATTR_Y(vtx3)
    blt y2, y3, half_swap
    move t1, vtx2
    move vtx2, vtx3
    move vtx3, t1
    xor cull, 1

half_swap:
    lh y1, VTX_ATTR_Y(vtx1)
    lh y2, VTX_ATTR_Y(vtx2)
    blt y1, y2, swap_end
    move t1, vtx1
    move vtx1, vtx2
    move vtx2, t1
    xor cull, 1

swap_end:
    bnez t0, swap_loop
    addi t0, -1

    # We want to build this layout
    #  vxy32 = X3 X2 X3 --    Y3 Y2 Y3 --
    #  vxy21 = X1 -- X2 --    Y1 -- Y2 --

    lsv vxy32.e0, VTX_ATTR_X,vtx3
    lsv vxy32.e4, VTX_ATTR_Y,vtx3
    vor vxy32, vzero, vxy32.h0
    lsv vxy32.e1, VTX_ATTR_X,vtx2
    lsv vxy32.e5, VTX_ATTR_Y,vtx2

    lsv vxy21.e0, VTX_ATTR_X,vtx1
    lsv vxy21.e2, VTX_ATTR_X,vtx2
    lsv vxy21.e4, VTX_ATTR_Y,vtx1
    lsv vxy21.e6, VTX_ATTR_Y,vtx2

    # Store Y values in output
    ssv vxy21.e4, 6,s3    # y1
    ssv vxy32.e5, 4,s3    # y2
    ssv vxy32.e4, 2,s3    # y3

    # Add num mipmap levels to tricmd
    lbu t1, %lo(RDPQ_OTHER_MODES) + 0
    andi t1, 0x38     # Isolate bits 2-5 (aka 59-61 of SOM)
    or tricmd, t1

    # Now calculate:
    #  vxy32    = X3 X2 X3 --    Y3 Y2 Y3 --
    #    -
    #  vxy21.0q = X1 X1 X2 X2    Y1 Y1 Y2 Y2
    #    = 
    #  vhml     = HX MX LX --    HY MY LY --
    vsubc vhml, vxy32, vxy21.q0
    #define hx  vhml.e0
    #define mx  vhml.e1
    #define lx  vhml.e2
    #define my1 vhml.e3
    #define hy  vhml.e4
    #define my  vhml.e5
    #define ly  vhml.e6
    #define mx1 vhml.e7

    # Duplicate MX and MY into the two empty lanes.
    #  vhml     = HX MX LX MY    HY MY LY MX
    vmov mx1, mx
    vmov my1, my

    # Calculate normal: compute 32-bit cross product:
    #
    #  vhml     = HX MX LX MY    HY MY LY MX
    #    *
    #  vhml.3h  = MY MY MY MY    MX MX MX MX
    #    =
    #  nz       = HX*MY -- -- --    HY*MX -- -- -- --
    vmudh vnz_f, vhml, vhml.h3
    vsar vnz_i, COP2_ACC_HI
    vsar vnz_f, COP2_ACC_MD

    # Compute HY*MX - HX*MY. Result in e4.
    vsubc vnz_f, vnz_f.e0
    vsub  vnz_i, vnz_i.e0

    # Extract left flag from the sign of NZ.
    # Since we calculated -NZ, we need to reverse the sign
    mfc2 t0, vnz_i.e4
    sge t0, t0, zero
    beq t0, cull, JrRa
    sll t0, 7
    or tricmd, t0

    # Calculate reciprocal of normal
    vrcph vnz_i.e0, vnz_i.e4
    vrcpl vnz_f.e0, vnz_f.e4
    vrcph vnz_i.e0, v__.e0
    #define inz_f   vnz_f.e0
    #define inz_i   vnz_i.e0

    # Compute SLOPE vector
    # slope    =  -- -- -- --    1/HY 1/MY 1/LY 1/NZ

    # Compute ISL (L slope). 1/LY  (s14.1)
    vrcp  vslope_f.e6, vhml.e6
    vrcph vslope_i.e6, vhml.e6
    # Compute ISM (M slope). 1/MY  (s14.1)
    vrcp  vslope_f.e5, vhml.e5
    vrcph vslope_i.e5, vhml.e5
    # Compute ISH (H slope). 1/HY  (s14.1)
    vrcp  vslope_f.e4, vhml.e4
    vrcph vslope_i.e4, vhml.e4

    ##################
    # 1 NR pass
    ##################
    vmov vslope_f.e7, inz_f
    vmov vslope_i.e7, inz_i

    # Adjust multiplying by 2 (required after reciprocal)
    #vmudn vslope_f, vslope_f, K2
    #vmadh vslope_i, vslope_i, K2
    vaddc vslope_f, vslope_f
    vadd  vslope_i, vslope_i

    # Prepare 32-bit number containing the source of the reciprocal
    # Notice that we're calculating NR over 1 32-bit input (NZ) and
    # 3 16-bit inputs (HY, MY, LY), for which we provide 0 in the lower
    # part.
    #    vhml      =   HX MX LX MY    HY   MY   LY   NZf
    #    v__       =    0 0  0  0     0    0    0    NZi
    vxor v__, v__
    vmov v__.e7,  vnz_i.e4
    vmov vhml.e7, vnz_f.e4

    #define vtmp_f  vattr1
    #define vtmp_i  vattr2
    #define vk2     vattr3

    # NR: R*X
    vmudl vtmp_f, vslope_f, vhml
    vmadm vtmp_f, vslope_i, vhml
    vmadn vtmp_f, vslope_f, v__
    vmadh vtmp_i, vslope_i, v__

    # NR: 2 - R*X   
    vor vk2, vzero, K2
    vsubc vtmp_f, vzero, vtmp_f
    vsub  vtmp_i, vk2,   vtmp_i

    # NR: X * (2 - R*X)
    vmudl vk2, vtmp_f, vslope_f
    vmadm vk2, vtmp_i, vslope_f
    vmadn vslope_f, vtmp_f, vslope_i
    vmadh vslope_i, vtmp_i, vslope_i
    #vmadn vslope_f, vzero, vzero     # re-read vslope_f in case of overflow

    # vhml      =   HX MX LX MY    HY   MY   LY   NZf
    # v__       =    0 0  0  0     0    0    0    NZi
    # slope     =   -- -- -- --   1/HY 1/MY 1/LY  1/NZ
    
    vmov vnz_f.e0, vslope_f.e7
    vmov vnz_i.e0, vslope_i.e7

    # Rotate slope
    # slope     =   1/HY 1/MY 1/LY 1/NZ   -- -- -- --
    sqv vslope_f.e4, 0x10,s3
    lqv vslope_f.e0  0x10,s3
    sqv vslope_i.e4, 0x10,s3
    lqv vslope_i.e0  0x10,s3

    # Shift left NZ (that contains INVNZ) by 2, to align with the fixed point precision
    # that will be required later.
    vmudn vnz_f, vnz_f, K4
    vmadh vnz_i, vnz_i, K4

    # FY.e4 = fy (s15.16)
    vsll8 vfy_f, vxy21, 14
    vsra  vfy_i, vxy21, 2
    # FY.e4 = floorf(y1) - y1
    # TODO: this is always a negative fraction, so fy_i is always 0xFFFF (or fy_i=fy_f=0).
    # See if we can take advantage of this somehow to simplify later.
    vsubc vfy_f, vzero, vfy_f
    vsub  vfy_i, vfy_i

    # Finalize slope divisions by multiplying by the reciprocal.
    #  vhml     =   HX    MX    LX    1     HY MY LY MX
    #    *
    #  slope    =  1/HY  1/MY  1/LY  1/NZ   -- -- -- --
    #    =
    #  slope    =  HX/HY MX/MY LX/LY  --    -- -- -- --
    vmudn v__,     vslope_f, vhml
    vmadh v__,     vslope_i, vhml
    vsar  vslope_f, COP2_ACC_MD
    vsar  vslope_i, COP2_ACC_HI

    #define ish_f   vslope_f.e0
    #define ish_i   vslope_i.e0
    #define ism_f   vslope_f.e1
    #define ism_i   vslope_i.e1
    #define isl_f   vslope_f.e2
    #define isl_i   vslope_i.e2

    # Store slopes
    ssv isl_f, 14,s3
    ssv isl_i, 12,s3
    ssv ism_f, 30,s3
    ssv ism_i, 28,s3
    ssv ish_f, 22,s3
    ssv ish_i, 20,s3

    #  vxy21 =   X1   --    X2   --  Y1 -- Y2 --
    #  slope = HX/HY MX/MY LX/LY --  -- -- -- --

    # FINAL = X1/X2 in 16.16 precision
    # TODO: maybe fold into the next MAC sequence?
    vsra  vfinal_i, vxy21, 2
    vsll8 vfinal_f, vxy21, 14

    # Store X2 value in output (as XL)
    ssv vfinal_f.e2, 10,s3  # XL_F
    ssv vfinal_i.e2,  8,s3  # Xl_I

    # Compute XH/XM
    # TODO: fy_i is always 0xFFFFFFFF here. See if we can benefit from this.
    vmudl v__,      vslope_f, vfy_f.e4
    vmadm v__,      vslope_i, vfy_f.e4
    vmadn vedges_f, vslope_f, vfy_i.e4
    vmadh vedges_i, vslope_i, vfy_i.e4

    vaddc vedges_f, vfinal_f.q0
    vadd  vedges_i, vfinal_i.q0

    ssv vedges_f.e1, 26,s3  # XM_F
    ssv vedges_i.e1, 24,s3  # XM_I
    ssv vedges_f.e0, 18,s3  # XH_F
    ssv vedges_i.e0, 16,s3  # XH_I

    sh tricmd, 0(s3)
    add s3, 32

    # Load attributes into ATTR registers.
    # TODO: we can interleave these in all the code above, and at that point
    # it's useless to test for tricmd to save loads. Just load them all.

    #define attr1_r     vattr1.e0
    #define attr2_r     vattr2.e0
    #define attr3_r     vattr3.e0
    #define attr1_s     vattr1.e4
    #define attr2_s     vattr2.e4
    #define attr3_s     vattr3.e4
    #define attr1_invw  vattr1.e6
    #define attr2_invw  vattr2.e6
    #define attr3_invw  vattr3.e6
    #define attr1_z     vattr1.e7
    #define attr2_z     vattr2.e7
    #define attr3_z     vattr3.e7
    luv attr1_r, VTX_ATTR_RGBA,vtx1 # RGBA
    luv attr2_r, VTX_ATTR_RGBA,vtx2
    luv attr3_r, VTX_ATTR_RGBA,vtx3

    llv attr1_s, VTX_ATTR_S,vtx1  # S & T
    llv attr2_s, VTX_ATTR_S,vtx2
    llv attr3_s, VTX_ATTR_S,vtx3

    # We need to normalize INV_W in [0..1], by dividing them by the maximum INV_W.
    # We will multiply by W instead, and thus we search for the minimum W.
    lw t0, VTX_ATTR_W(vtx1)
    lw t1, VTX_ATTR_W(vtx2)
    blt t0, t1, 1f
    lw t2, VTX_ATTR_W(vtx3)
    move t0, t1
1:
    blt t0, t2, 1f
    nop
    move t0, t2
1:
    mtc2 t0, vinvw_f.e0
    srl t0, 16
    mtc2 t0, vinvw_i.e0

    lsv vinvw_i.e4, VTX_ATTR_INVWi,vtx1
    lsv vinvw_i.e5, VTX_ATTR_INVWi,vtx2
    lsv vinvw_i.e6, VTX_ATTR_INVWi,vtx3

    lsv vinvw_f.e4, VTX_ATTR_INVWf,vtx1
    lsv vinvw_f.e5, VTX_ATTR_INVWf,vtx2
    lsv vinvw_f.e6, VTX_ATTR_INVWf,vtx3

    #define K_FFFF     vtmp.e0

    li s0, %lo(TRICONST1)+8
    lsv K_FFFF, -8,s0

    # invw:     minw -- -- --            invw1 invw2 invw3 --
    #
    # We need to multiply minw with the three invw. All numbers are positive s16.16,
    # and the result is known to fit 0..1. By doing a standard 32-bit multiplication
    # on RSP, we end up with a positive s16.16 number, where the integer word is zero.
    # In fact, in theory W * 1/W = 1, but both numbers are likely missing enough bits
    # of precision that the result will always be slightly lower than 1 (and thus the
    # integer part will be 0).
    vmudl v__,     vinvw_f, vinvw_f.e0
    vmadm v__,     vinvw_i, vinvw_f.e0
    vmadn vinvw_f, vinvw_f, vinvw_i.e0
    vmadh vinvw_i, vinvw_i, vinvw_i.e0
    
    # So now vinvw_i should be 0 (in lanes 4..6). It turns out there is one exception:
    # minw == invw == 1.0. In that case, the result will be exactly 1, and thus
    # vinvw_i will be 1. Since we want to simplify further calculations and avoid
    # taking vinvw_i into account, we want to replace 0x1_0000 with 0x0_FFFF.
    # Do a manual saturation: vinvw_f = (vinvw_i == 0 ? vinvw_f : 0xFFFF)
    veq  vinvw_i, vzero
    vmrg vinvw_f, K_FFFF

    # Load 0x200 in the first 4 lanes of the vector, using a misaliged lqv.
    # 0x200 is the constant that can be used to >>7, which will be used for
    # the RGBA components.
    #
    # invw:  0x200 0x200 0x200 0x200   invw1 invw2 invw3 --
    lqv vinvw_f, 0,s0

    vmudm vattr1, vinvw_f.h0
    vmudm vattr2, vinvw_f.h1
    vmudm vattr3, vinvw_f.h2

    # Change inv_w from 0.16 to s0.15 by shifting by one
    vsrl vinvw_f, vinvw_f, 1

    # Copy inv_w components into ATTRn
    vmov vattr1.e6, vinvw_f.e4
    vmov vattr2.e6, vinvw_f.e5
    vmov vattr3.e6, vinvw_f.e6

    lsv attr1_z, VTX_ATTR_Z,vtx1  # Load Z
    lsv attr2_z, VTX_ATTR_Z,vtx2
    lsv attr3_z, VTX_ATTR_Z,vtx3

    ########################################################
    # ATTRIBUTES
    ########################################################
calc_attrs:
    # MA = A2 - A1
    # HA = A3 - A1
    # NOTE: S/T coordinates are kept as s10.5, so they can overflow here.
    # The subtraction is saturated so the error is minimized, but it is 
    # indeed there. To fix this, we would have to produce a 32-bit result here
    # and then change the DX/DY calculations to use 32-bit numbers as well.
    # Note also that we need "vsubc zero,zero" to clear the VCC (carry) bit
    # which vsub reads as input.
    vsubc vzero, vzero
    vsub vma, vattr2, vattr1
    vsub vha, vattr3, vattr1

    #  vhml     = HX MX LX MY1    HY MY LY MX1

    # TODO: find other strategies to negate MY and HX?
    # Or maybe this is good as we can probably interleave it, being scalar ops.
    # TODO: or we could also compute -MA / -HA. But that's even more vector ops.
    mfc2 t0, my
    mfc2 t1, hx
    neg t0
    neg t1
    mtc2 t0, my
    mtc2 t1, hx

    # DX = MA * HY - HA * MY
    vmudh vdx_f, vma, hy
    vmadh vdx_f, vha, my
    vsar vdx_f, COP2_ACC_MD
    vsar vdx_i, COP2_ACC_HI

    # DY = HA * MX - MA * HX
    vmudh vdy_f, vha, mx
    vmadh vdy_f, vma, hx
    vsar vdy_f, COP2_ACC_MD
    vsar vdy_i, COP2_ACC_HI

    # DX * 1/N (TODO: check if we can pre-multiply edges to avoid this)
    vmudl v__,  vdx_f, inz_f
    vmadm v__,  vdx_i, inz_f
    vmadn vdx_f, vdx_f, inz_i
    vmadh vdx_i, vdx_i, inz_i

    # DY * 1/N (TODO: check if we can pre-multiply edges to avoid this)
    vmudl v__,  vdy_f, inz_f
    vmadm v__,  vdy_i, inz_f
    vmadn vdy_f, vdy_f, inz_i
    vmadh vdy_i, vdy_i, inz_i

    # DE = DX * invsh + DY
    vmadl v__,  vdx_f, ish_f
    vmadm v__,  vdx_i, ish_f
    vmadn vde_f, vdx_f, ish_i
    vmadh vde_i, vdx_i, ish_i

    # FINAL = vATTR1 + DE * FY
    # TODO: fy_i is always 0xFFFFFFFF here. See if we can benefit from this.
    # TODO: actually, it can also be fy_i = fy_f = 0.
    vmudl v__,      vde_f, vfy_f.e4
    vmadm v__,      vde_i, vfy_f.e4
    vmadn vfinal_f, vde_f, vfy_i.e4
    vmadh vfinal_i, vde_i, vfy_i.e4
    vmadh vfinal_i, vattr1, K1

    andi t0, tricmd, 0x400
    beqz t0, no_color

    # Store color
    sdv vfinal_i.e0, 0x00,s3
    sdv    vdx_i.e0, 0x08,s3
    sdv vfinal_f.e0, 0x10,s3
    sdv    vdx_f.e0, 0x18,s3
    sdv    vde_i.e0, 0x20,s3
    sdv    vdy_i.e0, 0x28,s3
    sdv    vde_f.e0, 0x30,s3
    sdv    vdy_f.e0, 0x38,s3
    addi s3, 0x40

no_color:
    andi t0, tricmd, 0x200
    beqz t0, no_texture

    # Store texture
    sdv vfinal_i.e4, 0x00,s3
    sdv    vdx_i.e4, 0x08,s3
    sdv vfinal_f.e4, 0x10,s3
    sdv    vdx_f.e4, 0x18,s3
    sdv    vde_i.e4, 0x20,s3
    sdv    vdy_i.e4, 0x28,s3
    sdv    vde_f.e4, 0x30,s3
    sdv    vdy_f.e4, 0x38,s3
    addi s3, 0x40

no_texture:
    andi t0, tricmd, 0x100
    beqz t0, JrRa

    # Store z
    ssv vfinal_i.e7, 0x00,s3
    ssv vfinal_f.e7, 0x02,s3
    ssv    vdx_i.e7, 0x04,s3
    ssv    vdx_f.e7, 0x06,s3
    ssv    vde_i.e7, 0x08,s3
    ssv    vde_f.e7, 0x0A,s3
    ssv    vdy_i.e7, 0x0C,s3
    ssv    vdy_f.e7, 0x0E,s3
    addi s3, 0x10

#if 0
    emux_trace_stop

    mfc0 t0, COP0_DP_CLOCK
    sub t0, t0, fp
    emux_log_string "RDPQ_Triangle:  "
    emux_dump_gpr t0
#endif

    jr ra
    nop

    #undef tricm
    #undef vtx1 
    #undef vtx2 
    #undef vtx3 
    #undef cull 

    #undef y1   
    #undef y2   
    #undef y3   
    #undef x1   
    #undef x2   
    #undef x3   

    # r, g, b, a, s, t, w, z
    #undef vfinal_i         
    #undef vfinal_f         
    #undef vdx_i            
    #undef vdx_f            
    #undef vde_i            
    #undef vde_f            
    #undef vdy_i            
    #undef vdy_f            

    #undef vattr1           
    #undef vattr2           
    #undef vattr3           
    #undef vma              
    #undef vha              

    #undef vinvw_i          
    #undef vinvw_f          

    #undef vedges_i         
    #undef vedges_f         
    #undef vnz_i            
    #undef vnz_f            
    #undef vslope_i         
    #undef vslope_f         
    #undef vxy32            
    #undef vxy21            
    #undef vhml             
    #undef vfy_i            
    #undef vfy_f            

    #undef vtmp
    #undef v__         
    #undef invn_i      
    #undef invn_f      
    #undef invsh_i     
    #undef invsh_f     

    #undef VTX_ATTR_X   
    #undef VTX_ATTR_Y   
    #undef VTX_ATTR_Z   
    #undef VTX_ATTR_RGBA
    #undef VTX_ATTR_S    
    #undef VTX_ATTR_T    
    #undef VTX_ATTR_W    
    #undef VTX_ATTR_INVWi
    #undef VTX_ATTR_INVWf

    .endfunc

    .section .text
