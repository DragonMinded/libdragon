#include "../src/regs.S"

	.set noreorder
    
    .text

    # This file contains functions used by test_backtrace.c to test
    # the backtrace analysis code. The code of this functions is not
    # run, but just scanned to extract the stack frame information.

    # BT1: a function with a stack frame that does not use FP as framepointer
    # but dirties it.
test_bt_1:
    addiu   sp,sp,-112     # <- stack frame 112
    sd      s3,56(sp)
    lw      s3,148(sp)
    lw      v0,4(a0)
    lw      v1,0(a0)
    sd      s5,72(sp)
    sd      s4,64(sp)
    addiu   s5,a0,8
    addu    s4,a1,a2
    subu    a2,s3,a2
    sd      s7,88(sp)
    sd      s6,80(sp)
    sd      s2,48(sp)
    sd      s1,40(sp)
    sd      s0,32(sp)
    sd      ra,104(sp)    # <- ra offset 104
    sd      fp,96(sp)     # <- fp offset 96
    .globl test_bt_1_start
test_bt_1_start:
    move    s1,a0
    

    # BT2: a function using FP as framepointer
test_bt_2:
    addiu   sp,sp,-128   # <- stack frame 128
    sd      fp,112(sp)   # <- fp offset 112
    move    fp,sp        # <- fp used as framepointer
    sd      s0,48(sp)
    move    s0,a0
    lw      a0,188(fp)
    sd      s7,104(sp)
    sd      s5,88(sp)
    sd      s2,64(sp)
    sd      s1,56(sp)
    sd      ra,120(sp)
    sd      s6,96(sp)
    sd      s4,80(sp)
    sd      s3,72(sp)
    addiu   a0,a0,108
    sw      s0,128(fp)
    lw      s2,172(fp)
    sd      a2,144(fp)
    sd      a3,152(fp)
    # [...]
    ld      ra,120(sp)
    ld      fp,112(sp)
    ld      s7,104(sp)
    ld      s6,96(sp)
    ld      s5,88(sp)
    ld      s4,80(sp)
    ld      s3,72(sp)
    ld      s2,64(sp)
    ld      s1,56(sp)
    ld      s0,48(sp)
    jr      ra            # <- return point in the middle of the function
    addiu   sp,sp,128
    # [...]
    lw      v0,0(a0)
    sll     v0,v0,0x2
    addiu   sp,sp,-180   # <- potentially confusing alloca (not the real stack frame)
    addu    v0,s2,v0
    lw      v0,0(v0)
    addu    v0,v0,s4
    .globl test_bt_2_start
test_bt_2_start:
    lb      v0,0(v0)

    # BT3: a function changing FP with a leaf basis block
test_bt_3:
    addiu   sp,sp,-80
    sd      ra,20(sp)
    sd      fp,16(sp)
    # [...]
    ld      fp,16(sp)
    ld      ra,20(sp)
    ld      s0,48(sp)
    jr      ra            # <- return point in the middle of the function
    addiu   sp,sp,80
    .globl test_bt_3_start
test_bt_3_start:
    lb      v0,0(v0)      # <- leaf basis block


    # BT4: a leaf function preceded by alignment nops
    nop; nop; nop; nop
    .align 5
test_bt_4:
    lw      a3,-29740(gp)
    lui     t5,0x51eb
    sll     v1,a3,0x3
    ori     t5,t5,0x851f
    mult    v1,t5
    sll     t0,a3,0x5
    sra     t1,t0,0x1f
    sra     v1,v1,0x1f
    dsra32  t2,a1,0x0
    mfhi    v0
    sra     v0,v0,0x5
    subu    v0,v0,v1
    mult    t0,t5
    addu    v0,v0,t2
    sd      a1,8(sp)
    move    t3,t2
    .globl test_bt_4_start
test_bt_4_start:
    mfhi    t0

    # BT5: a leaf function without nop, identified via explicit start address
    addiu   sp,sp,-80   # fake precedeing stack frame
    sd      ra,20(sp)
    ld      ra,20(sp)
    jr      ra
    addiu   sp,sp,80
    .globl  test_bt_5
test_bt_5:
    lw      a3,-29740(gp)
    lui     t5,0x51eb
    sll     v1,a3,0x3
    ori     t5,t5,0x851f
    mult    v1,t5
    sll     t0,a3,0x5
    sra     t1,t0,0x1f
    sra     v1,v1,0x1f
    dsra32  t2,a1,0x0
    mfhi    v0
    sra     v0,v0,0x5
    subu    v0,v0,v1
    mult    t0,t5
    addu    v0,v0,t2
    sd      a1,8(sp)
    move    t3,t2
    .globl test_bt_5_start
test_bt_5_start:
    mfhi    t0
