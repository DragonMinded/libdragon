########################################################
# Include this file wherever you prefer in your text segment
########################################################

########################################################
# DMA functions:
#   DMAIn / DMAInAsync / DMAOut / DMAOutAsync / DMAExec
#
# These functions are used to issue DMA transfers between
# RDRAM and internal DMEM:
#
#   * DMAIn: run a RDRAM -> DMEM transfer, waiting for completion
#   * DMAInAsync: start a background RDRAM -> DMEM transfer
#   * DMAOut: run a DMEM -> RDRAM transfer, waiting for completion
#   * DMAOutAsync: start a background DMEM -> RDRAM transfer
#   * DMAExec: single entry point for any type of transfer. It can be used
#     to write direction-agnostic transfer code that can be then invoked
#     passing in the direction in a register.
#
# The RSP DMA controller allows to enqueue one transfer while another one
# is in progress. You can use the *Async functions to do this. If you try
# to enqueue a third transfer, though, the *Async function will automatically
# block waiting for the first one to finish. This allows the code to call
# *Async functions freely and get "best effort" background transfers.
# 
# When you need to explicitly synchronize (eg: to access read data), you can
# use DMAWaitReady to wait until the DMA engine is ready to receive another
# transfer, or DMAWaitIdle to wait until all DMA transfers are finished. See
# their documentation below.
#
# The DMA controller is able to read/write "rectangular portions"
# of memory in RDRAM (that is, you can define width, height and
# pitch to extract a portion of a larger image). Accesses in DMEM
# are always linear, though. If you don't need this feature,
# you can set the transfer size as width and use 1 as height: this way,
# the pitch will be ignored.
#
# At the hardware level, both the DMEM and the RDRAM addresses are
# treated as 8-byte aligned (that is, the lowest 3 bits are ignored).
# If you issue a transfer from RDRAM with an unaligned address (in s0),
# the DMEM address (s4) will be updated to reflect where the byte that
# was actually requested ended up. For instance, if s0 = 0x80041203, s4=0x100,
# after the function s4 will be 0x103, as that is the address where the
# byte at 0x80041203 ended up being written to.
#
# INPUT:
#   t0: transfer size as width/height (use DMA_SIZE macro)
#   t1: pitch (ignored if height==1)
#   s0: RDRAM address
#   s4: DMEM address. After DMA, updated with unalignment
#
#   Only for DMAExec:
#   t2: DMA operation flags: select direction and sync/async.
#       Use one of the DMA_* macros below. If async, you can call
#       DMAWaitIdle to synchronize later if required.
#
# OUTPUT:
#   Only for DMAIn:
#   s4: adjusted to point to the first requested byte
#       (because of DMA alignment, more bytes might have been
#       DMA'd).
#
# DESTROY:
#   t2, at
# 
#########################################################

    .func DMAOut
DMAOut:
    b DMAExec
    li t2, DMA_OUT
    .endfunc

    .func DMAOutAsync
DMAOutAsync:
    b DMAExec
    li t2, DMA_OUT_ASYNC
    .endfunc

    .func DMAInAsync
DMAInAsync:
    b DMAExec
    li t2, DMA_IN_ASYNC
    .endfunc

    .func DMAIn
DMAIn:
    li t2, DMA_IN
    # fallthrough!
    .endfunc

    .set noat
    .func DMAExec
DMAExec:
    # Wait for the DMA engine to be ready. We do this
    # every time because with async transfers it's easy to get
    # confused and forget to wait.
    mfc0 $1, COP0_DMA_FULL
    bnez $1, DMAExec
    # Merge pitch into the width/height word, converting it
    # into "skip" (= pitch-width)
    andi $1, t0, 0xFFF
    sub $1, t1, $1
    addi $1, -1
    sll $1, 20
    or $1, t0
    # Write RDRAM address
    mtc0 s0, COP0_DMA_RAMADDR
    # Check whether it's read or write (depending on sign of t2,
    # see the DMA_* macros)
    bgez t2, DMAIn1
    mtc0 s4, COP0_DMA_SPADDR
    b DMAInEnd
    mtc0 $1, COP0_DMA_WRITE
DMAIn1:
    mtc0 $1, COP0_DMA_READ
DMAInEnd:
    # Update s4 with the unalignment offset in source buffer,
    # so that s4 points to the first requested byte.
    andi $1, s0, 7
    add s4, $1
    andi t2, 0xF   # isolate DMA wait flags
SpStatusWait:
DMAWaitLoop:
    # Wait for DMA to finish. This actually checks the
    # requested wait flags. If the transfer is ASYNC,
    # the wait flags are 0 so this loop exits immediately
    # without checking for anything.
    mfc0 $1, COP0_SP_STATUS
DMAWaitLoop2:
    and $1, t2
    bnez $1, DMAWaitLoop2
    mfc0 $1, COP0_SP_STATUS
JrRa: # Useful label if you need a pure "jr ra" somewhere in memory to jump to
    jr ra
    nop
    .endfunc

###################################################################
# DMAWaitIdle
#
# Wait until the DMA is idle again, which means all enqueued DMA
# transfers has been executed
#
# DESTROY:
#   at, t2
###################################################################
    .func DMAWaitIdle
DMAWaitIdle:
    b DMAWaitLoop
    li t2, SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL
    .endfunc

###################################################################
# DMAWaitReady
#
# Wait until the DMA engine is ready to accept another transfer.
# Since the DMA engine allows a transfer to be enqueued when another
# is in progress, this means that one DMA transfer might still be
# in progress when this function returns.
#
# IMPORTANT NOTE:
#
# If you know that you have enqueued two DMA transfers, this function
# can be used as a proxy to wait for only the first transfer to finish
# (because when the first transfer is finished, the second begins and
# the queue becomes ready to accept a new transfer).
# 
# Notice, though, that the DMA engine becomes ready to accept a new 
# transfer about 10 cycles before the previous transfer actually finish
# writing data in DMEM. So after DMAWaitReady returns, you must allow
# for a small pause before actually accessing data read by the first
# transfer.
#
# DESTROY:
#   at, t2
###################################################################

    .func DMAWaitReady
DMAWaitReady:
    b DMAWaitLoop
    li t2, SP_STATUS_DMA_FULL
    .endfunc
    .set at
