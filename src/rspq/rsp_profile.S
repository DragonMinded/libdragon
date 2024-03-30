#include <rsp_queue.inc>
#if RSPQ_PROFILE

    .data

    RSPQ_BeginOverlayHeader
RSPQ_DefineCommand Cmd_ProfileFrame,    4     # 0x00
    RSPQ_EndOverlayHeader

    RSPQ_BeginSavedState

PROFILE_STATE_START:
PROFILE_CNTR1:           .long     0
                         .long     0  # padding
PROFILE_OVL_SLOTS:       .space    8 * RSPQ_MAX_OVERLAYS
PROFILE_CMN_SLOTS:       .space    8 * RSPQ_PROFILE_CSLOT_COUNT
PROFILE_CMN_SLOTS_END:
PROFILE_FRAME_TIME:      .long     0
PROFILE_BUSY_TIME:       .long     0
PROFILE_FRAME_LAST:      .long     0
PROFILE_BUSY_LAST:       .long     0
                         .long     0 # padding
PROFILE_CNTR2:           .long     0
PROFILE_STATE_END:

    RSPQ_EndSavedState

    .text

    .func Cmd_ProfileFrame
Cmd_ProfileFrame:
    # Update various per-frame counters
    mfc0 t0, COP0_DP_CLOCK
    mfc0 t3, COP0_DP_BUSY
    sll t0, 8
    sll t3, 8
    lw t1, %lo(PROFILE_FRAME_LAST)
    lw t4, %lo(PROFILE_BUSY_LAST)
    sw t0, %lo(PROFILE_FRAME_LAST)
    sw t3, %lo(PROFILE_BUSY_LAST)
    subu t2, t0, t1
    subu t5, t3, t4
    sra t2, 8
    sra t5, 8
    lw t0, %lo(PROFILE_FRAME_TIME)
    lw t1, %lo(PROFILE_BUSY_TIME)
    addu t0, t0, t2
    addu t1, t1, t5
    sw t0, %lo(PROFILE_FRAME_TIME)
    sw t1, %lo(PROFILE_BUSY_TIME)

    # Copy common slots
    li s0, %lo(RSPQ_PROFILE_DATA)
    li s1, %lo(PROFILE_CMN_SLOTS)
    li s2, %lo(PROFILE_CMN_SLOTS_END)
loop_cmn:
    lw t0, 0(s0)
    lw t1, 4(s0)
    addiu s1, 8
    sw t0, -8(s1)
    sw t1, -4(s1)
    bne s1, s2, loop_cmn
     addiu s0, 8

    #define inptr      s1
    #define inptr_end  t5
    #define outptr     s3

    # Copy builtin overlay data to OVL_PROFILE_SLOTS
    li s0, %lo(RSPQ_PROFILESLOT_BUILTINS)
    li outptr, %lo(PROFILE_OVL_SLOTS)
    ldv $v01, 0,s0
    sdv $v01, 0,outptr
    addiu outptr, 8

    # Loop through the overlay table
    li inptr, %lo(RSPQ_OVERLAY_TABLE)+4
    li inptr_end, %lo(RSPQ_OVERLAY_TABLE)+4*RSPQ_MAX_OVERLAYS
loop_ovl:
    # Load packed size+address of data segment. Ignore the size
    # We want to load the first two words which are the state offset and size
    lw s2, 0(inptr)
    beqz s2, loop_ovl_epilog

    # Fetch the profile slot offset in the data segment
     addiu s0, s2, RSPQ_OH_PROFILESLOT
    li s4, %lo(RSPQ_SCRATCH_MEM)
    jal DMAIn
     li t0, DMA_SIZE(8, 1)

    # Read the profile offset, and then fetch the profile slot
    lhu s0, 0(s4)
    addiu s0, -(_ovl_data_start - _data_start)
    addu s0, s2
    move s4, outptr
    jal DMAIn
     li t0, DMA_SIZE(8, 1)

loop_ovl_epilog:
    addiu inptr, 4
    bne inptr, inptr_end, loop_ovl
     addiu outptr, 8

    #undef inptr
    #undef inptr_end
    #undef outptr

    # Increment the two counters used to avoid race conditions on the CPU
    lw t0, %lo(PROFILE_CNTR1)
    lw t1, %lo(PROFILE_CNTR2)
    addiu t0, 1
    addiu t1, 1
    sw t0, %lo(PROFILE_CNTR1)
    sw t1, %lo(PROFILE_CNTR2)

profile_end:    
    # DMA the whole profile state to RDRAM where requested by the CPU
    move s0, a0
    li s4, %lo(PROFILE_STATE_START)
    jal DMAOut
     li t0, DMA_SIZE(PROFILE_STATE_END - PROFILE_STATE_START, 1)

    j RSPQ_Loop
     nop

    .endfunc

#endif
