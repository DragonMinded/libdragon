#ifndef RSP_QUEUE_INC
#define RSP_QUEUE_INC

# Globally reserved registers
#define rspq_dmem_buf_ptr gp

# Can be used with l* instructions to get contents of the current command at the specified offset.
# The total command size needs to be specified as well.
#define CMD_ADDR(offset, cmdsize) (%lo(RSPQ_DMEM_BUFFER) + (offset) - (cmdsize)) (rspq_dmem_buf_ptr)

#define OVERLAY_HEADER_SIZE 0x8

.macro RSPQ_DefineCommand function size
    .ifne ((\size) % 4)
        .error "Invalid size - must be multiple of 4"
        .exitm
    .endif

    .iflt (\size)
        .error "Invalid size - valid range: [0, 60]"
        .exitm
    .endif

    .ifgt ((\size) - 60)
        .error "Invalid size - valid range: [0, 60]"
        .exitm
    .endif

    # Put the command size (as number of 4 byte words) into the high 4 bits,
    # which are not used by the RSP anyway.
    .short (\function - _start) | ((\size) & 0x3C) << 10
.endm

.macro RSPQ_OverlayHeader stateStart, stateEnd, cmdBase
    OVERLAY_HEADER:
    .long \stateStart
    .short (\stateEnd - \stateStart) - 1
    .short (\cmdBase << 1)
.endm

#endif
