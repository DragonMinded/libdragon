matrix_mult_mtx:
  # Inputs
  #define mtx_mult_a $14
  #define mtx_mult_b $15
  #define mtx_mult_out $16
  # Clobbers
  #define a0i $v01
  #define a0f $v02
  #define a1i $v03
  #define a1f $v04
  #define a2i $v05
  #define a2f $v06
  #define a3i $v07
  #define a3f $v08
  #define b01i $v09
  #define b01f $v10
  #define b23i $v11
  #define b23f $v12
  #define ab01i $v13
  #define ab01f $v14
  #define ab23i $v15
  #define ab23f $v16
  #define vtemp $v17

    # Load matrix a
    ldv a0i, 0x0, 0x00, mtx_mult_a
    ldv a0i, 0x8, 0x00, mtx_mult_a
    ldv a1i, 0x0, 0x08, mtx_mult_a
    ldv a1i, 0x8, 0x08, mtx_mult_a
    ldv a2i, 0x0, 0x10, mtx_mult_a
    ldv a2i, 0x8, 0x10, mtx_mult_a
    ldv a3i, 0x0, 0x18, mtx_mult_a
    ldv a3i, 0x8, 0x18, mtx_mult_a
    ldv a0f, 0x0, 0x20, mtx_mult_a
    ldv a0f, 0x8, 0x20, mtx_mult_a
    ldv a1f, 0x0, 0x28, mtx_mult_a
    ldv a1f, 0x8, 0x28, mtx_mult_a
    ldv a2f, 0x0, 0x30, mtx_mult_a
    ldv a2f, 0x8, 0x30, mtx_mult_a
    ldv a3f, 0x0, 0x38, mtx_mult_a
    ldv a3f, 0x8, 0x38, mtx_mult_a

    # Load matrix b
    lqv b01i, 0x0, 0x00, mtx_mult_b
    lqv b23i, 0x0, 0x10, mtx_mult_b
    lqv b01f, 0x0, 0x20, mtx_mult_b
    lqv b23f, 0x0, 0x30, mtx_mult_b

    # Multiply rows of a with columns of b

    # Rows 0,1,2,3 with columns 0,1
    vmudl vtemp, a0f, b01f, VE_0h
    vmadm vtemp, a0i, b01f, VE_0h
    vmadn vtemp, a0f, b01i, VE_0h
    vmadh vtemp, a0i, b01i, VE_0h
    vmadl vtemp, a1f, b01f, VE_1h
    vmadm vtemp, a1i, b01f, VE_1h
    vmadn vtemp, a1f, b01i, VE_1h
    vmadh vtemp, a1i, b01i, VE_1h
    vmadl vtemp, a2f, b01f, VE_2h
    vmadm vtemp, a2i, b01f, VE_2h
    vmadn vtemp, a2f, b01i, VE_2h
    vmadh vtemp, a2i, b01i, VE_2h
    vmadl vtemp, a3f, b01f, VE_3h
    vmadm vtemp, a3i, b01f, VE_3h
    vmadn ab01f, a3f, b01i, VE_3h
    vmadh ab01i, a3i, b01i, VE_3h
    # ab01 = r0xc0, r1xc0, r2xc0, r3xc0, r0xc1, r1xc1, r2xc1, r3xc1

    # Rows 0,1,2,3 with columns 2,3
    vmudl vtemp, a0f, b23f, VE_0h
    vmadm vtemp, a0i, b23f, VE_0h
    vmadn vtemp, a0f, b23i, VE_0h
    vmadh vtemp, a0i, b23i, VE_0h
    vmadl vtemp, a1f, b23f, VE_1h
    vmadm vtemp, a1i, b23f, VE_1h
    vmadn vtemp, a1f, b23i, VE_1h
    vmadh vtemp, a1i, b23i, VE_1h
    vmadl vtemp, a2f, b23f, VE_2h
    vmadm vtemp, a2i, b23f, VE_2h
    vmadn vtemp, a2f, b23i, VE_2h
    vmadh vtemp, a2i, b23i, VE_2h
    vmadl vtemp, a3f, b23f, VE_3h
    vmadm vtemp, a3i, b23f, VE_3h
    vmadn ab23f, a3f, b23i, VE_3h
    vmadh ab23i, a3i, b23i, VE_3h
    # ab23 = r0xc2, r1xc2, r2xc2, r3xc2, r0xc3, r1xc3, r2xc3, r3xc3

    # Write result to mtx_mult_out
    sqv ab01i, 0x0, 0x00, mtx_mult_out
    sqv ab23i, 0x0, 0x10, mtx_mult_out
    sqv ab01f, 0x0, 0x20, mtx_mult_out
    jr ra
    sqv ab23f, 0x0, 0x30, mtx_mult_out
    
  #undef a0i
  #undef a0f
  #undef a1i
  #undef a1f
  #undef a2i
  #undef a2f
  #undef a3i
  #undef a3f
  #undef b01i
  #undef b01f
  #undef b23i
  #undef b23f
  #undef ab01i
  #undef ab01f
  #undef ab23i
  #undef ab23f
  #undef vtemp
