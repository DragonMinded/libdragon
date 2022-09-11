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


# Temporary combiner memory location for RDPQ_UpdateRenderMode
RDPQ_MODE_COMBINER_1CYC:     .quad  0
RDPQ_MODE_COMBINER_2CYC:     .quad  0

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
RDPQ_CMD_STAGING:       .ds.b 0x118

    .text

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
    move ra2, ra

    # Fetch current pointer in RDRAM where to write the RDP command
    mfc0 rdram_cur, COP0_DP_END

    # Calculate buffer size and DMA transfer length
    sub rspq_cmd_size, s3, s4
    add t0, rspq_cmd_size, -1

    # Fetch the sentinel (end of buffer). Check whether there is
    # enough room to add the new command. If so, run the DMA transfer,
    # and then call RSPQCmd_RdpAppendBuffer to update DP_END to include
    # the new commands.
    lw sentinel, %lo(RDPQ_SENTINEL)
    sub sentinel, rspq_cmd_size
    bge sentinel, rdram_cur, do_dma
    li next_func, RSPQCmd_RdpAppendBuffer

    # There is not enough space in the current buffer. Switch to the
    # next RDRAM buffer. Since there are two of them, also switch between
    # them so next time we will pick the other one.
    lw rdram_cur, %lo(RDPQ_DYNAMIC_BUFFERS) + 4
    lw t1, %lo(RDPQ_DYNAMIC_BUFFERS) + 0
    sw rdram_cur, %lo(RDPQ_DYNAMIC_BUFFERS) + 0
    sw t1, %lo(RDPQ_DYNAMIC_BUFFERS) + 4

    # Calculate new sentinel (end of buffer)
    addi sentinel, rdram_cur, RDPQ_DYNAMIC_BUFFER_SIZE

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
    # MODE API FUNCTIONS 
    #
    # These functions implement the mode API. They can be useful
    # for overlays that want to change RDP render mode, using the
    # mode API for simplicity and interoperability.
    ###############################################################

    .func RDPQCmd_SetBlendingMode
RDPQCmd_SetBlendingMode:
    j RDPQ_UpdateRenderMode
    sw a1, %lo(RDPQ_MODE_BLENDER_STEPS)(a0)
    # fallthrough
    .endfunc

    .func RDPQCmd_SetCombineMode_1Pass
RDPQCmd_SetCombineMode_1Pass:
    # Turn off RDPQ_COMBINER_2PASS (bit 63). This is set by default
    # because the overlay is regisred in slots 0xC0-0xF0.
    # We need to remember that this combiner does not require 2 passes
    xor a0, RDPQ_COMBINER_2PASS >> 32
    sw a2, %lo(RDPQ_COMBINER_MIPMAPMASK) + 0
    sw a3, %lo(RDPQ_COMBINER_MIPMAPMASK) + 4
    # fallthrough!
    .endfunc

    .func RDPQCmd_SetCombineMode_2Pass
RDPQCmd_SetCombineMode_2Pass:
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
    #   * If mipmap is enabled, modify the color combiner adding
    #     the mipmap formula.
    #   * Merge the two blender steps (fogging / blending), taking
    #     care of adjustments if either is active or not.
    #   * Decide whether to use 1cycle or 2cycle mode, depending
    #     on color combiner and blender.
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

    # If updates are frozen, do nothing
    lw som_hi, %lo(RDPQ_OTHER_MODES) + 0
    andi t0, som_hi, SOMX_UPDATE_FREEZE >> 32
    bnez t0, RSPQ_Loop
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
    and t0, som_hi, SOMX_LOD_INTERPOLATE >> 32
    beqz t0, store_comb_1cyc

    # Mipmapping is active. We want to add RDPQ_COMB_MIPMAP as step0
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
    li cycle_type, ((SOM_CYCLE_MASK ^ SOM_CYCLE_2) >> 32) | 0x10000000
set_1cyc:
    li s0, %lo(RDPQ_MODE_COMBINER_1CYC)
    move blend_final, blend_1cyc
    li cycle_type, ((SOM_CYCLE_MASK ^ SOM_CYCLE_1) >> 32)  | 0x10000000
set_2cyc:
    lw comb_hi, 0(s0)
    lw comb_lo, 4(s0)

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

    # Set cycle type bits in other modes high word. Also put the correct
    # command (0xEF) in the top byte: we achieve this by first setting the
    # top byte to 0xFF, and then xoring with 0x10 (which is included in
    # cycle_type).
    or som_hi, (SOM_CYCLE_MASK >> 32) | 0xFF000000
    xor som_hi, cycle_type

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
    move a0, som_hi
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

#if RDPQ_TRIANGLE_REFERENCE
    .text
    .func RDPQCmd_Triangle
RDPQCmd_Triangle:
    assert RDPQ_ASSERT_INVALID_CMD_TRI
#else
    .text

    #####################################################################
    # RDPQ_Triangle
    #
    #####################################################################

    #define VTX_ATTR_X   0
    #define VTX_ATTR_Y   4 


    .func RDPQCmd_Triangle
RDPQCmd_Triangle:
    #define vtx0   s0
    #define vtx1   s1
    #define vtx2   s2

    la vtx0, CMD_ADDR( 4, 7*4)
    la vtx1, CMD_ADDR(12, 7*4)
    la vtx2, CMD_ADDR(20, 7*4)
    
    #define y0     t4
    #define y1     t5
    #define y2     t6

    j half_swap
    li t0, 1

swap_loop:
    lw y1, VTX_ATTR_Y(vtx1)
    lw y2, VTX_ATTR_Y(vtx2)
    blt y1, y2, half_swap
    nop
    xor vtx1, vtx2
    xor vtx2, vtx1
    xor vtx1, vtx2

half_swap:
    lw y0, VTX_ATTR_Y(vtx0)
    lw y1, VTX_ATTR_Y(vtx1)
    blt y0, y1, swap_end
    nop
    xor vtx0, vtx1
    xor vtx1, vtx0
    xor vtx0, vtx1

swap_end:
    bnez t0, swap_loop
    addi t0, -1

    #
    # 
    #
    #

    # r, g, b, a, s, t, w, z
    #define final_i          $v00
    #define final_f          $v01
    #define dx_i             $v02
    #define dx_f             $v03
    #define de_i             $v04
    #define de_f             $v05
    #define dy_i             $v06
    #define dy_f             $v07

    #define a1_i             $v08
    #define a1_f             $v09
    #define a2_i             $v10
    #define a2_f             $v11
    #define a3_i             $v12
    #define a3_f             $v13

    #define ma_i             $v14
    #define ma_f             $v15
    #define ha_i             $v16
    #define ha_f             $v17

    #define v__              $v29
    #define fy_i             $v30,e(0)
    #define fy_f             $v30,e(1)
    #define hx_i             $v30,e(2)
    #define hx_f             $v30,e(3)
    #define mx_i             $v30,e(4)
    #define mx_f             $v30,e(5)
    #define hy_i             $v31,e(0)
    #define hy_f             $v31,e(1)
    #define my_i             $v31,e(2)
    #define my_f             $v31,e(3)
    #define invn_i           $v31,e(4)
    #define invn_f           $v31,e(5)
    #define invsh_i          $v31,e(6)
    #define invsh_f          $v31,e(7)

    # MA = A2 - A1
    vsubc ma_f, a2_f, a1_f
    vsub  ma_i, a2_i, a1_i

    # HA = A3 - A1
    vsubc ha_f, a3_f, a1_f
    vsub  ha_i, a3_i, a1_i

    # DX = MA * HY - HA * MY
    # TODO: MY must be negated
    vmudl v__,  ma_f, hy_f
    vmadm v__,  ma_f, hy_i
    vmadn v__,  ma_i, hy_f
    vmadh v__,  ma_i, hy_i
    vmadl v__,  ha_f, my_f
    vmadm v__,  ha_f, my_i
    vmadn dx_f, ha_i, my_f
    vmadh dx_i, ha_i, my_i

    # DY = HA * MX - MA * HX
    # TODO: HX must be negated
    vmudl v__,  ma_f, hx_f
    vmadm v__,  ma_f, hx_i
    vmadn v__,  ma_i, hx_f
    vmadh v__,  ma_i, hx_i
    vmadl v__,  ha_f, mx_f
    vmadm v__,  ha_f, mx_i
    vmadn dx_f, ha_i, mx_f
    vmadh dx_i, ha_i, mx_i

    # DX * 1/N (TODO: check if we can pre-multiply edges to avoid this)
    vmudl v__,  dx_f, invn_f
    vmadm v__,  dx_i, invn_f
    vmadn dx_f, dx_f, invn_i
    vmadh dx_i, dx_i, invn_i

    # DY * 1/N (TODO: check if we can pre-multiply edges to avoid this)
    vmudl v__,  dy_f, invn_f
    vmadm v__,  dy_i, invn_f
    vmadn dy_f, dy_f, invn_i
    vmadh dy_i, dy_i, invn_i

    # DE = DX * invsh + DY
    vmudl v__,  dx_f, invsh_f
    vmadm v__,  dx_i, invsh_f
    vmadn de_f, dx_f, invsh_i
    vmadh de_i, dx_i, invsh_i
    vaddc de_f, dy_f
    vadd  de_i, dy_i

    # FINAL = A1 + DE * FY
    vmudl v__,     de_f, fy_f
    vmadm v__,     de_i, fy_f
    vmadn final_f, de_f, fy_i
    vmadh final_i, de_i, fy_i
    vaddc final_f, a1_f
    vadd  final_i, a1_i

    li s4, %lo(RDPQ_CMD_STAGING)
    move s3, s4

    # Store color
    sdv final_i, 0, 0x00,s3
    sdv dx_i,    0, 0x08,s3
    sdv final_f, 0, 0x10,s3
    sdv dx_f,    0, 0x18,s3
    sdv de_i,    0, 0x20,s3
    sdv dy_i,    0, 0x28,s3
    sdv de_f,    0, 0x30,s3
    sdv dy_f,    0, 0x38,s3
    addi s3, 0x40

    # Store texture
    sdv final_i, 8, 0x00,s3
    sdv dx_i,    8, 0x08,s3
    sdv final_f, 8, 0x10,s3
    sdv dx_f,    8, 0x18,s3
    sdv de_i,    8, 0x20,s3
    sdv dy_i,    8, 0x28,s3
    sdv de_f,    8, 0x30,s3
    sdv dy_f,    8, 0x38,s3
    addi s3, 0x40

    # Store z
    ssv final_i, 14, 0x00,s3
    ssv final_f, 14, 0x02,s3
    ssv dx_i,    14, 0x04,s3
    ssv dx_f,    14, 0x06,s3
    ssv de_i,    14, 0x08,s3
    ssv de_f,    14, 0x0A,s3
    ssv dy_i,    14, 0x0C,s3
    ssv dy_f,    14, 0x0E,s3
    addi s3, 0x10

    j RDPQ_Send
    nop

    .endfunc


#endif


