############################################################
# rsp_lspr1.S
#
# RSP ucode for BC1/DXT1 (BC1Q) sprite decoding. Decodes BC1
# block-compressed data into RGBA5551 pixels in RDRAM via SP DMA.
#
# Pipeline:
#   1. CPU calls cmd_lspr1_set_buffers once per sprite to record the
#      RDRAM physical addresses of the compressed payload and the
#      decoded RGBA5551 destination, plus the destination row stride.
#   2. CPU then enqueues a series of cmd_lspr1_decode_strip commands,
#      one per horizontal strip of up to N_STRIP_BLOCKS_MAX blocks.
#      Each strip DMAs in its compressed blocks, decodes them with a
#      vectorized per-block kernel, and DMAs the four output pixel rows
#      back to RDRAM at the appropriate destination row offsets.
#
# The per-block math mirrors src/lspr1.c::lspr1_decode_block() bit
# for bit so the RSP output is bit-exact with the reference C decoder.
############################################################

#include <rsp_queue.inc>

    .set noreorder
    .set noat

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand cmd_lspr1_set_buffers,    12
        RSPQ_DefineCommand cmd_lspr1_decode_strip,   12
    RSPQ_EndOverlayHeader

    .align 4
    .ascii " Dragon RSP BC1Q"

    RSPQ_BeginSavedState
RDRAM_IN_BASE:    .long 0     # phys addr of payload[] (8-byte aligned)
RDRAM_OUT_BASE:   .long 0     # phys addr of decoded RGBA5551 region
RDRAM_OUT_STRIDE: .long 0     # bytes between RGBA rows (= width * 2)
    RSPQ_EndSavedState

# Vector constants:
#   e0 = 0x1F     (5-bit channel mask for the vectorized endpoint unpack)
#   e1 = 2048     (<<11)
#   e2 = 64       (<<6)
#   e3 = 2        (<<1)
#   e4 = 1        (alpha bit for RGBA5551)
#   e5 = 0x8000   (vmudl logical >>1)
#   e6 = 0x0100   (vmudl logical >>8)
#   e7 = 21888    (= 171<<7: vmudm accumulates full products, vd = ACC>>16,
#                  so a *21888 MAC chain yields (sum*171)>>9 = sum/3)
    .align 4
BC1Q_VCONST:
    .half 0x1F, 2048, 64, 2, 1, 0x8000, 0x0100, 21888
BC1Q_PACKCONST:
    .half 2048, 64, 2, 0, 0, 0, 0, 0
# Per-lane vmudl multipliers that logically shift an RGBA5551 endpoint right so
# each 5-bit channel lands in bits [4:0] ((c*m)>>16): R>>11 (*32), G>>6 (*1024),
# B>>1 (*32768). Lanes 0..2 and 4..6 hold the two endpoints; 3/7 are unused.
BC1Q_UNPACK:
    .half 32, 1024, 0x8000, 0, 32, 1024, 0x8000, 0
# Per-lane left-shift multipliers for the vectorized index extraction. For pixel
# lane j the 2-bit index lives at bits [2j:2j+1] of the 16-bit indices half; a
# vmudn by 2^(14-2j) lands that field in bits [14:15] of lane j. The gather then
# reads only bit15 (index bit1, the sign) and bit14 (index bit0), so the low
# garbage bits left by the multiply are irrelevant and no shift/mask is needed.
BC1Q_LOOKUP:
    .half 16384, 4096, 1024, 256, 64, 16, 4, 1
# Low-half-only pack constants for alpha_mode. Identical to the pack/alpha-bit
# multipliers but zeroed in lanes 4..7, so the packed result is 0 there
# (v_pal.e4 = pal[3] = fully transparent). Four contiguous vectors, loaded via
# lqv at +0/+16/+32/+48.
    .align 4
BC1Q_PACKLO:
    .half 2048, 2048, 2048, 2048, 0, 0, 0, 0   # R<<11 in lanes 0..3
    .half   64,   64,   64,   64, 0, 0, 0, 0   # G<<6
    .half    2,    2,    2,    2, 0, 0, 0, 0   # B<<1
    .half    1,    1,    1,    1, 0, 0, 0, 0   # alpha bit

# Strip buffers.
.equ N_STRIP_BLOCKS_MAX, 32

    .bss

    .align 4
INBUF:      .ds.b N_STRIP_BLOCKS_MAX * 8

    .align 4
# Output layout within OUTBUF (row-interleaved across the strip):
#   row 0 of all blocks, contiguous   [n_blocks * 8 bytes]
#   row 1 of all blocks                ...
#   row 2 of all blocks
#   row 3 of all blocks
# Per-strip row stride = n_blocks * 8 bytes; total = 4 * n_blocks * 8.
OUTBUF:     .ds.b N_STRIP_BLOCKS_MAX * 16 * 2

    .text

# ============================================================
# cmd_lspr1_set_buffers
#   a0 = RDRAM_IN_BASE  (phys addr of payload[])
#   a1 = RDRAM_OUT_BASE (phys addr of decoded RGBA5551 region)
#   a2 = RDRAM_OUT_STRIDE (bytes per RGBA row = width * 2)
# ============================================================
    .func cmd_lspr1_set_buffers
cmd_lspr1_set_buffers:
    sw a0, %lo(RDRAM_IN_BASE)
    sw a1, %lo(RDRAM_OUT_BASE)
    jr  ra
    sw a2, %lo(RDRAM_OUT_STRIDE)
    .endfunc


# ============================================================
# cmd_lspr1_decode_strip
#   a0 = in_offset_bytes  (offset from RDRAM_IN_BASE  to first block)
#   a1 = out_offset_bytes (offset from RDRAM_OUT_BASE to pixel (0,0)
#                           of this strip's top-left block)
#   a2 = n_blocks (1..N_STRIP_BLOCKS_MAX)
# ============================================================
    .func cmd_lspr1_decode_strip
cmd_lspr1_decode_strip:
    #define out_offset   s5
    #define n_blocks     s6
    #define block_idx    s7
    #define row_stride   s8
    #define v_const      $v16
    #define v_ep0        $v20
    #define v_ep1        $v21
    #define v_tmp0       $v22
    # v_pal: packed RGBA5551 palette for the current block, pal[2] in lane e0,
    # pal[3] in lane e4.
    #define v_pal        $v24
    #define v_unpack     $v26
    # Low-half-only pack constants used by alpha_mode (0 in lanes 4..7 so the
    # packed pal[3] is 0 = transparent).
    #define v_pack_r_lo  $v07
    #define v_pack_g_lo  $v08
    #define v_pack_b_lo  $v23
    #define v_one_lo     $v25
    #define v_pack       $v28
    #define v_pack_r     $v17
    #define v_pack_g     $v18
    #define v_pack_b     $v19
    #define v_one        $v29
    # v_shl: per-lane index-extraction shifts (pixel lookup phase)
    #define v_shl        $v27
    # Per-block decode scratch (rspq reserves only $v00/$v30/$v31, so $v01..$v15
    # are available here). The 16 pixels are processed as two 8-pixel halves:
    # half A = pixels 0..7 (output rows 0/1), half B = pixels 8..15 (rows 2/3).
    #   v_pp0 / v_pp1  = pal[0] / pal[1], broadcast to all 8 lanes
    #   v_tA  / v_tB   = the two 16-bit index words (pixels 0..7 / 8..15) in lane 0
    #   v_idxA/ v_idxB = per-lane 2-bit pixel indices for half A / half B
    #   v_b0A / v_b0B  = index bit0 of half A / half B
    #   v_evA / v_odA  = half A even/odd palette candidates (idx&2 == 0 / != 0)
    #   v_evB / v_odB  = half B even/odd palette candidates
    #   v_rA  / v_rB   = final packed 8-pixel results of half A / half B
    #   v_junk         = throwaway vlt/vge destination (only VCC matters)
    #define v_tA         $v01
    #define v_tB         $v02
    #define v_idxA       $v03
    #define v_idxB       $v04
    #define v_pp0        $v05
    #define v_pp1        $v06
    #define v_b0A        $v09
    #define v_b0B        $v10
    #define v_evA        $v11
    #define v_odA        $v12
    #define v_evB        $v13
    #define v_odB        $v14
    #define v_junk       $v15
    # v_rA/v_rB share the even-candidate registers; v_tA/v_tB are reserved to hold
    # the next block's prefetched index words (see pixel_lookup).
    #define v_rA         v_evA
    #define v_rB         v_evB

    # Preserve args we need after DMAIn returns
    move out_offset, a1
    move n_blocks,   a2
    li   t0, %lo(BC1Q_VCONST)
    lqv  v_const, 0, 0, t0
    li   t0, %lo(BC1Q_PACKCONST)
    lqv  v_pack, 0, 0, t0
    li   t0, %lo(BC1Q_UNPACK)
    lqv  v_unpack, 0, 0, t0
    li   t0, %lo(BC1Q_LOOKUP)
    lqv  v_shl, 0, 0, t0
    li   t0, %lo(BC1Q_PACKLO)
    lqv  v_pack_r_lo, 0,  0, t0
    lqv  v_pack_g_lo, 0, 16, t0
    lqv  v_pack_b_lo, 0, 32, t0
    lqv  v_one_lo,    0, 48, t0
    vcopy v_pack_r,  v_pack.e0
    vcopy v_pack_g,  v_pack.e1
    vcopy v_pack_b,  v_pack.e2
    vcopy v_one,     v_const.e4   # 1 in all lanes: alpha bit via MAC

    # ---- DMA compressed blocks into INBUF ----
    lw  s0, %lo(RDRAM_IN_BASE)
    li  s4, %lo(INBUF)
    add s0, s0, a0
    sll t0, n_blocks, 3        # n_blocks * 8 (bytes)
    li  t1, 0                  # height=1, pitch ignored
    jal DMAIn
    addi t0, t0, -1            # DMA_SIZE wants width-1

    # ---- Decode loop ----
    # A block's 8-byte input (endpoints c0/c1 + the 32-bit index word) is fetched
    # one iteration ahead of where it is decoded: while block N is being decoded,
    # block N+1's input is loaded (in pixel_lookup) into the registers below. The
    # prologue loads block 0, then the loop body begins directly at the unpack.
    li  block_idx, 0
    sll row_stride, n_blocks, 3   # bytes per strip row in OUTBUF (= n_blocks * 8)

    # ---- Prologue: load block 0's input (same layout as the prefetch below) ----
    # c0/c1 go into lanes e0/e4 of the endpoint vectors (halves swapped between
    # ep0/ep1). The two 16-bit index words are read straight from DMEM into lane 0
    # of v_tA/v_tB: in the big-endian 32-bit index word, pixels 0..7 are the low
    # u16 (the last two bytes, offset 6) and pixels 8..15 the high u16 (offset 4).
    li   t6, %lo(INBUF)        # &INBUF[0]
    lhu  k0, 0(t6)             # c0  (BE u16 from disk == native u16 on RSP)
    lhu  k1, 2(t6)             # c1
    lsv  v_ep0.e0, 0, t6       # c0 -> ep0 lane 0 / ep1 lane 4 (halves swapped)
    lsv  v_ep0.e4, 2, t6       # c1
    lsv  v_ep1.e0, 2, t6       # c1
    lsv  v_ep1.e4, 0, t6       # c0
    lsv  v_tA.e0, 6, t6        # pixels 0..7  indices (low u16  @ byte 6)
    lsv  v_tB.e0, 4, t6        # pixels 8..15 indices (high u16 @ byte 4)
    sltu v1, k1, k0            # mode: v1 = (c0 > c1)

    # Loop body throughput (one block = 16 pixels): ~38 cycles in opaque mode,
    # ~37 in alpha mode, i.e. ~2.4 cycles per decoded pixel.
decode_block:
    # ---- Vectorized endpoint unpack (RGBA5551 -> per-channel 5/5/5 lanes) ----
    # Inputs were loaded by the previous iteration (or the prologue for block 0):
    # c0/c1 in lanes e0/e4 of v_ep0/v_ep1 (halves swapped), the two 16-bit index
    # words in lane 0 of v_tA/v_tB, and v1 = mode flag. The .h0 selector on the
    # endpoint vectors broadcasts c0/c1 within each half (lanes 0..3 = c0/ep0,
    # 4..7 = c1/ep1); .e0 broadcasts the index word across all lanes.
    #
    # Three things are produced:
    #   v_ep0/v_ep1 : each 5-bit channel logically shifted into bits [4:0] via the
    #                 vmudl multipliers (R>>11, G>>6, B>>1), then masked to 5 bits.
    #   v_pp0/v_pp1 : pal[0]/pal[1] = (c0 | alpha) / (c1 | alpha), one per lane.
    #   v_idxA/idxB : vmudn lands each pixel's 2-bit index in bits [14:15] of its
    #                 lane (the low bits are ignored later).
    # vor must read v_ep0 before vmudl overwrites it.
    vor   v_pp0, v_one,   v_ep0.h0                         # [pal0=c0|1 x4 | pal1=c1|1 x4]
    vmudl v_ep0, v_unpack, v_ep0.h0                        # ep0 channels -> [4:0]
    vmudl v_ep1, v_unpack, v_ep1.h0                        # ep1 channels -> [4:0]
    vmudn v_idxA, v_shl,  v_tA.e0                          # pixels 0..7  indices
    vmudn v_idxB, v_shl,  v_tB.e0                          # pixels 8..15 indices
    vand  v_ep0, v_ep0, v_const.e0                         # mask to 5 bits
    vand  v_ep1, v_ep1, v_const.e0
    vcopy v_pp1, v_pp0.e4                                  # pal[1] in all lanes
    # ---- Mode select: c0 > c1 -> opaque (4-color); else alpha (3-color + transp) ----
    beqz v1, alpha_mode
    vcopy v_pp0, v_pp0.e0                                  # pal[0] in all lanes

    # ============================================================
    # OPAQUE MODE
    #   pal[2] = ((2*r0+r1)/3, (2*g0+g1)/3, (2*b0+b1)/3, A=1)
    #   pal[3] = ((r0+2*r1)/3, (g0+2*g1)/3, (b0+2*b1)/3, A=1)
    # ============================================================
opaque_mode:
    # pal[2]/pal[3] interpolate the two endpoints at 1/3 and 2/3. The /3 is done
    # exactly as (sum*171)>>9: vmudm/vmadm accumulate the full 32-bit products in
    # the accumulator and expose ACC>>16 as vd, so the sum stays exact and the
    # rounding happens once. With a *21888 (= 171<<7) multiplier:
    #   ACC = (2*ep0 + ep1) * 21888 ; vd = ACC>>16 = (2*ep0+ep1)*171>>9 = sum/3
    # The endpoint vectors hold ep0 in lanes 0..2 and ep1 in lanes 4..6, so:
    #   lanes 0..2 => pal[2] = (2*ep0 + ep1)/3
    #   lanes 4..6 => pal[3] = (2*ep1 + ep0)/3
    vmudm v_tmp0, v_ep0, v_const.e7
    vmadm v_tmp0, v_ep0, v_const.e7
    vmadm v_tmp0, v_ep1, v_const.e7
    # Destination pointer for each of the 4 output rows (used by the stores in
    # pixel_lookup): row dy dst = OUTBUF + block_idx*8 + dy*row_stride.
    sll  t0, block_idx, 3
    addi t0, t0, %lo(OUTBUF)             # row 0
    add  t1, t0, row_stride              # row 1
    add  t2, t1, row_stride              # row 2
    add  t3, t2, row_stride              # row 3

    # ---- Pack pal[2] and pal[3] into v_pal in one vector pass ----
    # All channels are 5-bit (RGBA5551), so v_tmp0 feeds the packers directly.
    # Three MACs shift each channel into place (R<<11, G<<6, B<<1) and OR them by
    # accumulation; a 4th MAC term (+1*1) sets the RGBA5551 alpha bit. Lane e0 ends
    # up holding the packed pal[2], lane e4 the packed pal[3].
    vmudn v_pal, v_pack_r, v_tmp0.h0
    vmadn v_pal, v_pack_g, v_tmp0.h1
    vmadn v_pal, v_pack_b, v_tmp0.h2
    j   pixel_lookup
    vmadn v_pal, v_one,    v_one   # +alpha bit (delay slot)

    # ============================================================
    # ALPHA MODE
    #   pal[2] = ((r0+r1)/2, (g0+g1)/2, (b0+b1)/2, A=1)
    #   pal[3] = 0x0000 (fully transparent)
    # ============================================================
alpha_mode:
    # pal[2] is the midpoint (ep0 + ep1)/2 of the two endpoints. As with the
    # opaque /3, the >>1 is done via the accumulator: a *0x8000 (= 1<<15) MAC
    # chain accumulates (ep0+ep1)*0x8000 and vd = ACC>>16 = (ep0+ep1)>>1, exact.
    vmudm v_tmp0, v_ep0, v_const.e5
    vmadm v_tmp0, v_ep1, v_const.e5
    # Output-row destination pointers (see opaque_mode).
    sll  t0, block_idx, 3
    addi t0, t0, %lo(OUTBUF)             # row 0
    add  t1, t0, row_stride              # row 1
    add  t2, t1, row_stride              # row 2
    add  t3, t2, row_stride              # row 3

    # ---- Pack pal[2] into v_pal; pal[3] is transparent ----
    # Same 3-MAC channel pack as opaque_mode (R<<11, G<<6, B<<1) plus the alpha
    # bit, but using the low-half-only pack constants (0 in lanes 4..7). The result
    # is the packed pal[2] in lane e0 and 0 in lane e4 (pal[3] = fully transparent).
    vmudn v_pal, v_pack_r_lo, v_tmp0.e0
    vmadn v_pal, v_pack_g_lo, v_tmp0.e1
    vmadn v_pal, v_pack_b_lo, v_tmp0.e2
    vmadn v_pal, v_one_lo,    v_one_lo

    # ============================================================
    # PIXEL LOOKUP & STORE (vectorized)
    # For each of 16 pixels, idx = (indices >> (2*pixel)) & 3, and
    # OUTBUF[dy=pixel/4][block=block_idx][dx=pixel%4] = pal[idx].
    # OUTBUF layout: row-interleaved across the strip, so
    #   dst_row[dy] = OUTBUF + dy*row_stride + block_idx*8
    #
    # The 16 indices are already in v_idxA (pixels 0..7) and v_idxB (pixels 8..15)
    # from the unpack phase, with each pixel's 2-bit index sitting in bits [14:15]
    # of its lane. pal[0]/pal[1] are in the broadcasts v_pp0/v_pp1; pal[2]/pal[3]
    # are in v_pal.e0/.e4. Each half selects pal[idx] per lane with two bit-keyed
    # vmrg merges (vmrg vd = VCC ? vs : vt), driven by the two index bits:
    #   bit1 = sign(idx): vge idx,0 -> VCC=(idx<2); picks even(pal0/pal2)/odd(pal1/pal3)
    #   bit0 = idx & 0x4000 (=K16384): vlt 0,bit0 -> VCC; picks within the pair
    # The 8 packed RGBA5551 pixels of each half are then stored as two rows by sdv.
    # ============================================================
pixel_lookup:
    # While this block's gather runs, the right column prefetches the NEXT block's
    # input into the registers freed by this block (v_ep0/v_ep1 and v_tA/v_tB):
    # endpoints, the two index words, and the mode flag, exactly as the prologue
    # loaded block 0. The next INBUF address is INBUF + (block_idx+1)*8. On the
    # final iteration this reads one block past the data, which is harmless as it
    # is never decoded.
    vge   v_junk, v_idxA, vzero;    sll   t6, block_idx, 3          # VCC=(idxA<2); \ next INBUF
    vand  v_b0A,  v_idxA, K16384;   addi  t6, t6, %lo(INBUF) + 8    # bit0A;        / address
    vand  v_b0B,  v_idxB, K16384;   lhu   k0, 0(t6)                 # bit0B; next c0
    vmrg  v_evA,  v_pp0, v_pal.e0;  lhu   k1, 2(t6)                 # evenA; next c1
    vmrg  v_odA,  v_pp1, v_pal.e4;  lsv   v_tA.e0, 6, t6            # oddA;  idx pixels 0..7
    vge   v_junk, v_idxB, vzero;    lsv   v_tB.e0, 4, t6            # VCC=(idxB<2); idx pixels 8..15
    vmrg  v_evB,  v_pp0, v_pal.e0;  lsv   v_ep0.e0, 0, t6           # evenB; c0->ep0.e0
    vmrg  v_odB,  v_pp1, v_pal.e4;  lsv   v_ep0.e4, 2, t6           # oddB;  c1->ep0.e4
    vlt   v_junk, vzero, v_b0A;     lsv   v_ep1.e0, 2, t6           # VCC=bit0A; c1->ep1.e0
    vmrg  v_rA,   v_odA, v_evA;     lsv   v_ep1.e4, 0, t6           # half A done; c0->ep1.e4
    vlt   v_junk, vzero, v_b0B;     sltu  v1, k1, k0                # VCC=bit0B; next mode
    vmrg  v_rB,   v_odB, v_evB;     addi  block_idx, block_idx, 1   # half B done; loop bump
    sdv   v_rA.e0, 0, t0                  # pixels 0..3  -> row 0
    sdv   v_rA.e4, 0, t1                  # pixels 4..7  -> row 1
    sdv   v_rB.e0, 0, t2                  # pixels 8..11 -> row 2
    bne   block_idx, n_blocks, decode_block                       # loop
    sdv   v_rB.e4, 0, t3                  # pixels 12..15-> row 3 (delay slot)

    # ============================================================
    # DMA the 4 output rows back to RDRAM.
    # Row r destination = RDRAM_OUT_BASE + out_offset + r*RDRAM_OUT_STRIDE.
    # Each row carries n_blocks*8 bytes (= row_stride).
    # ============================================================
    lw  s1, %lo(RDRAM_OUT_BASE)
    lw  s2, %lo(RDRAM_OUT_STRIDE)
    add s1, s1, out_offset     # s1 = row 0 RDRAM dst

    # row_stride - 1 (for DMA_SIZE width field)
    addi t0, row_stride, -1

    # Row 0
    move s0, s1
    li   s4, %lo(OUTBUF)
    li   t1, 0
    jal  DMAOutAsync
    nop

    # Row 1
    add  s1, s1, s2
    move s0, s1
    li   s4, %lo(OUTBUF)
    add  s4, s4, row_stride
    jal  DMAOutAsync
    nop

    # Row 2
    add  s1, s1, s2
    move s0, s1
    li   s4, %lo(OUTBUF)
    add  s4, s4, row_stride
    add  s4, s4, row_stride
    jal  DMAOutAsync
    nop

    # Row 3 (final — sync)
    add  s1, s1, s2
    move s0, s1
    li   s4, %lo(OUTBUF)
    add  s4, s4, row_stride
    add  s4, s4, row_stride
    add  s4, s4, row_stride
    jal_and_j DMAOut, RSPQ_Loop

    #undef out_offset
    #undef n_blocks
    #undef block_idx
    #undef row_stride
    #undef v_const
    #undef v_ep0
    #undef v_ep1
    #undef v_tmp0
    #undef v_pal
    #undef v_unpack
    #undef v_pack_r_lo
    #undef v_pack_g_lo
    #undef v_pack_b_lo
    #undef v_one_lo
    #undef v_pack
    #undef v_pack_r
    #undef v_pack_g
    #undef v_pack_b
    #undef v_one
    #undef v_shl
    #undef v_pp0
    #undef v_pp1
    #undef v_tA
    #undef v_tB
    #undef v_idxA
    #undef v_idxB
    #undef v_b0A
    #undef v_b0B
    #undef v_evA
    #undef v_odA
    #undef v_evB
    #undef v_odB
    #undef v_rA
    #undef v_rB
    #undef v_junk
    .endfunc
