
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

# TODO: commands larger than 16 bytes?
.macro commandTableEntry function size
    .short (\function - _start) | \size << 12
.endm

.macro overlayHeader savedDataStart, savedDataEnd, mask
    OVERLAY_HEADER:
    .short \savedDataStart
    .short (\savedDataEnd - \savedDataStart) - 1
    .short \mask
.endm

#endif
