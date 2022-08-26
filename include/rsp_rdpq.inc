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

    .data

AA_BLEND_MASK:
    #  MASK
    .word SOM_COVERAGE_DEST_MASK | SOM_BLEND_MASK | SOM_BLALPHA_MASK | SOM_COLOR_ON_CVG_OVERFLOW
AA_BLEND_TABLE:
    #  AA=0 / BLEND=0
    .word SOM_COVERAGE_DEST_ZAP
    #  AA=0 / BLEND=1
    .word SOM_COVERAGE_DEST_ZAP
    #  AA=1 / BLEND=0
    .word SOM_BLALPHA_CVG | SOM_COVERAGE_DEST_CLAMP | \
          RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, MEMORY_CVG))  # Includes SOM_READ_ENABLE
    #  AA=1 / BLEND=1
    .word SOM_COLOR_ON_CVG_OVERFLOW | SOM_COVERAGE_DEST_WRAP


#define RDPQ_COMB_MIPMAP2       RDPQ_COMBINER2((TEX1, TEX0, LOD_FRAC, TEX0), (TEX1, TEX0, LOD_FRAC, TEX0), (0,0,0,0), (0,0,0,0))
#define RDPQ_COMB_SHADE_FOG     RDPQ_COMBINER1((0,0,0,SHADE),      (0,0,0,1))
#define RDPQ_COMB_TEX_SHADE_FOG RDPQ_COMBINER1((TEX0,0,SHADE,0),   (0,0,0,TEX0))

COMB0_MASK:             .quad RDPQ_COMB0_MASK
COMBINER_SHADE:         .quad RDPQ_COMBINER_SHADE
COMBINER_SHADE_FOG:     .quad RDPQ_COMB_SHADE_FOG
COMBINER_TEX_SHADE:     .quad RDPQ_COMBINER_TEX_SHADE
COMBINER_TEX_SHADE_FOG: .quad RDPQ_COMB_TEX_SHADE_FOG

COMBINER_MIPMAP2:       .quad (RDPQ_COMB_MIPMAP2 & RDPQ_COMB0_MASK) | RDPQ_COMBINER_2PASS


# TODO: get rid of the constant offset
RDPQ_CMD_PTR:           .word RDPQ_CMD_STAGING - 0x04000000

    .bss

    .align 4
    # Enough for a full triangle command
RDPQ_CMD_STAGING:       .ds.b 0xB0

    .text

    #############################################################
    # RDPQ_Finalize
    #
    # Submits the RDP command(s) in RDPQ_CMD_STAGING to the RDP.
    #############################################################
    .func RDPQ_Finalize
RDPQ_Finalize:
    li s4, %lo(RDPQ_CMD_STAGING)
    lw s3, %lo(RDPQ_CMD_PTR)
    j RSPQ_RdpSend
    sw s4, %lo(RDPQ_CMD_PTR)
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

    #############################################################
    # RDPQCmd_ModifyOtherModes
    #
    # Modifies a specific part of the other modes and sends the updated
    # value to the RDP.
    # This function can be used as a standard fixup (in which case,
    # it will potential emit a SET_SCISSOR in case the cycle type
    # changed between COPY/FILL and 1CYC/2CYC), or as a mode fixup,
    # part of the mode API, in which case it must call RDPQ_UpdateRenderMode
    # to regenerate the new render mode.
    #
    # NOTE: The new value in a2 should never have bits set outside of
    #       the inverse bitmask to ensure correct results.
    #
    # ARGS:
    #   a0: Bit 24..31: Command id
    #       Bit 15:     If 1, call RDPQ_UpdateRenderMode.
    #       Bit 12-0:   Word offset into other modes (0 or 4)
    #   a1: Inverse bit mask of the value to be written
    #   a2: New value
    #############################################################
    .func RDPQCmd_ModifyOtherModes
RDPQCmd_ModifyOtherModes:
    lw t1, %lo(RDPQ_OTHER_MODES)(a0)
    and t1, a1
    or t1, a2
    sw t1, %lo(RDPQ_OTHER_MODES)(a0)
    sll a0, 16
    bltz a0, RDPQ_UpdateRenderMode

    # Prepare the updated command in a0-a1 for the following steps
    lw a0, %lo(RDPQ_OTHER_MODES) + 0x0
    lw a1, %lo(RDPQ_OTHER_MODES) + 0x4
    # fallthrough
    .endfunc

    #############################################################
    # RDPQ_WriteOtherModes
    #
    # Appends the other modes command in a0-a1 to the staging area
    # and falls through to #RDPQ_FinalizeOtherModes.
    #############################################################
    .func RDPQ_WriteOtherModes
RDPQ_WriteOtherModes:
    # Write other modes command to staging area
    jal RDPQ_Write8
    nop
    # fallthrough!
    .endfunc

    #############################################################
    # RDPQ_FinalizeOtherModes
    #
    # Re-evaluates any commands that depend on the other modes,
    # appends them to the staging area, and finally calls #RDPQ_Finalize,
    # finishing the current command.
    #############################################################
    .func RDPQ_FinalizeOtherModes
RDPQ_FinalizeOtherModes:
    # Update other commands that need to change some state depending on the other modes

    # SetScissor:
    # load the cached command first
    lw a0, %lo(RDPQ_SCISSOR_RECT) + 0x0
    lw a1, %lo(RDPQ_SCISSOR_RECT) + 0x4
    # Append the fixed up SetScissor command to staging area and then finalize
    jal_and_j RDPQ_WriteSetScissor, RDPQ_Finalize
    .endfunc

    #############################################################
    # RDPQCmd_SetFillColor32
    #
    # The RDP command SetFillColor expects a 32-bit value which
    # is a "packed color", that is the 32-bit value that must be
    # blindly repeated in the framebuffer. Semantically, for 32-bit
    # framebuffers, this is the standard RGBA8888 format. For 16-bit
    # framebuffers, it must be RGBA5551 repeated two times.
    #
    # To allow a more flexible approach where the same fill color
    # command can be used irrespective of the target framebuffer, 
    # we create our own SetFillColor32 that only accepts a 
    # RGBA8888 color but convert it automatically to RGBA5551
    # depending on the target bitdepth (using the last value stored
    # by SetColorImage).
    #############################################################
    .func RDPQCmd_SetFillColor32
RDPQCmd_SetFillColor32:
    sw a1, %lo(RDPQ_FILL_COLOR)
    li ra, %lo(RDPQ_Finalize)

RDPQ_WriteSetFillColor:
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

    #############################################################
    # RDPQCmd_SetScissorEx
    #
    # The RDP command SetScissor has slightly different behavior
    # for rectangles depending on the current cycle mode. In 1cycle/2cycle 
    # mode the right edge works as an "exclusive" range, meaning
    # that pixels with the same x-coordinate as the edge are not
    # rendered to. In fill/copy mode on the other hand, it's an inclusive
    # range, so one additional column of pixels is included.
    #
    # To make the API more consistent across all cycle modes, this
    # command will adjust the coordinate of the right edge to compensate
    # for this behavior depending on the current cycle mode.
    #############################################################
    .func RDPQCmd_SetScissorEx
RDPQCmd_SetScissorEx:
    lui t1, 0xED00 ^ 0xD200 # SetScissorEx -> SET_SCISSOR
    xor a0, t1
    li ra, %lo(RDPQ_Finalize)

    # Given a SET_SCISSOR command in a0/a1, write it into RDPQ_SCISSOR_RECT
    # as-is (exclusive), and then send it to RDP after optionally adjusting
    # the extents to match the current SOM cycle type.
    # Returns to caller.
RDPQ_WriteSetScissor:
    sw a0, %lo(RDPQ_SCISSOR_RECT) + 0x0
    lb t6, %lo(RDPQ_OTHER_MODES) + 0x1
    # Bit 21 of the first word is set if FILL or COPY mode is active
    andi t6, 0x1 << 5
    # Leave unchanged when not in FILL or COPY mode
    beqz t6, scissor_substitute
    sw a1, %lo(RDPQ_SCISSOR_RECT) + 0x4

    # Subtract 1 subpixel from XL (bits 23:12, as 10.2 fixed point)
    addiu a1, -(1 << 12)

scissor_substitute:
    j RDPQ_Write8
    nop
    .endfunc

    ###############################################################
    # 
    #
    ###############################################################
    .func RDPQCmd_SetBlendingMode
RDPQCmd_SetBlendingMode:
    j RDPQ_UpdateCombiner
    sw a1, %lo(RDPQ_MODE_BLENDER_STEPS)(a0)
    # fallthrough
    .endfunc

    .func RDPQCmd_SetCombineMode_1Pass
RDPQCmd_SetCombineMode_1Pass:
    # Turn off RDPQ_COMBINER_2PASS (bit 63). This is set by default
    # because the overlay is regisred in slots 0xC0-0xF0.
    # We need to remember that this combiner does not require 2 passes
    xor a0, RDPQ_COMBINER_2PASS >> 32
    # fallthrough!
    .endfunc

    .func RDPQCmd_SetCombineMode_2Pass
RDPQCmd_SetCombineMode_2Pass:
    # Set correct SET_COMBINE opcode (0xFC). The opcode can be anything of
    # the other 4 (1pass/2pass dynamic/static).
    or a0, 0x7F000000
    xor a0, 0x7F000000 ^ 0x7C000000
    # Save the input combiner
    sw a0, %lo(RDPQ_COMB_BASE) + 0
    sw a1, %lo(RDPQ_COMB_BASE) + 4
    sw a2, %lo(RDPQ_COMB_BASE_MIPMAPMASK) + 0
    sw a3, %lo(RDPQ_COMB_BASE_MIPMAPMASK) + 4
    # fallthrough!
    .endfunc

    ###########################################################
    # RDPQ_UpdateCombiner
    #
    # Process the combiner set by the user in RDPQ_COMB_BASE
    # together with other render states, and write the final
    # combiner into RDPQ_MODE_COMBINER_1CYC and
    # RDPQ_MODE_COMBINER_2CYC.
    #
    # This must be called any time a new combiner is set in
    # RDPQ_COMB_BASE.
    # 
    # * a0/a1 RDPQ_COMB_BASE
    # * a2/a3 RDPQ_COMB_BASE_MIPMAPMASK
    #
    ###########################################################

    .func RDPQ_UpdateCombiner
RDPQ_UpdateCombiner:
    #define som_hi       t4
    #define comb_hi      a0
    #define comb_lo      a1
    #define comb_hi_noid t5

    lw comb_hi, %lo(RDPQ_COMB_BASE) + 0
    lw comb_lo, %lo(RDPQ_COMB_BASE) + 4
    lw a2, %lo(RDPQ_COMB_BASE_MIPMAPMASK) + 0
    lw a3, %lo(RDPQ_COMB_BASE_MIPMAPMASK) + 4

    # If the input combiner is 2-passes, we can just store it.
    bltz a0, store_comb_2cyc    
    # One-pass combiners needs to be adapated for mipmap, by setting a second step.
    lw som_hi, %lo(RDPQ_OTHER_MODES) + 0

    # Check if fogging is active
    andi t0, som_hi, SOMX_FOG >> 32
    beqz t0, check_mipmap

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
    bne t0, comb_hi_noid, check_mipmap
    lw t0, %lo(COMBINER_SHADE) + 4
    bne t0, comb_lo, check_mipmap
    li s0, %lo(COMBINER_SHADE_FOG)

fog_change:
    lw comb_hi, 0(s0)
    lw comb_lo, 4(s0)

check_mipmap:
    and t0, som_hi, SOM_TEXTURE_LOD >> 32
    beqz t0, store_comb_1cyc

    # Mipmapping is active. We want to add RDPQ_COMB_MIPMAP as step0
    # and use only step 1 of the incoming formula. Unfortunately, this
    # also means that all TEX0 slots must be converted into COMBINED slots.
    # We do this by using the mask already loaded in a2/a3
    lw t0, %lo(COMBINER_MIPMAP2) + 0
    lw t1, %lo(COMBINER_MIPMAP2) + 4
    and comb_hi, a2
    and comb_lo, a3
    # Since this combiner now requires two-cycle mode, we can simply store in the
    # 2-cycle mode slot. No need to touch the 1-cycle mode slot as it will not
    # be used anyway.
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
    # The 2PASS flag will not be set, as this combiner does not require 2cycle.
    lw t0, %lo(COMB0_MASK) + 0
    lw t1, %lo(COMB0_MASK) + 4
    and comb_hi, t0
    and comb_lo, t1

    # This combiner setting will force 2cycle mode. Store it
    # in the 2cyc slot, and ignore the 1cyc slot (it's not going
    # to be used).
store_comb_2cyc:
    sw comb_hi, %lo(RDPQ_MODE_COMBINER_2CYC) + 0
    sw comb_lo, %lo(RDPQ_MODE_COMBINER_2CYC) + 4
    # fallthrough1
    .endfunc

    #undef som_hi
    #undef comb_hi
    #undef comb_lo
    #undef comb_hi_noid

    ###############################################################
    # RDPQ_UpdateRenderMode
    #
    # This function is part of the mode API. It recalculates the
    # render mode given the current settings, and emits the RDP
    # commands necessary to configure it (SET_OTHER_MODES + SET_COMBINE).
    #
    # It is called by the mode API any time a mode changes.
    ################################################################
    .func RDPQ_UpdateRenderMode
RDPQ_UpdateRenderMode:
    #define blend_1cyc  t3
    #define blend_2cyc  t4
    #define blend_final t4
    #define passthrough t5
    #define cycle_type  t6

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
    li passthrough, 0
    lw t0, %lo(RDPQ_MODE_BLENDER_STEPS) + 0   # Load step0
    lw t1, %lo(RDPQ_MODE_BLENDER_STEPS) + 4   # Load step1
    
    beqz t0, blender_merge
    move blend_1cyc, t1
    
    beqz t1, blender_merge
    move blend_1cyc, t0

    and passthrough, t1, SOM_BLEND1_MASK
    or passthrough, SOMX_BLEND_2PASS
blender_merge:
    and blend_1cyc, SOM_BLEND0_MASK
    or blend_2cyc, blend_1cyc, passthrough

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
    bltz t1, set_2cyc
    li cycle_type, (SOM_CYCLE_MASK ^ SOM_CYCLE_2) >> 32
set_1cyc:
    li s0, %lo(RDPQ_MODE_COMBINER_1CYC)
    move blend_final, blend_1cyc
    li cycle_type, (SOM_CYCLE_MASK ^ SOM_CYCLE_1) >> 32
set_2cyc:
    #define comb_hi a0
    #define comb_lo a1
    #define som_hi  a2
    #define som_lo  a3
    lw comb_hi, 0(s0)
    lw comb_lo, 4(s0)
    lw som_hi, %lo(RDPQ_OTHER_MODES) + 0
    lw som_lo, %lo(RDPQ_OTHER_MODES) + 4

    # Set correct SET_COMBINE opcode (0xFC). The opcode can be anything of
    # the other 4 (1pass/2pass dynamic/static).
    or comb_hi, 0xFF000000
    xor comb_hi, 0xFF000000 ^ 0xFC000000

    # Coverage calculation. We need to configure the coverage bits depending
    # on the AA (SOM_AA_ENABLE) and blender settings (SOM_BLENDING). The bits
    # to set are written in the AA_BLEND_TABLE.
    #
    # Notice that if either fogging or blending are set, SOM_BLENDING will be
    # set in blend_final (which is the blender configuration to apply).
    and t0, som_lo, SOM_AA_ENABLE      # Bit 3
    and t1, blend_final, SOM_BLENDING  # Bit 14 -> 2
    srl t1, 14-2
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
    sw som_lo, %lo(RDPQ_OTHER_MODES) + 4

    # Set cycle type bits in other modes high word
    or som_hi, SOM_CYCLE_MASK >> 32
    xor som_hi, cycle_type
    sw som_hi, %lo(RDPQ_OTHER_MODES) + 0

    jal_and_j RDPQ_Write16, RDPQ_Finalize
    .endfunc

    #undef comb_hi
    #undef comb_lo
    #undef som_hi
    #undef som_lo

    #undef blend_1cyc
    #undef blend_2cyc
    #undef blend_final
    #undef passhthrough
    #undef cycle_type
