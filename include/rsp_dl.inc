
#ifndef RSP_DL_INC
#define RSP_DL_INC

# Globally reserved registers
# TODO: Move some of these to DMEM
#define input_buf_start fp
#define input_buf_ptr   gp
#define input_buf_end   k1
#define read_ptr        k0
#define write_ptr       t9
#define wrap_ptr        t8
#define cur_ovl         t7

#define OVERLAY_HEADER_SIZE 0x8

# TODO: commands smaller than 4 bytes?
.macro commandTableEntry function size
    .ifne ((\size) % 4)
        .error "Invalid size - must be multiple of 4"
        .exitm
    .endif

    .ifle (\size)
        .error "Invalid size - valid range: [4, 60]"
        .exitm
    .endif

    .ifgt ((\size) - 60)
        .error "Invalid size - valid range: [4, 60]"
        .exitm
    .endif

    # Put the command size (as number of 4 byte words) into the high 4 bits,
    # which are not used by the RSP anyway.
    .short (\function - _start) | ((\size) & 0x3C) << 10
.endm

.macro overlayHeader savedDataStart, savedDataEnd, cmdBase
    OVERLAY_HEADER:
    .long \savedDataStart
    .short (\savedDataEnd - \savedDataStart) - 1
    .short (\cmdBase << 1)
.endm

#endif
