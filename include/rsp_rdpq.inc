    #define VTX_ATTR_Wi      16
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
RDPQ_CMD_STAGING:       .ds.b 0xB0

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
    #ifndef NDEBUG
    andi s3, 0xFFF
    andi s4, 0xFFF
    assert_gt s3, s4, RDPQ_ASSERT_SEND_INVALID_SIZE
    #endif
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

    #####################################################################
    # RDPQ_Triangle
    #
    #####################################################################

    .data
    .align 4
TRICONST1: .half 0,0,0,0,0x200,0x200,0x200,0x200


    .text

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

    #define y1     t4
    #define y2     t5
    #define y3     t6
    #define x1     t7
    #define x2     t8
    #define x3     v0

    # r, g, b, a, s, t, w, z
    #define final_i          $v01
    #define final_f          $v02
    #define dx_i             $v03
    #define dx_f             $v04
    #define de_i             $v05
    #define de_f             $v06
    #define dy_i             $v07
    #define dy_f             $v08

    #define attr1            $v09
    #define attr2            $v10
    #define attr3            $v11
    #define ma               $v12
    #define ha               $v13

    #define invw_i           $v14
    #define invw_f           $v15

    #define edges_i          $v16
    #define edges_f          $v17
    #define nz_i             $v18
    #define nz_f             $v19
    #define slope_i          $v20
    #define slope_f          $v21
    #define vxy32            $v22
    #define vxy21            $v23
    #define vhml             $v24
    #define fy_i             $v25
    #define fy_f             $v26

    #define v__              $v29
    #define invn_i           $v31,e(4)
    #define invn_f           $v31,e(5)
    #define invsh_i          $v31,e(6)
    #define invsh_f          $v31,e(7)

    #define VTX_ATTR_X      0
    #define VTX_ATTR_Y      2
    #define VTX_ATTR_Z      6
    #define VTX_ATTR_RGBA   8
    #define VTX_ATTR_S      12
    #define VTX_ATTR_T      14
    #define VTX_ATTR_W      16
    #define VTX_ATTR_INVWi  20
    #define VTX_ATTR_INVWf  22

    li s4, %lo(RDPQ_CMD_STAGING)
    move s3, s4

    j half_swap
    li t0, 1

swap_loop:
    lh y2, VTX_ATTR_Y(vtx2)
    lh y3, VTX_ATTR_Y(vtx3)
    blt y2, y3, half_swap
    nop
    xor vtx2, vtx3
    xor vtx3, vtx2
    xor vtx2, vtx3

half_swap:
    lh y1, VTX_ATTR_Y(vtx1)
    lh y2, VTX_ATTR_Y(vtx2)
    blt y1, y2, swap_end
    nop
    xor vtx1, vtx2
    xor vtx2, vtx1
    xor vtx1, vtx2

swap_end:
    bnez t0, swap_loop
    addi t0, -1

    # We want to build this layout
    #  vxy32 = X3 X2 X3 --    Y3 Y2 Y3 --
    #  vxy21 = X1 -- X2 --    Y1 -- Y2 --

    lsv vxy32,0,  VTX_ATTR_X,vtx3
    lsv vxy32,8,  VTX_ATTR_Y,vtx3
    vor vxy32, vzero, vxy32,e(0h)
    lsv vxy32,2,  VTX_ATTR_X,vtx2
    lsv vxy32,10, VTX_ATTR_Y,vtx2

    lsv vxy21,0,  VTX_ATTR_X,vtx1
    lsv vxy21,4,  VTX_ATTR_X,vtx2
    lsv vxy21,8,  VTX_ATTR_Y,vtx1
    lsv vxy21,12, VTX_ATTR_Y,vtx2

    # Store Y values in output
    ssv vxy21,8,  6,s3    # y1
    ssv vxy32,10, 4,s3    # y2
    ssv vxy32,8,  2,s3    # y3

    # Now calculate:
    #  vxy32    = X3 X2 X3 --    Y3 Y2 Y3 --
    #    -
    #  vxy21.0q = X1 X1 X2 X2    Y1 Y1 Y2 Y2
    #    = 
    #  vhml     = HX MX LX --    HY MY LY --
    vsub vhml, vxy32, vxy21,e(0q)
    #define hx  vhml,e(0)
    #define mx  vhml,e(1)
    #define lx  vhml,e(2)
    #define my1 vhml,e(3)
    #define hy  vhml,e(4)
    #define my  vhml,e(5)
    #define ly  vhml,e(6)
    #define mx1 vhml,e(7)

    #  vhml     = HX MX LX MY    HY MY LY MX
    vmov vhml,15, vhml,9
    vmov vhml,11, vhml,13

    # Calculate normal: compute 32-bit cross product:
    #
    #  vhml     = HX MX LX MY    HY MY LY MX
    #    *
    #  vhml.3h  = MY MY MY MY    MX MX MX MX
    #    =
    #  nz       = HX*MY -- -- --    HY*MX -- -- -- --
    vmudh nz_f, vhml, vhml,e(3h)
    vsar nz_i, COP2_ACC_HI
    vsar nz_f, COP2_ACC_MD

    # Compute HY*MX - HX*MY. Result in e(4).
    vsubc nz_f, nz_f,e(0)
    vsub  nz_i, nz_i,e(0)

    # Extract left flag from the sign of NZ.
    # Since we calculated -NZ, we need to reverse the sign
    mfc2 t0, nz_i,e(4)
    sge t0, t0, zero
    sll t0, 7
    or tricmd, t0

    # Add num mipmap levels
    lbu t1, %lo(RDPQ_OTHER_MODES) + 0
    andi t1, 0x38     # Isolate bits 2-5 (aka 59-61 of SOM)
    or tricmd, t1

    # Calculate reciprocal of normal
    vrcph nz_i,e(0), nz_i,e(4)
    vrcpl nz_f,e(0),  nz_f,e(4)
    vrcph nz_i,e(0), v__,e(0)
    #define inz_f   nz_f,e(0)
    #define inz_i   nz_i,e(0)

    # Compute SLOPE vector
    # slope    =  -- -- -- --    1/HY 1/MY 1/LY 1/NZ

    # Compute ISL (L slope). 1/LY  (s14.1)
    vrcp  slope_f,14, vhml,e(6)
    vrcph slope_i,14, vhml,e(6)
    # Compute ISM (M slope). 1/MY  (s14.1)
    vrcp  slope_f,13,  vhml,e(5)
    vrcph slope_i,13,  vhml,e(5)
    # Compute ISH (H slope). 1/HY  (s14.1)
    vrcp  slope_f,12,  vhml,e(4)
    vrcph slope_i,12,  vhml,e(4)

    ##################
    # 1 NR pass
    ##################
    vmov slope_f,15, inz_f
    vmov slope_i,15, inz_i

    # Adjust multiplying by 2 (required after reciprocal)
    #vmudn slope_f, slope_f, K2
    #vmadh slope_i, slope_i, K2
    vaddc slope_f, slope_f
    vadd slope_i, slope_i

    # Prepare 32-bit number containing the source of the reciprocal
    # Notice that we're calculating NR over 1 32-bit input (NZ) and
    # 3 16-bit inputs (HY, MY, LY), for which we provide 0 in the lower
    # part.
    #    vhml      =   HX MX LX MY    HY   MY   LY   NZf
    #    v__       =    0 0  0  0     0    0    0    NZi
    vxor v__, v__
    vmov v__,15, nz_i,e(4)
    vmov vhml,15, nz_f,e(4)

    #define vtmp_f  attr1
    #define vtmp_i  attr2
    #define vk2     attr3

    # NR: R*X
    vmudl vtmp_f, slope_f, vhml
    vmadm vtmp_f, slope_i, vhml
    vmadn vtmp_f, slope_f, v__
    vmadh vtmp_i, slope_i, v__

    # NR: 2 - R*X   
    vor vk2, vzero, K2
    vsubc vtmp_f, vzero, vtmp_f
    vsub  vtmp_i, vk2,   vtmp_i

    # NR: X * (2 - R*X)
    vmudl vk2, vtmp_f, slope_f
    vmadm vk2, vtmp_i, slope_f
    vmadn slope_f, vtmp_f, slope_i
    vmadh slope_i, vtmp_i, slope_i
    #vmadn slope_f, vzero, vzero     # re-read slope_f in case of overflow

    # vhml      =   HX MX LX MY    HY   MY   LY   NZf
    # v__       =    0 0  0  0     0    0    0    NZi
    # slope     =   -- -- -- --   1/HY 1/MY 1/LY  1/NZ
    
    vmov nz_f,8, slope_f,e(7)
    vmov nz_i,8, slope_i,e(7)

    # Rotate slope
    # slope     =   1/HY 1/MY 1/LY 1/NZ   -- -- -- --
    sqv slope_f,e(4), 0x10,s3
    lqv slope_f,e(0)  0x10,s3
    sqv slope_i,e(4), 0x10,s3
    lqv slope_i,e(0)  0x10,s3

    # Shift left NZ (that contains INVNZ) by 2, to align with the fixed point precision
    # that will be required later.
    vmudn nz_f, nz_f, K4
    vmadh nz_i, nz_i, K4

    # FY.e4 = fy (s15.16)
    vsll8 fy_f, vxy21, 14
    vsra fy_i, vxy21, 2
    # FY.e4 = floorf(y1) - y1
    # TODO: this is always a negative fraction, so fy_i is always 0xFFFF (or fy_i=fy_f=0).
    # See if we can take advantage of this somehow to simplify later.
    vsubc fy_f, vzero, fy_f
    vsub  fy_i, fy_i

    # Finalize slope divisions by multiplying by the reciprocal.
    #  vhml     =   HX    MX    LX    1     HY MY LY MX
    #    *
    #  slope    =  1/HY  1/MY  1/LY  1/NZ   -- -- -- --
    #    =
    #  slope    =  HX/HY MX/MY LX/LY  --    -- -- -- --
    vmudn v__,     slope_f, vhml
    vmadh v__,     slope_i, vhml
    vsar  slope_f, COP2_ACC_MD
    vsar  slope_i, COP2_ACC_HI

    #define ish_f   slope_f,e(0)
    #define ish_i   slope_i,e(0)
    #define ism_f   slope_f,e(1)
    #define ism_i   slope_i,e(1)
    #define isl_f   slope_f,e(2)
    #define isl_i   slope_i,e(2)

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
    vsra final_i, vxy21, 2
    vsll8 final_f, vxy21, 14

    # Store X2 value in output (as XL)
    ssv final_f,4, 10,s3  # XL_F
    ssv final_i,4,  8,s3  # Xl_I

    # Compute XH/XM
    # TODO: fy_i is always 0xFFFFFFFF here. See if we can benefit from this.
    vmudl v__,     slope_f, fy_f,e(4)
    vmadm v__,     slope_i, fy_f,e(4)
    vmadn edges_f, slope_f, fy_i,e(4)
    vmadh edges_i, slope_i, fy_i,e(4)

    vaddc edges_f, final_f.q0
    vadd  edges_i, final_i.q0

    ssv edges_f,2, 26,s3  # XM_F
    ssv edges_i,2, 24,s3  # XM_I
    ssv edges_f,0, 18,s3  # XH_F
    ssv edges_i,0, 16,s3  # XH_I

    sh tricmd, 0(s3)
    add s3, 32

    # Load attributes into ATTR registers.
    # TODO: we can interleave these in all the code above, and at that point
    # it's useless to test for tricmd to save loads. Just load them all.

    #define attr1_r     attr1,e(0)
    #define attr2_r     attr2,e(0)
    #define attr3_r     attr3,e(0)
    #define attr1_s     attr1,e(4)
    #define attr2_s     attr2,e(4)
    #define attr3_s     attr3,e(4)
    #define attr1_invw  attr1,e(6)
    #define attr2_invw  attr2,e(6)
    #define attr3_invw  attr3,e(6)
    #define attr1_z     attr1,e(7)
    #define attr2_z     attr2,e(7)
    #define attr3_z     attr3,e(7)
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
    mtc2 t0, invw_f,e(0)
    srl t0, 16
    mtc2 t0, invw_i,e(0)

    lsv invw_i,e(4), VTX_ATTR_INVWi,vtx1
    lsv invw_i,e(5), VTX_ATTR_INVWi,vtx2
    lsv invw_i,e(6), VTX_ATTR_INVWi,vtx3

    lsv invw_f,e(4), VTX_ATTR_INVWf,vtx1
    lsv invw_f,e(5), VTX_ATTR_INVWf,vtx2
    lsv invw_f,e(6), VTX_ATTR_INVWf,vtx3

    # invw:     minw -- -- --            invw1 invw2 invw3 --
    #
    # We need to multiply minw with the three invw. All numbers are unsigned 0.31,
    # the result is known to fit 0..1 and so will be 0.31 again. and we want to keep
    # only the higher part of it, so shift the result right by 16.
    # Change the usual sequence to put vmudl last, to extract the correct
    # portion of the accumulator. Don't do the vmudh part as it's guaranteed to be
    # 0, and we don't need it.
    vmudm v__,    invw_i, invw_f,e(0)
    vmadn v__,    invw_f, invw_i,e(0)
    vmadl invw_i, invw_f, invw_f,e(0)

    # Load 0x200 in the first 4 lanes of the vector, using a misaliged lqv.
    # 0x200 is the constant that can be used to >>7, which will be used for
    # the RGBA components.
    #
    # invw:  0x200 0x200 0x200 0x200   invw1 invw2 invw3 --
    li s0, %lo(TRICONST1)+8
    lqv invw_i,0, 0,s0

    vmudm attr1, invw_i.h0
    vmudm attr2, invw_i.h1
    vmudm attr3, invw_i.h2

    # Change inv_w from 0.16 to s0.15 by shifting by one
    vsrl invw_i, invw_i, 1

    # Copy inv_w components into ATTRn
    vmov attr1,14, invw_i,12
    vmov attr2,14, invw_i,13
    vmov attr3,14, invw_i,14

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
    vsub  ma, attr2, attr1
    vsub  ha, attr3, attr1

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
    vmudh dx_f, ma, hy
    vmadh dx_f, ha, my
    vsar dx_f, COP2_ACC_MD
    vsar dx_i, COP2_ACC_HI

    # DY = HA * MX - MA * HX
    vmudh dy_f, ha, mx
    vmadh dy_f, ma, hx
    vsar dy_f, COP2_ACC_MD
    vsar dy_i, COP2_ACC_HI

    # DX * 1/N (TODO: check if we can pre-multiply edges to avoid this)
    vmudl v__,  dx_f, inz_f
    vmadm v__,  dx_i, inz_f
    vmadn dx_f, dx_f, inz_i
    vmadh dx_i, dx_i, inz_i

    # DY * 1/N (TODO: check if we can pre-multiply edges to avoid this)
    vmudl v__,  dy_f, inz_f
    vmadm v__,  dy_i, inz_f
    vmadn dy_f, dy_f, inz_i
    vmadh dy_i, dy_i, inz_i

    # DE = DX * invsh + DY
    vmadl v__,  dx_f, ish_f
    vmadm v__,  dx_i, ish_f
    vmadn de_f, dx_f, ish_i
    vmadh de_i, dx_i, ish_i

    # FINAL = ATTR1 + DE * FY
    # TODO: fy_i is always 0xFFFFFFFF here. See if we can benefit from this.
    # TODO: actually, it can also be fy_i = fy_f = 0.
    vmudl v__,     de_f, fy_f,e(4)
    vmadm v__,     de_i, fy_f,e(4)
    vmadn final_f, de_f, fy_i,e(4)
    vmadh final_i, de_i, fy_i,e(4)
    vmadh final_i, attr1, K1

    andi t0, tricmd, 0x400
    beqz t0, no_color

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

no_color:
    andi t0, tricmd, 0x200
    beqz t0, no_texture

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

no_texture:
    andi t0, tricmd, 0x100
    beqz t0, no_z

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

no_z:
    j RDPQ_Send
    nop

    .endfunc


