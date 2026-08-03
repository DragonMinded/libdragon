################################################################################
# rsp_ulc.S
#
# Authors: Dominic Szablewski <https://phoboslab.org>
#
# ULC RSP overlay: IMDCT, overlap-add, output scaling, and stereo interleaving.
# Signal processing uses signed 16-bit fixed-point values with 14 fractional
# bits (Q1.14).
################################################################################

#include <rsp_queue.inc>

.set noreorder
.set noat
.set nomacro

.data
  RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand synthesize, 16
    RSPQ_DefineCommand stereo_interleave, 12
  RSPQ_EndOverlayHeader

  RSPQ_EmptySavedState

    .align 1
    ROTATIONS: .half 11585, 11585, 0, 16384, 15137, 6270, 11585, 11585, 16069, 3196, 15137, 6270, 16305, 1606, 16069, 3196, 16364, 804, 16305, 1606, 16379, 402, 16364, 804, 16383, 201, 16379, 402, 16384, 101, 16383, 201, 16384, 50, 16384, 101, 16384, 25, 16384, 50, 16384, 13, 16384, 25, 16384, 6, 16384, 13, 16384, 3, 16384, 6, 16384, 2, 16384, 3, 16384, 1, 16384, 2, 16384, 0, 16384, 1, 16384, 0, 16384, 0
    .align 4
    WINDOW_C: .half 16305, 15679, 14449, 12665, 10394, 7723, 4756, 1606, 16364, 16207, 15893, 15426, 14811, 14053, 13160, 12140, 16379, 16340, 16261, 16143, 15986, 15791, 15557, 15286, 16383, 16373, 16353, 16324, 16284, 16235, 16176, 16107, 16384, 16381, 16376, 16369, 16359, 16347, 16332, 16315, 16384, 16383, 16382, 16380, 16378, 16375, 16371, 16367, 16384, 16384, 16384, 16383, 16382, 16382, 16381, 16380, 16384, 16384, 16384, 16384, 16384, 16383, 16383, 16383
    .align 4
    WINDOW_S: .half 1606, 4756, 7723, 10394, 12665, 14449, 15679, 16305, 804, 2404, 3981, 5520, 7005, 8423, 9760, 11003, 402, 1205, 2006, 2801, 3590, 4370, 5139, 5897, 201, 603, 1005, 1406, 1806, 2205, 2603, 2999, 101, 302, 503, 704, 904, 1105, 1306, 1506, 50, 151, 251, 352, 452, 553, 653, 754, 25, 75, 126, 176, 226, 276, 327, 377, 13, 38, 63, 88, 113, 138, 163, 188
    .align 1
    WINDOW_JUMP: .half 0, 16384, 11585, 11585, 15137, 6270, 16069, 3196, 16305, 1606, 16364, 804, 16379, 402, 16383, 201

.bss
  TEMP_STATE_MEM_START:
    .align 1
    # WORK and OUTPUT form one contiguous 2048-byte arena during a maximum-size
    # transform. OUTPUT is only independent scratch after the transform ends.
    WORK: .ds.b 1024
    .align 1
    OUTPUT: .ds.b 1024
    .align 1
    LAP: .ds.b 1024
  TEMP_STATE_MEM_END:

.text

#define coeff_rdram       a0
#define lap_rdram         a1
#define output_rdram      a2
#define command_flags     a3
#define transform_size    s7
#define transform_bytes   s6

#define FLAG_TRANSFORM       0x80000000
#define FLAG_LAP             0x40000000
#define FLAG_CLEAR_LAP       0x20000000
#define FLAG_DISCARD_OUTPUT  0x10000000
#define FLAG_SCALE_SAMPLES   0x08000000
#define FLAGS_SHIFT     27

# ------------------------------------------------------------------------------
# Synthesize command
# performs imdct, clearing the lap buffer, lapping and output scaling depending 
# on the input flags.
# ------------------------------------------------------------------------------

synthesize:
  # Bits 0..10 contain the transform size, bits 16..26 the overlap size,
  # and bits 27..31 the operation flags defined above.
  andi    transform_size, command_flags, 0x07ff

  # DMA the transform coefficients into DMEM.
  sll     transform_bytes, transform_size, 1
  addiu   t0, transform_bytes, -1
  ori     s4, zero, %lo(WORK)
  addiu   t2, zero, 12
  jal     DMAExec
  or      s0, zero, coeff_rdram

  # TRANSFORM is the sign bit, allowing a direct signed branch here.
  # Skip it when the input already contains transformed samples.
  bgez    command_flags, .imdct_transform_done
  srl     s5, transform_size, 1

  jal     swap_odd_reverse
  sll     s3, transform_size, 1

  jal     load_rotation_step
  or      v0, zero, s5
  vor     $v27, $v00, $v01
  jal     load_rotation_4
  vor     $v28, $v00, $v02

  ori     s2, zero, %lo(WORK)
  ori     s1, zero, %lo(WORK)
  addu    s1, s1, transform_bytes
  addiu   t3, zero, 0x55
  ctc2    t3, COP2_CTRL_VCC
  jal     pre_twiddle
  vsub    $v22, $v00, $v23.v

# Fused radix-4/radix-2 DIF FFT. Vector stages run from largest to smallest,
# followed by scalar length-4/2 stages. The result is reordered from
# bit-reversed to natural order before post-twiddling.

#define complex_count   $21
#define fft_length      $16
#define fft_half        $30
#define rotation_size   $2
#define rotation_cursor $10
#define radix2_pending  $9
#define fft_quarter     $8
#define fft_block       $26
#define stage_end       $25
#define odd_offset      $29
#define block_stride    $27
#define even_ptr        $24
#define odd_ptr         $15
#define butterfly       $14
#define radix4_ptr1     $13
#define radix4_ptr2     $12
#define radix4_ptr3     $11


  or      fft_length, complex_count, $0
  andi    radix2_pending, complex_count, 0x2a0
  srl     rotation_size, fft_length, 2
  addiu   rotation_cursor, $0, %lo(ROTATIONS) - 8
.fft_rotation_address:
  srl     rotation_size, rotation_size, 1
  bnez    rotation_size, .fft_rotation_address
  addiu   rotation_cursor, rotation_cursor, 8

.fft_stage_setup:
  srl     fft_half, fft_length, 1

  # Load and broadcast this stage's one-step twiddle rotation.
  lsv     $v26.e0, 4, rotation_cursor
  lsv     $v25.e0, 6, rotation_cursor
  addiu   rotation_cursor, rotation_cursor, -8
  sll     odd_offset, fft_half, 2
  vor     $v26, $v00, $v26.e0
  vor     $v25, $v00, $v25.e0

  # Build four consecutive twiddles and their four-step jump.
  vxor    $v19, $v00, $v31.e1
  vxor    $v18, $v00, $v00
  vsub    $v17, $v00, $v25.v
  vxor    $v23, $v00, $v00
  vxor    $v22, $v00, $v00
  vmov    $v23.e0, $v19.e0
  vmov    $v23.e1, $v19.e0
  vmov    $v22.e0, $v18.e0
  vmov    $v22.e1, $v18.e0

  # r0 -> r1
  vor     $v16, $v00, $v19
  vor     $v15, $v00, $v18
  vmulf   $v19, $v16, $v26.v
  vmacf   $v19, $v15, $v17.v
  vmulf   $v18, $v16, $v25.v
  vmacf   $v18, $v15, $v26.v
  vadd    $v19, $v19, $v19.v
  vadd    $v18, $v18, $v18.v
  vmov    $v23.e2, $v19.e0
  vmov    $v23.e3, $v19.e0
  vmov    $v22.e2, $v18.e0
  vmov    $v22.e3, $v18.e0

  # r1 -> r2
  vor     $v16, $v00, $v19
  vor     $v15, $v00, $v18
  vmulf   $v19, $v16, $v26.v
  vmacf   $v19, $v15, $v17.v
  vmulf   $v18, $v16, $v25.v
  vmacf   $v18, $v15, $v26.v
  vadd    $v19, $v19, $v19.v
  vadd    $v18, $v18, $v18.v
  vmov    $v23.e4, $v19.e0
  vmov    $v23.e5, $v19.e0
  vmov    $v22.e4, $v18.e0
  vmov    $v22.e5, $v18.e0

  # r2 -> r3
  vor     $v16, $v00, $v19
  vor     $v15, $v00, $v18
  vmulf   $v19, $v16, $v26.v
  vmacf   $v19, $v15, $v17.v
  vmulf   $v18, $v16, $v25.v
  vmacf   $v18, $v15, $v26.v
  vadd    $v19, $v19, $v19.v
  vadd    $v18, $v18, $v18.v
  vmov    $v23.e6, $v19.e0
  vmov    $v23.e7, $v19.e0
  vmov    $v22.e6, $v18.e0
  vmov    $v22.e7, $v18.e0

  # Reuse r3 and advance once for the four-twiddle jump r4.
  vor     $v16, $v00, $v19
  vor     $v15, $v00, $v18
  vmulf   $v19, $v16, $v26.v
  vmacf   $v19, $v15, $v17.v
  vmulf   $v18, $v16, $v25.v
  vmacf   $v18, $v15, $v26.v
  vadd    $v19, $v19, $v19.v
  vadd    $v18, $v18, $v18.v
  vor     $v21, $v00, $v19
  vor     $v20, $v00, $v18

  vxor    $v19, $v00, $v00.e0
  vsub    $v19, $v19, $v20.v

  # Set up the byte offsets and stage bounds shared by both kernels.
  sll     block_stride, fft_length, 2
  ori     fft_block, $0, %lo(WORK)
  ori     stage_end, $0, %lo(WORK)
  sll     even_ptr, complex_count, 2
  addu    stage_end, stage_end, even_ptr
  beqz    radix2_pending, .fft_radix4_setup
  nop

  # Process every four-butterfly group in this FFT stage.
  vor     $v28, $v00, $v23

.fft_vector_block:
  vor     $v27, $v00, $v22
  or      even_ptr, fft_block, $0
  addu    odd_ptr, fft_block, odd_offset
  or      butterfly, $0, $0

.fft_vector_butterfly:
  lqv     $v18, 0, 0, even_ptr
  lqv     $v17, 0, 0, odd_ptr
  vor     $v15, $v00, $v18.q1
  vor     $v16, $v00, $v18.q0
  addiu   butterfly, butterfly, 4

  # DIF butterfly: sum stays in the even half; rotate the difference.
  vsub    $v12, $v00, $v17.q0
  vsub    $v11, $v00, $v17.q1
  vsub    $v12, $v16, $v12.v
  vsub    $v11, $v15, $v11.v
  vsub    $v10, $v16, $v17.q0
  vsub    $v09, $v15, $v17.q1

  vsub    $v08, $v00, $v27.v
  vmulf   $v14, $v10, $v28.v
  vmacf   $v14, $v09, $v27.v
  vmulf   $v13, $v09, $v28.v
  vmacf   $v13, $v10, $v08.v

  # Pack the sum into a dead input register. This leaves v18/v17 available for
  # the twiddle copies while the multiply results become readable.
  vmrg    $v16, $v12, $v11
  vadd    $v14, $v14, $v14.v
  vadd    $v13, $v13, $v13.v
  vor     $v18, $v00, $v28
  vor     $v17, $v00, $v27
  sqv     $v16, 0, 0, even_ptr
  vmrg    $v16, $v14, $v13

  vmulf   $v28, $v18, $v21.v
  vmacf   $v28, $v17, $v19.v
  vmulf   $v27, $v18, $v20.v
  sqv     $v16, 0, 0, odd_ptr
  vmacf   $v27, $v17, $v21.v
  addiu   even_ptr, even_ptr, 16
  addiu   odd_ptr, odd_ptr, 16
  vadd    $v28, $v28, $v28.v
  bne     butterfly, fft_half, .fft_vector_butterfly
  vadd    $v27, $v27, $v27.v

  addu    fft_block, fft_block, block_stride
  bne     fft_block, stage_end, .fft_vector_block
  vor     $v28, $v00, $v23

  or      radix2_pending, $0, $0
  b       .fft_stage_setup
  srl     fft_length, fft_length, 1

.fft_radix4_setup:
  srl     fft_quarter, fft_length, 2
  vor     $v28, $v00, $v23

.fft_radix4_block:
  vor     $v27, $v00, $v22
  or      even_ptr, fft_block, $0
  addu    radix4_ptr1, even_ptr, fft_length
  addu    radix4_ptr2, radix4_ptr1, fft_length
  addu    radix4_ptr3, radix4_ptr2, fft_length
  or      butterfly, $0, $0

.fft_radix4_butterfly:
  # r2 = r * r for the second fused DIF level.
  vsub    $v24, $v00, $v27.v

  # Interleave the four input loads with the independent r2 calculation.
  lqv     $v29, 0, 0, even_ptr
  vmulf   $v26, $v28, $v28.v
  lqv     $v30, 0, 0, radix4_ptr1
  lqv     $v14, 0, 0, radix4_ptr2
  lqv     $v12, 0, 0, radix4_ptr3
  vmacf   $v26, $v27, $v24.v
  addiu   butterfly, butterfly, 4
  vmulf   $v25, $v28, $v27.v
  vmacf   $v25, $v28, $v27.v
  vor     $v18, $v00, $v29.q0
  vadd    $v26, $v26, $v26.v
  vor     $v15, $v00, $v30.q1
  vadd    $v25, $v25, $v25.v
  vor     $v16, $v00, $v30.q0
  vor     $v17, $v00, $v29.q1
  vsub    $v04, $v00, $v25.v

  # First DIF level: (x0,x2), rotated by r.
  vsub    $v06, $v18, $v14.q0
  vsub    $v05, $v17, $v14.q1
  vsub    $v02, $v00, $v14.q0
  vsub    $v01, $v00, $v14.q1
  vmulf   $v10, $v06, $v28.v
  vmacf   $v10, $v05, $v27.v
  vmulf   $v09, $v05, $v28.v
  vmacf   $v09, $v06, $v24.v
  vsub    $v18, $v18, $v02.v
  vsub    $v17, $v17, $v01.v
  vadd    $v10, $v10, $v10.v
  vadd    $v09, $v09, $v09.v

  # First DIF level: (x1,x3), rotated by r * -i.
  vsub    $v06, $v16, $v12.q0
  vsub    $v05, $v15, $v12.q1
  vsub    $v02, $v00, $v12.q0
  vsub    $v01, $v00, $v12.q1
  vsub    $v03, $v00, $v06.v
  vmulf   $v08, $v05, $v28.v
  vmacf   $v08, $v06, $v24.v
  vmulf   $v07, $v03, $v28.v
  vmacf   $v07, $v05, $v24.v
  vsub    $v16, $v16, $v02.v
  vsub    $v15, $v15, $v01.v
  vadd    $v08, $v08, $v08.v
  vadd    $v07, $v07, $v07.v

  # Second DIF level for the sums, rotated by r2.
  vsub    $v06, $v18, $v16.v
  vsub    $v05, $v17, $v15.v
  vsub    $v02, $v00, $v16.v
  vsub    $v01, $v00, $v15.v
  vmulf   $v14, $v06, $v26.v
  vmacf   $v14, $v05, $v25.v
  vmulf   $v13, $v05, $v26.v
  vmacf   $v13, $v06, $v04.v
  vsub    $v18, $v18, $v02.v
  vsub    $v17, $v17, $v01.v
  vadd    $v14, $v14, $v14.v
  vadd    $v13, $v13, $v13.v

  # Second DIF level for the differences, rotated by r2.
  vsub    $v06, $v10, $v08.v
  vsub    $v05, $v09, $v07.v
  vsub    $v02, $v00, $v08.v
  vsub    $v01, $v00, $v07.v
  vmulf   $v12, $v06, $v26.v
  vmacf   $v12, $v05, $v25.v
  vmulf   $v11, $v05, $v26.v
  vmacf   $v11, $v06, $v04.v
  vsub    $v10, $v10, $v02.v
  vsub    $v09, $v09, $v01.v
  vadd    $v12, $v12, $v12.v
  vadd    $v11, $v11, $v11.v

  # Pack and store the four fused-stage outputs.
  vmrg    $v29, $v18, $v17
  vmrg    $v16, $v14, $v13

  # Advance the four-lane twiddle by r4.
  vor     $v18, $v00, $v28
  vor     $v17, $v00, $v27
  vmrg    $v15, $v10, $v09
  sqv     $v29, 0, 0, even_ptr
  addiu   even_ptr, even_ptr, 16
  vmrg    $v14, $v12, $v11
  sqv     $v16, 0, 0, radix4_ptr1
  vmulf   $v28, $v18, $v21.v
  addiu   radix4_ptr1, radix4_ptr1, 16
  vmacf   $v28, $v17, $v19.v
  sqv     $v15, 0, 0, radix4_ptr2
  vmulf   $v27, $v18, $v20.v
  addiu   radix4_ptr2, radix4_ptr2, 16
  vmacf   $v27, $v17, $v21.v
  sqv     $v14, 0, 0, radix4_ptr3
  addiu   radix4_ptr3, radix4_ptr3, 16
  vadd    $v28, $v28, $v28.v
  bne     butterfly, fft_quarter, .fft_radix4_butterfly
  vadd    $v27, $v27, $v27.v

  addu    fft_block, fft_block, block_stride
  bne     fft_block, stage_end, .fft_radix4_block
  vor     $v28, $v00, $v23

  addiu   rotation_size, $0, 4
  srl     fft_length, fft_length, 2
  bne     fft_length, rotation_size, .fft_stage_setup
  addiu   rotation_cursor, rotation_cursor, -8

.fft_stages_done:
  # Fuse the final length-4 and length-2 passes into one radix-4 DIF stage.
  ori     even_ptr, $0, %lo(WORK)
  ori     stage_end, $0, %lo(WORK)
  sll     odd_ptr, complex_count, 2
  addu    stage_end, stage_end, odd_ptr

.fft_radix4_final:
  lsv     $v23.e0, 0, even_ptr
  lsv     $v22.e0, 2, even_ptr
  lsv     $v21.e0, 4, even_ptr
  lsv     $v20.e0, 6, even_ptr
  lsv     $v19.e0, 8, even_ptr
  lsv     $v18.e0, 10, even_ptr
  lsv     $v17.e0, 12, even_ptr
  lsv     $v16.e0, 14, even_ptr

  # First DIF level: (x0,x2) and (x1,x3).
  vsub    $v15, $v00, $v19.v
  vsub    $v14, $v00, $v18.v
  vsub    $v11, $v00, $v17.v
  vsub    $v10, $v00, $v16.v
  vsub    $v13, $v23, $v19.v
  vsub    $v12, $v22, $v18.v
  vsub    $v09, $v21, $v17.v
  vsub    $v08, $v20, $v16.v
  vsub    $v15, $v23, $v15.v
  vsub    $v14, $v22, $v14.v
  vsub    $v11, $v21, $v11.v
  vsub    $v10, $v20, $v10.v
  vsub    $v03, $v00, $v09.v       # -i * (v09 + i*v08) = v08 - i*v09

  # Second DIF level for the two unrotated sums.
  vsub    $v07, $v00, $v11.v
  vsub    $v06, $v00, $v10.v
  vsub    $v05, $v15, $v11.v
  vsub    $v04, $v14, $v10.v
  vsub    $v07, $v15, $v07.v
  vsub    $v06, $v14, $v06.v
  ssv     $v05, 0, 4, even_ptr
  ssv     $v04, 0, 6, even_ptr
  ssv     $v07, 0, 0, even_ptr

  # Second DIF level for the two rotated differences.
  vsub    $v07, $v00, $v08.v
  ssv     $v06, 0, 2, even_ptr
  vsub    $v06, $v00, $v03.v
  vsub    $v05, $v13, $v08.v
  vsub    $v04, $v12, $v03.v
  vsub    $v07, $v13, $v07.v
  vsub    $v06, $v12, $v06.v
  ssv     $v05, 0, 12, even_ptr
  ssv     $v04, 0, 14, even_ptr
  ssv     $v07, 0, 8, even_ptr
  ssv     $v06, 0, 10, even_ptr

  addiu   even_ptr, even_ptr, 16
  bne     even_ptr, stage_end, .fft_radix4_final
  nop

  jal     dif_reorder
  nop

#undef complex_count
#undef fft_length
#undef fft_half
#undef rotation_size
#undef rotation_cursor
#undef radix2_pending
#undef fft_quarter
#undef fft_block
#undef stage_end
#undef odd_offset
#undef block_stride
#undef even_ptr
#undef odd_ptr
#undef butterfly
#undef radix4_ptr1
#undef radix4_ptr2
#undef radix4_ptr3

  jal     load_rotation_step
  or      v0, zero, s5
  vor     $v27, $v00, $v01
  jal     load_rotation_4
  vor     $v28, $v00, $v02

  ori     s2, zero, %lo(WORK)
  jal     post_twiddle
  vsub    $v22, $v00, $v23.v

.imdct_transform_done:
  # Shifting LAP into the sign bit allows another direct signed branch.
  # Without lapping, return the transformed coefficients to coeff_rdram.
  sll     t3, command_flags, 1
  bgez    t3, .raw_output
  lui     t3, 0x07ff

  # Decode lapping flags and fetch or clear the persistent 512-sample lap state.
  and     s5, command_flags, t3
  srl     s2, command_flags, FLAGS_SHIFT
  andi    s1, s2, (FLAG_SCALE_SAMPLES >> FLAGS_SHIFT)
  srl     s3, s2, 4
  xori    s3, s3, 1

  andi    t4, s2, (FLAG_CLEAR_LAP >> FLAGS_SHIFT)
  bnez    t4, .clear_lap
  srl     s5, s5, 16

  addiu   t0, zero, 1023
  ori     s4, zero, %lo(LAP)
  addiu   t2, zero, 12
  jal     DMAExec
  or      s0, zero, lap_rdram
  b       .lap_ready
  nop

.clear_lap:
  ori     t4, zero, %lo(LAP)
  addiu   t5, t4, 1024
  vxor    $v31, $v31, $v31
.clear_lap_next:
  sqv     $v31, 0, 0, t4
  addiu   t4, t4, 16
  bne     t4, t5, .clear_lap_next
  nop

.lap_ready:

  # Save the lower transform half as the next lap state.
  addiu   t0, transform_size, -1
  ori     s4, zero, %lo(WORK)
  addiu   t2, zero, -32756
  jal     DMAExec
  or      s0, zero, lap_rdram

  ori     fp, zero, %lo(LAP)
  addu    fp, fp, transform_size
  addiu   fp, fp, -2
  ori     sp, zero, %lo(WORK)
  addu    sp, sp, transform_size
  ori     k1, zero, %lo(WORK)
  subu    k0, transform_size, s5
  srl     k0, k0, 1
  jal     lap_prefix
  subu    s0, s1, s3

  beqz    s5, .lap_complete
  srl     t4, s5, 1
  jal     load_window_8
  vxor    $v24, $v00, $v00
  jal     lap_windowed
  vsub    $v24, $v24, $v25.v

.lap_complete:
  beqz    s3, .scaled_output
  nop

  # Reassemble and merge a short subblock into the 1024-sample state.
  jal     copy_lap_high
  ori     fp, zero, %lo(LAP)

  addiu   t0, transform_size, -1
  ori     s4, zero, %lo(LAP)
  addiu   t2, zero, 12
  jal     DMAExec
  or      s0, zero, lap_rdram

  jal     reshuffle_lap
  addiu   t0, zero, 1024

  addiu   t0, t0, -1
  ori     s4, zero, %lo(LAP)
  addiu   t2, zero, -32756
  jal     DMAExec
  or      s0, zero, lap_rdram

  andi    t4, s2, (FLAG_DISCARD_OUTPUT >> FLAGS_SHIFT)
  bnez    t4, RSPQ_Loop
  nop

  sll     fp, transform_size, 1
  addiu   t0, fp, -1
  ori     s4, zero, %lo(OUTPUT)
  addiu   t2, zero, -32756
  jal     DMAExec
  or      s0, zero, output_rdram
  j       RSPQ_Loop
  nop

.scaled_output:
  andi    t4, s2, (FLAG_DISCARD_OUTPUT >> FLAGS_SHIFT)
  bnez    t4, RSPQ_Loop
  nop

  addiu   t0, transform_size, -1
  ori     s4, zero, %lo(WORK)
  addiu   t2, zero, -32756
  jal     DMAExec
  or      s0, zero, output_rdram

  addu    fp, output_rdram, transform_size
  addiu   t0, transform_size, -1
  ori     s4, zero, %lo(LAP)
  addiu   t2, zero, -32756
  jal     DMAExec
  or      s0, zero, fp
  j       RSPQ_Loop
  nop

.raw_output:
  sll     t0, transform_size, 1
  addiu   t0, t0, -1
  ori     s4, zero, %lo(WORK)
  addiu   t2, zero, -32756
  jal     DMAExec
  or      s0, zero, coeff_rdram
  j       RSPQ_Loop
  nop

#undef coeff_rdram
#undef lap_rdram
#undef command_flags
#undef output_rdram
#undef transform_size
#undef transform_bytes
#undef FLAG_TRANSFORM
#undef FLAG_LAP
#undef FLAG_CLEAR_LAP
#undef FLAG_DISCARD_OUTPUT
#undef FLAG_SCALE_SAMPLES
#undef FLAGS_SHIFT


# ------------------------------------------------------------------------------
# Transform reordering
# ------------------------------------------------------------------------------

# Convert the bit-reversed DIF output to natural order without an in-place
# bit-reversal pass. LAP temporarily holds the lower natural-order half.
# Inputs: s5 = complex count, s7 = transform size.
# Clobbers: s0, fp, sp, k1, k0, t9..t4.
#define reorder_half_bytes  $16
#define reorder_rev         $30
#define reorder_bit         $29
#define reorder_dst         $27
#define reorder_groups      $26
#define reorder_index       $25
#define reorder_src0        $24
#define reorder_src2        $15
#define reorder_src1        $14
#define reorder_src3        $13
#define reorder_value       $12

dif_reorder:
  srl     reorder_half_bytes, $23, 1
  srl     reorder_groups, $21, 3
  or      reorder_index, $0, $0
  or      reorder_rev, $0, $0
  addiu   reorder_dst, $0, %lo(LAP)
  sll     reorder_src0, reorder_rev, 3

# Natural indices 0..N/2-1 live at even DIF positions. Four consecutive
# natural indices select quadrants in the order 0, 2, 1, 3.
.dif_reorder_low:
  addiu   reorder_src0, reorder_src0, %lo(WORK)
  addu    reorder_src2, reorder_src0, $23
  addu    reorder_src1, reorder_src0, reorder_half_bytes
  addu    reorder_src3, reorder_src2, reorder_half_bytes
  llv     $v29.e0, 0, reorder_src0
  llv     $v29.e2, 0, reorder_src2
  llv     $v29.e4, 0, reorder_src1
  llv     $v29.e6, 0, reorder_src3
  addiu   reorder_index, reorder_index, 1
  srl     reorder_bit, reorder_groups, 1
  beq     reorder_index, reorder_groups, .dif_compact_odd_setup
  sqv     $v29, 0, 0, reorder_dst

  addiu   reorder_dst, reorder_dst, 16
.dif_reorder_low_carry:
  xor     reorder_rev, reorder_rev, reorder_bit
  and     reorder_value, reorder_rev, reorder_bit
  beqz    reorder_value, .dif_reorder_low_carry
  srl     reorder_bit, reorder_bit, 1
  b       .dif_reorder_low
  sll     reorder_src0, reorder_rev, 3

# Preserve all odd DIF positions compactly before WORK is overwritten.
.dif_compact_odd_setup:
  addiu   reorder_src0, $0, %lo(WORK) + 4
  addiu   reorder_dst, $0, %lo(WORK)
  srl     reorder_index, $21, 1
.dif_compact_odd:
  llv     $v29.e0, 0, reorder_src0
  llv     $v29.e2, 8, reorder_src0
  llv     $v29.e4, 16, reorder_src0
  llv     $v29.e6, 24, reorder_src0
  addiu   reorder_src0, reorder_src0, 32
  addiu   reorder_index, reorder_index, -4
  addiu   reorder_dst, reorder_dst, 16
  bnez    reorder_index, .dif_compact_odd
  sqv     $v29, 0, -16, reorder_dst

# Natural indices N/2..N-1 now come from the compact odd-position array.
  or      reorder_index, $0, $0
  or      reorder_rev, $0, $0
  addiu   reorder_dst, $23, %lo(WORK)
  sll     reorder_src0, reorder_rev, 2
.dif_reorder_high:
  addiu   reorder_src0, reorder_src0, %lo(WORK)
  addu    reorder_src2, reorder_src0, reorder_half_bytes
  srl     reorder_bit, reorder_half_bytes, 1
  addu    reorder_src1, reorder_src0, reorder_bit
  addu    reorder_src3, reorder_src2, reorder_bit
  llv     $v29.e0, 0, reorder_src0
  llv     $v29.e2, 0, reorder_src2
  llv     $v29.e4, 0, reorder_src1
  llv     $v29.e6, 0, reorder_src3
  addiu   reorder_index, reorder_index, 1
  srl     reorder_bit, reorder_groups, 1
  beq     reorder_index, reorder_groups, .dif_restore_low_setup
  sqv     $v29, 0, 0, reorder_dst

  addiu   reorder_dst, reorder_dst, 16
.dif_reorder_high_carry:
  xor     reorder_rev, reorder_rev, reorder_bit
  and     reorder_value, reorder_rev, reorder_bit
  beqz    reorder_value, .dif_reorder_high_carry
  srl     reorder_bit, reorder_bit, 1
  b       .dif_reorder_high
  sll     reorder_src0, reorder_rev, 2

# Restore the lower natural-order half from LAP with vector copies.
.dif_restore_low_setup:
  addiu   reorder_src0, $0, %lo(LAP)
  addiu   reorder_dst, $0, %lo(WORK)
  addu    reorder_src1, reorder_dst, $23
.dif_restore_low:
  lqv     $v29, 0, 0, reorder_src0
  addiu   reorder_dst, reorder_dst, 16
  addiu   reorder_src0, reorder_src0, 16
  bne     reorder_dst, reorder_src1, .dif_restore_low
  sqv     $v29, 0, -16, reorder_dst

  jr      $31
  nop

#undef reorder_half_bytes
#undef reorder_rev
#undef reorder_bit
#undef reorder_dst
#undef reorder_groups
#undef reorder_index
#undef reorder_src0
#undef reorder_src2
#undef reorder_src1
#undef reorder_src3
#undef reorder_value


# Swap WORK's odd-indexed samples with the same sequence in reverse order.
# This packs/unpacks the real/imaginary FFT representation.
# Input: s7 = transform size.
# Clobbers: t8, t7, t6, v29..v26.
# Processes four odd-lane pairs per iteration; supported sizes are powers of
# two greater than or equal to 32.
#define swap_left       $24
#define swap_right      $15
#define swaps_left      $14

swap_odd_reverse:
  addiu   swap_left, $0, %lo(WORK)
  sll     swap_right, $23, 1
  addiu   swap_right, swap_right, %lo(WORK) - 16
  srl     swaps_left, $23, 2

.swap_odd_reverse_loop:
  lqv     $v29, 0, 0, swap_left
  lqv     $v28, 0, 0, swap_right
  vor     $v27, $v00, $v29
  vor     $v26, $v00, $v28
  vmov    $v27.e1, $v28.e7
  vmov    $v27.e3, $v28.e5
  vmov    $v27.e5, $v28.e3
  vmov    $v27.e7, $v28.e1
  vmov    $v26.e1, $v29.e7
  vmov    $v26.e3, $v29.e5
  vmov    $v26.e5, $v29.e3
  vmov    $v26.e7, $v29.e1
  sqv     $v27, 0, 0, swap_left
  addiu   swaps_left, swaps_left, -4
  addiu   swap_left, swap_left, 16
  sqv     $v26, 0, 0, swap_right
  bnez    swaps_left, .swap_odd_reverse_loop
  addiu   swap_right, swap_right, -16

  jr      $31
  nop

#undef swap_left
#undef swap_right
#undef swaps_left


# ------------------------------------------------------------------------------
# Overlap-add helpers
# ------------------------------------------------------------------------------

# Load the first eight overlap-window coefficients and the eight-sample
# rotation step. Nonzero overlaps are powers of two in the range 32..1024.
# Input: s5 = overlap.
# Outputs: v28 = c, v27 = s, v26 = jump c, v25 = jump s.
# Clobbers: t9, t8, t7, t6, t5.

load_window_8:
  addiu   $25, $0, %lo(WINDOW_C)       # t9: c address
  addiu   $24, $0, %lo(WINDOW_S)       # t8: s address
  addiu   $15, $0, %lo(WINDOW_JUMP)    # t7: jump address
  or      $14, $21, $0                 # t6: remaining
  addiu   $13, $0, 8

# overlap >= 32, so at least one advance is always required.
.load_window_next:
  srl     $14, $14, 1
  addiu   $25, $25, 16
  addiu   $24, $24, 16
  bne     $14, $13, .load_window_next
  addiu   $15, $15, 4

  lqv     $v28, 0, 0, $25
  lqv     $v27, 0, 0, $24
  lsv     $v26.e0, 0, $15
  lsv     $v25.e0, 2, $15
  vor     $v26, $v00, $v26.e0
  jr      $31
  vor     $v25, $v00, $v25.e0


# Merge one decoded short block into the persistent 1024-sample lap layout.
# Exactly size samples are staged in OUTPUT. LAP receives the remaining old
# state followed by the unused decoded samples, both stored backwards.
# Inputs: s7 = size, s1 = mono output shift (0 or 1).
# Clobbers: k0, t9, t8, t7, t6, t5, fp, t4.

#define available       $26
#define lap_dst         $25
#define lap_src         $24
#define decoded_src     $15
#define output_dst      $14
#define shared_count    $13
#define samples_left    $30
#define sample          $12

reshuffle_lap:
  addiu   available, $0, 1024              # k0: available
  subu    available, available, $23
  or      shared_count, $23, $0            # t5: shared = size
  srl     available, available, 1
  sltu    sample, available, shared_count
  addiu   decoded_src, $0, %lo(WORK)       # t7: decoded source
  addiu   lap_dst, $0, %lo(LAP) + 1024     # t9: lap destination
  addiu   output_dst, $0, %lo(OUTPUT)      # t6: output destination
  beqz    sample, .reshuffle_shared_ready
  or      lap_src, lap_dst, $0             # t8: lap source
  or      shared_count, available, $0      # shared = available

.reshuffle_shared_ready:
  or      samples_left, shared_count, $0   # fp: remaining = shared

# Emit the shared tail of the old lap state in reverse order.
.reshuffle_output_lap:
  lh      sample, -2(lap_src)
  addiu   samples_left, samples_left, -1
  addiu   lap_src, lap_src, -2
  sllv    sample, sample, $17
  sh      sample, 0(output_dst)
  bnez    samples_left, .reshuffle_output_lap
  addiu   output_dst, output_dst, 2

# If size exceeds available, emit the remainder from the decoded block.
  subu    samples_left, $23, shared_count
  beqz    samples_left, .reshuffle_lap_old_setup
  nop
.reshuffle_output_decoded:
  lh      sample, 0(decoded_src)
  addiu   decoded_src, decoded_src, 2
  addiu   samples_left, samples_left, -1
  sllv    sample, sample, $17
  sh      sample, 0(output_dst)
  bnez    samples_left, .reshuffle_output_decoded
  addiu   output_dst, output_dst, 2

# Preserve any old lap samples which were not emitted.
.reshuffle_lap_old_setup:
  subu    samples_left, available, shared_count
  beqz    samples_left, .reshuffle_lap_decoded_setup
  nop
.reshuffle_lap_old:
  addiu   lap_dst, lap_dst, -2
  addiu   samples_left, samples_left, -1
  lh      sample, -2(lap_src)
  addiu   lap_src, lap_src, -2
  bnez    samples_left, .reshuffle_lap_old
  sh      sample, 0(lap_dst)

# Fill the rest of the new lap state from the decoded block.
.reshuffle_lap_decoded_setup:
  or      samples_left, shared_count, $0
.reshuffle_lap_decoded:
  lh      sample, 0(decoded_src)
  addiu   lap_dst, lap_dst, -2
  addiu   decoded_src, decoded_src, 2
  addiu   samples_left, samples_left, -1
  bnez    samples_left, .reshuffle_lap_decoded
  sh      sample, 0(lap_dst)

  jr      $31
  nop

#undef available
#undef lap_dst
#undef lap_src
#undef decoded_src
#undef output_dst
#undef shared_count
#undef samples_left
#undef sample



# Emit the unwindowed prefix of an IMDCT block and replace the consumed lap
# samples with the reversed high transform half.
# Inputs/outputs: fp = lap pointer, sp = transform pointer,
#                 k1 = output pointer, k0 = sample count.
# Input: s0 = 1 to scale mono output; any other value leaves it unscaled.
# Clobbers: fp, sp, k1, k0, t9, t8, t7, v28, v27.

#define lap_ptr         $30
#define transform_ptr   $29
#define output_ptr      $27
#define samples_left    $26
#define load_a          $25
#define one             $24
#define load_b          $15

lap_prefix:
  beqz    samples_left, .lap_prefix_done
  addiu   one, $0, 1                 # t8: direct-mono comparison

.lap_prefix_next:
  lh      load_a, 0(lap_ptr)
  lh      load_b, -2(lap_ptr)
  vxor    $v28, $v00, $v00
  mtc2    load_b, $v28.e1
  mtc2    load_a, $v28.e0
  lh      load_a, -4(lap_ptr)
  lh      load_b, -6(lap_ptr)
  mtc2    load_b, $v28.e3
  mtc2    load_a, $v28.e2
  lh      load_a, -8(lap_ptr)
  lh      load_b, -10(lap_ptr)
  mtc2    load_b, $v28.e5
  mtc2    load_a, $v28.e4
  lh      load_a, -12(lap_ptr)
  lh      load_b, -14(lap_ptr)
  mtc2    load_a, $v28.e6
  mtc2    load_b, $v28.e7
  lqv     $v27, 0, 0, transform_ptr
  bne     $16, one, .lap_prefix_store
  nop
  vaddc   $v28, $v28, $v28.v
  vaddc   $v27, $v27, $v27.v

.lap_prefix_store:
  sqv     $v28, 0, 0, output_ptr
  ssv     $v27, 0, 0, lap_ptr
  ssv     $v27, 2, -2, lap_ptr
  ssv     $v27, 4, -4, lap_ptr
  ssv     $v27, 6, -6, lap_ptr
  ssv     $v27, 8, -8, lap_ptr
  ssv     $v27, 10, -10, lap_ptr
  ssv     $v27, 12, -12, lap_ptr
  ssv     $v27, 14, -14, lap_ptr
  addiu   transform_ptr, transform_ptr, 16
  addiu   output_ptr, output_ptr, 16
  addiu   samples_left, samples_left, -8
  bnez    samples_left, .lap_prefix_next
  addiu   lap_ptr, lap_ptr, -16

.lap_prefix_done:
  jr      $31
  nop

#undef lap_ptr
#undef transform_ptr
#undef output_ptr
#undef samples_left
#undef load_a
#undef one
#undef load_b



# Apply the windowed overlap region, producing low output in WORK and the
# reversed high output in LAP while advancing the window rotation.
# Inputs/outputs: fp = lap pointer, sp = transform pointer,
#                 k1 = output pointer.
# Inputs: t4 = sample count; s0 = 1 to scale mono output;
#         v28/v27 = c/s, v26/v25 = jump c/s, v24 = -jump s.
# Clobbers: fp, sp, k1, t4, t9, t8, t7, v23..v17.

#define lap_ptr         $30
#define transform_ptr   $29
#define output_ptr      $27
#define samples_left    $12
#define load_a          $25
#define one             $24
#define load_b          $15

lap_windowed:
  addiu   one, $0, 1

.lap_windowed_next:
  lh      load_a, 0(lap_ptr)
  lh      load_b, -2(lap_ptr)
  vxor    $v23, $v00, $v00
  mtc2    load_b, $v23.e1
  mtc2    load_a, $v23.e0
  lh      load_a, -4(lap_ptr)
  lh      load_b, -6(lap_ptr)
  mtc2    load_b, $v23.e3
  mtc2    load_a, $v23.e2
  lh      load_a, -8(lap_ptr)
  lh      load_b, -10(lap_ptr)
  mtc2    load_b, $v23.e5
  mtc2    load_a, $v23.e4
  lh      load_a, -12(lap_ptr)
  lh      load_b, -14(lap_ptr)
  mtc2    load_a, $v23.e6
  mtc2    load_b, $v23.e7
  lqv     $v22, 0, 0, transform_ptr

  vsub    $v21, $v00, $v27.v
  vmulf   $v20, $v23, $v28.v
  vmacf   $v20, $v22, $v21.v
  vmulf   $v19, $v23, $v27.v
  vmacf   $v19, $v22, $v28.v
  vadd    $v20, $v20, $v20.v
  bne     $16, one, .lap_windowed_store
  vadd    $v19, $v19, $v19.v
  vaddc   $v20, $v20, $v20.v
  vaddc   $v19, $v19, $v19.v

.lap_windowed_store:

  vor     $v18, $v00, $v28
  sqv     $v20, 0, 0, output_ptr
  vor     $v17, $v00, $v27
  ssv     $v19, 0, 0, lap_ptr
  ssv     $v19, 2, -2, lap_ptr
  addiu   samples_left, samples_left, -8
  vmulf   $v28, $v18, $v26.v
  addiu   output_ptr, output_ptr, 16
  vmacf   $v28, $v17, $v24.v
  ssv     $v19, 4, -4, lap_ptr
  addiu   transform_ptr, transform_ptr, 16
  vmulf   $v27, $v18, $v25.v
  ssv     $v19, 6, -6, lap_ptr
  ssv     $v19, 8, -8, lap_ptr
  vmacf   $v27, $v17, $v26.v
  vadd    $v28, $v28, $v28.v
  ssv     $v19, 10, -10, lap_ptr
  ssv     $v19, 12, -12, lap_ptr
  ssv     $v19, 14, -14, lap_ptr
  vadd    $v27, $v27, $v27.v
  bnez    samples_left, .lap_windowed_next

  addiu   lap_ptr, lap_ptr, -16

  jr      $31
  nop

#undef lap_ptr
#undef transform_ptr
#undef output_ptr
#undef samples_left
#undef load_a
#undef one
#undef load_b


# ------------------------------------------------------------------------------
# Rotation and twiddle helpers
# ------------------------------------------------------------------------------

# Load and broadcast the {wc,ws} rotation step for a power-of-two size.
# The table address walk and scalar-to-vector transfers are scheduled as one
# leaf operation instead of separate RSPL helpers.
# Input: $2 = size.
# Outputs: v2 = wc, v1 = ws.
# Clobbers: $2, $3, v2, v1.

load_rotation_step:
  addiu   $3, $0, %lo(ROTATIONS) - 8

.rotation_step_address:
  srl     $2, $2, 1
  bnez    $2, .rotation_step_address
  addiu   $3, $3, 8

  lsv     $v02.e0, 4, $3
  lsv     $v01.e0, 6, $3
  vor     $v02, $v00, $v02.e0
  jr      $31
  vor     $v01, $v00, $v01.e0


# Build four consecutive rotations in paired lanes and a four-step jump.
# Inputs: s3 = initial size, v28/v27 = step c/s.
# Outputs: v26/v25 = c/s, v24/v23 = jump c/s.
# Clobbers: t3, t2, v22..v18.

load_rotation_4:
  or      $11, $19, $0
  addiu   $10, $0, %lo(ROTATIONS) - 8
.rotation4_address:
  srl     $11, $11, 1
  bnez    $11, .rotation4_address
  addiu   $10, $10, 8
  lh      $11, 0($10)
  lh      $10, 2($10)
  vxor    $v21, $v00, $v00
  vxor    $v25, $v00, $v00
  vsub    $v20, $v00, $v27.v

  vxor    $v22, $v00, $v00
  mtc2    $10, $v21.e0
  mtc2    $11, $v22.e0
  vxor    $v26, $v00, $v00
  vor     $v21, $v00, $v21.e0
  vor     $v22, $v00, $v22.e0
  vmov    $v25.e0, $v21.e0
  vmov    $v26.e1, $v22.e0

  vor     $v19, $v00, $v22
  vor     $v18, $v00, $v21
  vmov    $v26.e0, $v22.e0
  vmov    $v25.e1, $v21.e0
  vmulf   $v22, $v19, $v28.v
  vmacf   $v22, $v18, $v20.v
  vmulf   $v21, $v19, $v27.v
  vmacf   $v21, $v18, $v28.v
  vadd    $v22, $v22, $v22.v
  vadd    $v21, $v21, $v21.v
  vmov    $v26.e2, $v22.e0
  vmov    $v26.e3, $v22.e0

  vor     $v19, $v00, $v22
  vor     $v18, $v00, $v21
  vmov    $v25.e2, $v21.e0
  vmov    $v25.e3, $v21.e0
  vmulf   $v22, $v19, $v28.v
  vmacf   $v22, $v18, $v20.v
  vmulf   $v21, $v19, $v27.v
  vmacf   $v21, $v18, $v28.v
  vadd    $v22, $v22, $v22.v
  vadd    $v21, $v21, $v21.v
  vmov    $v26.e4, $v22.e0

  vor     $v19, $v00, $v22
  vmov    $v25.e4, $v21.e0
  vor     $v18, $v00, $v21
  vmov    $v26.e5, $v22.e0
  vmov    $v25.e5, $v21.e0
  vmulf   $v22, $v19, $v28.v
  vmacf   $v22, $v18, $v20.v
  vmulf   $v21, $v19, $v27.v
  vmacf   $v21, $v18, $v28.v
  vadd    $v22, $v22, $v22.v

  vxor    $v24, $v00, $v31.e1
  vadd    $v21, $v21, $v21.v
  vxor    $v23, $v00, $v00
  vmov    $v26.e7, $v22.e0
  vmov    $v26.e6, $v22.e0

# Advance the jump rotation four times.
  vor     $v19, $v00, $v24
  vor     $v18, $v00, $v23
  vmov    $v25.e6, $v21.e0
  vmov    $v25.e7, $v21.e0
  vmulf   $v24, $v19, $v28.v
  vmacf   $v24, $v18, $v20.v
  vmulf   $v23, $v19, $v27.v
  vmacf   $v23, $v18, $v28.v
  vadd    $v24, $v24, $v24.v
  vadd    $v23, $v23, $v23.v

  vor     $v19, $v00, $v24
  vor     $v18, $v00, $v23
  vmulf   $v24, $v19, $v28.v
  vmacf   $v24, $v18, $v20.v
  vmulf   $v23, $v19, $v27.v
  vmacf   $v23, $v18, $v28.v
  vadd    $v24, $v24, $v24.v
  vadd    $v23, $v23, $v23.v

  vor     $v19, $v00, $v24
  vor     $v18, $v00, $v23
  vmulf   $v24, $v19, $v28.v
  vmacf   $v24, $v18, $v20.v
  vmulf   $v23, $v19, $v27.v
  vmacf   $v23, $v18, $v28.v
  vadd    $v24, $v24, $v24.v
  vadd    $v23, $v23, $v23.v

  vor     $v19, $v00, $v24
  vor     $v18, $v00, $v23
  vmulf   $v24, $v19, $v28.v
  vmacf   $v24, $v18, $v20.v
  vmulf   $v23, $v19, $v27.v
  vmacf   $v23, $v18, $v28.v
  vadd    $v24, $v24, $v24.v
  jr      $31
  vadd    $v23, $v23, $v23.v


# Fixed register layout shared by the pre- and post-twiddle passes:
# s2/s1 = address/end; v26/v25 = c/s; v24/v23 = jump c/s;
# v22 = -jump s.
# Both passes clobber s2 and v21..v16. post_twiddle tail-calls
# swap_odd_reverse and therefore also clobbers t8, t7, t6, v29..v26.


pre_twiddle:
.pre_twiddle_next:
  vsub    $v16, $v00, $v25.v
  lqv     $v21, 0, 0, $18
  vor     $v20, $v00, $v25
  vmulf   $v18, $v26, $v21.q0
  vmacf   $v18, $v25, $v21.q1
  vmulf   $v17, $v26, $v21.q1
  vmacf   $v17, $v16, $v21.q0
  vor     $v21, $v00, $v26
  vadd    $v18, $v18, $v18.v
  vadd    $v17, $v17, $v17.v
  vmulf   $v26, $v21, $v24.v
  vmacf   $v26, $v20, $v22.v
  vmulf   $v25, $v21, $v23.v
  vmacf   $v25, $v20, $v24.v
  vmrg    $v21, $v18, $v17
  vadd    $v26, $v26, $v26.v
  addiu   $18, $18, 16
  vadd    $v25, $v25, $v25.v
  bne     $18, $17, .pre_twiddle_next
  sqv     $v21, 0, -16, $18

  jr      $31
  nop

post_twiddle:
.post_twiddle_next:
  vsub    $v16, $v00, $v25.v
  lqv     $v21, 0, 0, $18
  vor     $v20, $v00, $v25
  vmulf   $v18, $v26, $v21.q0
  vmacf   $v18, $v25, $v21.q1
  vmulf   $v17, $v26, $v21.q1
  vmacf   $v17, $v16, $v21.q0
  vor     $v21, $v00, $v26
  vadd    $v18, $v18, $v18.v
  vadd    $v17, $v17, $v17.v
  vmulf   $v26, $v21, $v24.v
  vmacf   $v26, $v20, $v22.v
  vsub    $v17, $v00, $v17.v
  vmulf   $v25, $v21, $v23.v
  vmacf   $v25, $v20, $v24.v
  vadd    $v26, $v26, $v26.v
  vmrg    $v21, $v18, $v17
  addiu   $18, $18, 16
  vadd    $v25, $v25, $v25.v
  bne     $18, $17, .post_twiddle_next
  sqv     $v21, 0, -16, $18

  # This pass is always followed by reverse unpacking. Preserve ra and
  # tail-call the helper directly.
  j       swap_odd_reverse
  nop


# ------------------------------------------------------------------------------
# Final overlap assembly helper
# ------------------------------------------------------------------------------

# Append the high output half staged in LAP to the low half in WORK.
# fp = source, k1 = destination, sp = destination end.
# Clobbers: fp, k1, v28.
# Every supported subblock contributes at least one 16-byte vector.

copy_lap_high:
.copy_lap_high_next:
  lqv     $v28, 0, 0, $30
  addiu   $27, $27, 16
  addiu   $30, $30, 16
  bne     $27, $29, .copy_lap_high_next
  sqv     $v28, 0, -16, $27

  jr      $31
  nop


# ------------------------------------------------------------------------------
# Stereo interleave command
# Interleaves the stereo samples from mid/side planar blocks and performs
# output scalind.
# ------------------------------------------------------------------------------

#define mid_rdram         a0
#define side_rdram        a1
#define stereo_rdram      a2

stereo_interleave:
  # Reconstruct and interleave four independent 256-sample chunks.
  or      s5, zero, stereo_rdram
  or      s6, zero, side_rdram
  or      s7, zero, mid_rdram
  addiu   s3, zero, 4
  addiu   t3, zero, 0x55
  ctc2    t3, COP2_CTRL_VCC

.stereo_chunk:
  # Load 256 side and mid samples into separate DMEM buffers.
  addiu   t0, zero, 511
  ori     s4, zero, %lo(WORK)
  addiu   t2, zero, 12
  jal     DMAExec
  or      s0, zero, s6
  addiu   t0, zero, 511
  ori     s4, zero, %lo(OUTPUT)
  addiu   t2, zero, 12
  jal     DMAExec
  or      s0, zero, s7

  # LAP is independent output staging, so neither input can be overwritten.
  addiu   t8, zero, %lo(OUTPUT)
  # Each iteration stores the previous result while computing the current one.
  # The first dummy store lands in OUTPUT's unused tail; the 33rd iteration
  # stores the final real vector while computing one harmless dummy result.
  addiu   t7, zero, %lo(LAP) - 32
  addiu   t6, zero, 33
.stereo_vectors:
  lqv     $v28, 0, 0, t8
  lqv     $v27, 0, -1024, t8
  vadd    $v26, $v28, $v27.v
  vsub    $v25, $v28, $v27.v
  slv     $v24, 0, 0, t7
  slv     $v23, 0, 4, t7
  slv     $v24, 4, 8, t7
  vadd    $v26, $v26, $v26.v
  slv     $v23, 4, 12, t7
  vadd    $v25, $v25, $v25.v
  slv     $v24, 8, 16, t7
  slv     $v23, 8, 20, t7
  slv     $v24, 12, 24, t7
  slv     $v23, 12, 28, t7
  vor     $v23, $v00, $v26.q1
  addiu   t6, t6, -1
  vor     $v24, $v00, $v26.q0
  addiu   t7, t7, 32
  addiu   t8, t8, 16
  vmrg    $v23, $v23, $v25.q1
  bnez    t6, .stereo_vectors
  vmrg    $v24, $v24, $v25.q0

.stereo_vectors_end:
  # Commit this interleaved chunk, then advance all RDRAM cursors.
  addiu   t0, zero, 1023
  ori     s4, zero, %lo(LAP)
  addiu   t2, zero, -32756
  jal     DMAExec
  or      s0, zero, s5
  addiu   s5, s5, 1024
  addiu   s6, s6, 512
  addiu   s3, s3, -1
  bnez    s3, .stereo_chunk
  addiu   s7, s7, 512
  j       RSPQ_Loop
  nop

#undef mid_rdram
#undef side_rdram
#undef stereo_rdram