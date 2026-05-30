############################################################
# rsp_bcsp.S
#
# RSP ucode for BC1/DXT1 (BCSP) sprite decoding. Decodes BC1
# block-compressed data into RGBA5551 pixels in RDRAM via SP DMA.
#
# Pipeline:
#   1. CPU calls cmd_bcsp_set_buffers once per sprite to record the
#      RDRAM physical addresses of the compressed payload and the
#      decoded RGBA5551 destination, plus the destination row stride.
#   2. CPU then enqueues a series of cmd_bcsp_decode_strip commands,
#      one per horizontal strip of up to N_STRIP_BLOCKS_MAX blocks.
#      Each strip DMAs in its compressed blocks, decodes them with a
#      scalar per-block kernel, and DMAs the four output pixel rows
#      back to RDRAM at the appropriate destination row offsets.
#
# The per-block math mirrors src/bcsprite.c::bcsp_decode_block() bit
# for bit so the RSP output is bit-exact with the reference C decoder.
############################################################

#include <rsp_queue.inc>

    .set noreorder
    .set noat

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand cmd_bcsp_set_buffers,    12
        RSPQ_DefineCommand cmd_bcsp_decode_strip,   12
    RSPQ_EndOverlayHeader

    .align 4
    .ascii " Dragon RSP BCSP"

    RSPQ_BeginSavedState
RDRAM_IN_BASE:    .long 0     # phys addr of payload[] (8-byte aligned)
RDRAM_OUT_BASE:   .long 0     # phys addr of decoded RGBA5551 region
RDRAM_OUT_STRIDE: .long 0     # bytes between RGBA rows (= width * 2)
    RSPQ_EndSavedState

# Divide-by-3 lookup table. Entry [n] = n / 3 for n in [0, 255].
# Used by opaque-mode endpoint interpolation: (2*p0+p1)/3 and (p0+2*p1)/3.
# Max numerator for our 5-bit channels is 2*31+31 = 93; for our 6-bit channel
# is 2*63+63 = 189. Both fit in a single 8-bit table indexed by the sum.
    .align 4
DIV3_TBL:
    .byte   0,   0,   0,   1,   1,   1,   2,   2,   2,   3,   3,   3,   4,   4,   4,   5
    .byte   5,   5,   6,   6,   6,   7,   7,   7,   8,   8,   8,   9,   9,   9,  10,  10
    .byte  10,  11,  11,  11,  12,  12,  12,  13,  13,  13,  14,  14,  14,  15,  15,  15
    .byte  16,  16,  16,  17,  17,  17,  18,  18,  18,  19,  19,  19,  20,  20,  20,  21
    .byte  21,  21,  22,  22,  22,  23,  23,  23,  24,  24,  24,  25,  25,  25,  26,  26
    .byte  26,  27,  27,  27,  28,  28,  28,  29,  29,  29,  30,  30,  30,  31,  31,  31
    .byte  32,  32,  32,  33,  33,  33,  34,  34,  34,  35,  35,  35,  36,  36,  36,  37
    .byte  37,  37,  38,  38,  38,  39,  39,  39,  40,  40,  40,  41,  41,  41,  42,  42
    .byte  42,  43,  43,  43,  44,  44,  44,  45,  45,  45,  46,  46,  46,  47,  47,  47
    .byte  48,  48,  48,  49,  49,  49,  50,  50,  50,  51,  51,  51,  52,  52,  52,  53
    .byte  53,  53,  54,  54,  54,  55,  55,  55,  56,  56,  56,  57,  57,  57,  58,  58
    .byte  58,  59,  59,  59,  60,  60,  60,  61,  61,  61,  62,  62,  62,  63,  63,  63
    .byte  64,  64,  64,  65,  65,  65,  66,  66,  66,  67,  67,  67,  68,  68,  68,  69
    .byte  69,  69,  70,  70,  70,  71,  71,  71,  72,  72,  72,  73,  73,  73,  74,  74
    .byte  74,  75,  75,  75,  76,  76,  76,  77,  77,  77,  78,  78,  78,  79,  79,  79
    .byte  80,  80,  80,  81,  81,  81,  82,  82,  82,  83,  83,  83,  84,  84,  84,  85

# Scratch storage. PAL_STACK holds the 4 RGBA5551 palette entries
# for the block currently being decoded; IDX_SAVE holds the 32-bit
# indices word so we don't burn a callee-saved register on it.
    .align 4
PAL_STACK:  .ds.b 8
IDX_SAVE:   .word 0

# Strip buffers.
.equ N_STRIP_BLOCKS_MAX, 32

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
# cmd_bcsp_set_buffers
#   a0 = RDRAM_IN_BASE  (phys addr of payload[])
#   a1 = RDRAM_OUT_BASE (phys addr of decoded RGBA5551 region)
#   a2 = RDRAM_OUT_STRIDE (bytes per RGBA row = width * 2)
# ============================================================
    .func cmd_bcsp_set_buffers
cmd_bcsp_set_buffers:
    sw a0, %lo(RDRAM_IN_BASE)
    sw a1, %lo(RDRAM_OUT_BASE)
    jr  ra
    sw a2, %lo(RDRAM_OUT_STRIDE)
    .endfunc


# ============================================================
# cmd_bcsp_decode_strip
#   a0 = in_offset_bytes  (offset from RDRAM_IN_BASE  to first block)
#   a1 = out_offset_bytes (offset from RDRAM_OUT_BASE to pixel (0,0)
#                           of this strip's top-left block)
#   a2 = n_blocks (1..N_STRIP_BLOCKS_MAX)
# ============================================================
    .func cmd_bcsp_decode_strip
cmd_bcsp_decode_strip:
    #define out_offset   s5
    #define n_blocks     s6
    #define block_idx    s7
    #define row_stride   s8

    # Preserve args we need after DMAIn returns
    move out_offset, a1
    move n_blocks,   a2

    # ---- DMA compressed blocks into INBUF ----
    lw  s0, %lo(RDRAM_IN_BASE)
    add s0, s0, a0
    li  s4, %lo(INBUF)
    sll t0, n_blocks, 3        # n_blocks * 8 (bytes)
    li  t1, 0                  # height=1, pitch ignored
    jal DMAIn
    addi t0, t0, -1            # DMA_SIZE wants width-1

    # ---- Decode loop ----
    li  block_idx, 0
    sll row_stride, n_blocks, 3   # bytes per strip row in OUTBUF (= n_blocks * 8)

decode_block:
    # ---- Load the 8-byte block from INBUF ----
    sll t0, block_idx, 3
    addi t0, t0, %lo(INBUF)    # t0 = &INBUF[block_idx * 8]
    lhu k0, 0(t0)              # c0  (BE u16 from disk == native u16 on RSP)
    lhu k1, 2(t0)              # c1
    lw  v0, 4(t0)              # indices (BE u32 from disk)
    sw  v0, %lo(IDX_SAVE)      # park indices for the pixel-store phase

    # ---- Unpack c0 RGB565 -> 5/6/5 ----
    srl t3, k0, 11             # r0
    andi t3, t3, 0x1F
    srl t4, k0, 5              # g0 (6 bits)
    andi t4, t4, 0x3F
    andi t5, k0, 0x1F          # b0

    # ---- Unpack c1 RGB565 -> 5/6/5 ----
    srl t6, k1, 11             # r1
    andi t6, t6, 0x1F
    srl t7, k1, 5              # g1 (6 bits)
    andi t7, t7, 0x3F
    andi t8, k1, 0x1F          # b1

    # ---- Build pal[0] = (r0<<11) | ((g0>>1)<<6) | (b0<<1) | 1 ----
    sll a0, t3, 11
    srl t9, t4, 1
    sll t9, t9, 6
    or  a0, a0, t9
    sll t9, t5, 1
    or  a0, a0, t9
    ori a0, a0, 1
    sh  a0, %lo(PAL_STACK)+0

    # ---- Build pal[1] = (r1<<11) | ((g1>>1)<<6) | (b1<<1) | 1 ----
    sll a1, t6, 11
    srl t9, t7, 1
    sll t9, t9, 6
    or  a1, a1, t9
    sll t9, t8, 1
    or  a1, a1, t9
    ori a1, a1, 1
    sh  a1, %lo(PAL_STACK)+2

    # ---- Mode select: c0 > c1 -> opaque (4-color); else alpha (3-color + transp) ----
    sltu v1, k1, k0            # v1 = (c1 < c0) = (c0 > c1)
    beqz v1, alpha_mode
    li   v0, %lo(DIV3_TBL)     # delay slot: preload DIV3 base

    # ============================================================
    # OPAQUE MODE
    #   pal[2] = ((2*r0+r1)/3, (2*g0+g1)/3, (2*b0+b1)/3, A=1)
    #   pal[3] = ((r0+2*r1)/3, (g0+2*g1)/3, (b0+2*b1)/3, A=1)
    # ============================================================
opaque_mode:
    # ---- pal[2] R = DIV3_TBL[2*r0 + r1] ----
    sll a2, t3, 1
    add a2, a2, t6
    add a2, a2, v0
    lbu a2, 0(a2)
    sll a2, a2, 11

    # ---- pal[2] G ----
    sll t9, t4, 1
    add t9, t9, t7
    add t9, t9, v0
    lbu t9, 0(t9)              # 6-bit g2
    srl t9, t9, 1              # collapse to 5-bit
    sll t9, t9, 6
    or  a2, a2, t9

    # ---- pal[2] B ----
    sll t9, t5, 1
    add t9, t9, t8
    add t9, t9, v0
    lbu t9, 0(t9)
    sll t9, t9, 1
    or  a2, a2, t9
    ori a2, a2, 1
    sh  a2, %lo(PAL_STACK)+4

    # ---- pal[3] R = DIV3_TBL[r0 + 2*r1] ----
    add a3, t3, t6
    add a3, a3, t6
    add a3, a3, v0
    lbu a3, 0(a3)
    sll a3, a3, 11

    # ---- pal[3] G ----
    add t9, t4, t7
    add t9, t9, t7
    add t9, t9, v0
    lbu t9, 0(t9)
    srl t9, t9, 1
    sll t9, t9, 6
    or  a3, a3, t9

    # ---- pal[3] B ----
    add t9, t5, t8
    add t9, t9, t8
    add t9, t9, v0
    lbu t9, 0(t9)
    sll t9, t9, 1
    or  a3, a3, t9
    ori a3, a3, 1
    j   pixel_lookup
    sh  a3, %lo(PAL_STACK)+6   # delay slot

    # ============================================================
    # ALPHA MODE
    #   pal[2] = ((r0+r1)/2, (g0+g1)/2, (b0+b1)/2, A=1)
    #   pal[3] = 0x0000 (fully transparent)
    # ============================================================
alpha_mode:
    # ---- pal[2] R = (r0+r1)/2 ----
    add a2, t3, t6
    srl a2, a2, 1
    sll a2, a2, 11

    # ---- pal[2] G ----
    add t9, t4, t7
    srl t9, t9, 1              # 6-bit g2
    srl t9, t9, 1              # collapse to 5-bit
    sll t9, t9, 6
    or  a2, a2, t9

    # ---- pal[2] B ----
    add t9, t5, t8
    srl t9, t9, 1
    sll t9, t9, 1
    or  a2, a2, t9
    ori a2, a2, 1
    sh  a2, %lo(PAL_STACK)+4

    # pal[3] = 0
    sh  zero, %lo(PAL_STACK)+6

    # ============================================================
    # PIXEL LOOKUP & STORE
    # For each of 16 pixels, idx = (indices >> (2*pixel)) & 3, and
    # OUTBUF[dy=pixel/4][block=block_idx][dx=pixel%4] = pal[idx].
    # OUTBUF layout: row-interleaved across the strip, so
    #   dst_row[dy] = OUTBUF + dy*row_stride + block_idx*8
    # ============================================================
pixel_lookup:
    # ---- Compute row base pointers ----
    sll  t0, block_idx, 3
    addi t0, t0, %lo(OUTBUF)   # t0 = OUTBUF + block_idx*8 (= row 0 dst)
    add  t1, t0, row_stride    # t1 = row 1 dst
    add  t2, t1, row_stride    # t2 = row 2 dst
    add  t3, t2, row_stride    # t3 = row 3 dst
    li   t4, %lo(PAL_STACK)
    lw   v1, %lo(IDX_SAVE)     # restore indices

    # ---- Macro: emit one pixel (idx_shift, row_ptr, byte_offset) ----
    # Extracts bits[1:0] of (v1 >> idx_shift), uses as halfword index into
    # PAL_STACK, and stores the loaded value to (row_ptr + byte_offset).
    .macro BCSP_PIX  shift, rowptr, off
        srl  t5, v1, \shift
        andi t5, t5, 3
        sll  t5, t5, 1
        add  t5, t5, t4
        lhu  t6, 0(t5)
        sh   t6, \off(\rowptr)
    .endm

    # Row 0: pixels 0..3
    BCSP_PIX  0, t0, 0
    BCSP_PIX  2, t0, 2
    BCSP_PIX  4, t0, 4
    BCSP_PIX  6, t0, 6
    # Row 1: pixels 4..7
    BCSP_PIX  8, t1, 0
    BCSP_PIX 10, t1, 2
    BCSP_PIX 12, t1, 4
    BCSP_PIX 14, t1, 6
    # Row 2: pixels 8..11
    BCSP_PIX 16, t2, 0
    BCSP_PIX 18, t2, 2
    BCSP_PIX 20, t2, 4
    BCSP_PIX 22, t2, 6
    # Row 3: pixels 12..15
    BCSP_PIX 24, t3, 0
    BCSP_PIX 26, t3, 2
    BCSP_PIX 28, t3, 4
    BCSP_PIX 30, t3, 6

    # ---- Advance to next block in strip ----
    addi block_idx, block_idx, 1
    bne  block_idx, n_blocks, decode_block
    nop

    # ============================================================
    # DMA the 4 output rows back to RDRAM.
    # Row r destination = RDRAM_OUT_BASE + out_offset + r*RDRAM_OUT_STRIDE.
    # Each row carries n_blocks*8 bytes (= row_stride).
    # ============================================================
    lw  s1, %lo(RDRAM_OUT_BASE)
    add s1, s1, out_offset     # s1 = row 0 RDRAM dst
    lw  s2, %lo(RDRAM_OUT_STRIDE)

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
    .endfunc
