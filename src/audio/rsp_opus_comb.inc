
#if !defined(RSP_OPUS_COMB_DUAL) || (RSP_OPUS_COMB_DUAL != 0 && RSP_OPUS_COMB_DUAL != 1)
#error "RSP_OPUS_COMB_DUAL must be defined to either 0 or 1"
#endif

#if RSP_OPUS_COMB_DUAL
#define D(...) __VA_ARGS__
#else
#define D(...)
#endif

    #define total_samples   k0
    #define num_samples     k1
    #define gains_01        a1
    #define gains_2         a2
    #define dmem_in_i0      s0
    #define dmem_in_f0      s1
    #define dmem_in_i1      s2
    #define dmem_in_f1      s3
    #define dmem_in_i2      s4
    #define dmem_in_f2      s5
    #define dmem_in_i3      s6
    #define dmem_in_f3      s7
    #define dmem_in_i4      t9
    #define dmem_in_f4      t8
    #define dmem_out_i      t7
    #define dmem_out_f      t6

    #define dmem_in2_i0      t5
    #define dmem_in2_f0      t4
    #define dmem_in2_i1      t3
    #define dmem_in2_f1      t2
    #define dmem_in2_i2      fp
    #define dmem_in2_f2      sp
    #define dmem_in2_i3      ra
    #define dmem_in2_f3      v0
    #define dmem_in2_i4      v1
    #define dmem_in2_f4      a0
    #define window           a1

    #define vsig0i         $v01
    #define vsig0f         $v02
    #define vsig1i         $v03
    #define vsig1f         $v04
    #define vsig2i         $v05
    #define vsig2f         $v06
    #define vsig3i         $v07
    #define vsig3f         $v08
    #define vsig4i         $v09
    #define vsig4f         $v10
    #define vouti          $v11
    #define voutf          $v12
    #define vgains         $v13
    #define v____          $v29

    #define vgain0         $v14
    #define vgain1         $v15
    #define vgain2         $v16
    #define vgain0_inv     $v17
    #define vgain1_inv     $v18
    #define vgain2_inv     $v19

    #define vwin           $v20
    #define vwin_inv       $v21
    #define vone           $v22

    srl total_samples, a0, 8
    andi total_samples, 0xFF
    addiu total_samples, 1
    sll total_samples, 3

    # Output buffer (containing input data too): x[i]/y[i]
    andi t0, a0, 0xFF
    sll t0, 3    # multiply by 4 to recover index, and by 2 because 16-bit
    addiu dmem_out_i, t0, %lo(COMB_I)
    addiu dmem_out_f, t0, %lo(COMB_F)

    # Start of samples to use for the comb filter (x[i-T-2])
    srl t0, a3, 16
    andi t0, 0xFFFF
    sll t0, 1    # multiply by 2 because 16-bit
    addiu dmem_in_i0, t0, %lo(COMB_I)
    addiu dmem_in_f0, t0, %lo(COMB_F)
    addiu dmem_in_i1, dmem_in_i0, 2
    addiu dmem_in_f1, dmem_in_f0, 2
    addiu dmem_in_i2, dmem_in_i1, 2
    addiu dmem_in_f2, dmem_in_f1, 2
    addiu dmem_in_i3, dmem_in_i2, 2
    addiu dmem_in_f3, dmem_in_f2, 2
    addiu dmem_in_i4, dmem_in_i3, 2
    addiu dmem_in_f4, dmem_in_f3, 2

    #if RSP_OPUS_COMB_DUAL
    andi t0, a3, 0xFFFF
    sll t0, 1    # multiply by 2 because 16-bit
    addiu dmem_in2_i0, t0, %lo(COMB_I)
    addiu dmem_in2_f0, t0, %lo(COMB_F)
    addiu dmem_in2_i1, dmem_in2_i0, 2
    addiu dmem_in2_f1, dmem_in2_f0, 2
    addiu dmem_in2_i2, dmem_in2_i1, 2
    addiu dmem_in2_f2, dmem_in2_f1, 2
    addiu dmem_in2_i3, dmem_in2_i2, 2
    addiu dmem_in2_f3, dmem_in2_f2, 2
    addiu dmem_in2_i4, dmem_in2_i3, 2
    addiu dmem_in2_f4, dmem_in2_f3, 2

    li t0, 0x7FFF
    mtc2 t0, vone.e0
    vor vone, vzero, vone.e0
    #endif

    #if RSPQ_DEBUG
    # Verify that we did our calculations correctly, and dmem_out are
    # 16-byte aligned. We need this as we want to just use "sqv" to store
    # them in the main loop.
    and t0, dmem_out_i, 0xF
    assert_eq t0, 0, 0x8100
    and t0, dmem_out_f, 0xF
    assert_eq t0, 0, 0x8101
    #endif

    #D(emux_breakpoint)
    li t1, 0x4000
    mtc2 t1, vgains.e3
    mtc2 gains_01, vgains.e0
    srl gains_01, 16
    mtc2 gains_01, vgains.e1
    mtc2 gains_2, vgains.e2
    vor vgain0, vzero, vgains.e0
    vor vgain1, vzero, vgains.e1
    vor vgain2, vzero, vgains.e2

    #if RSP_OPUS_COMB_DUAL
    srl gains_2, 16
    mtc2 gains_2, vgains.e4
    lw t0, CMD_ADDR(16, 20)
    mtc2 t0, vgains.e5
    srl t0, 16
    mtc2 t0, vgains.e6

    # Initial window value
    li window, %lo(WINDOW2)
    lqv vwin, 0x00,window
    addiu window, 0x10
    vsub vwin_inv, vone, vwin
    #endif

    # Make first out-of-phase store idempotent.
    lqv voutf, -0x10,dmem_out_f
    lqv vouti, -0x10,dmem_out_i
    # Load initial samples (same instrucitons
    # made at the end of the loop).
    lqv vsig2i, 0x00,dmem_in_i2 
    lqv vsig2f, 0x00,dmem_in_f2;
    lqv vsig3i, 0x00,dmem_in_i3;
    lqv vsig3f, 0x00,dmem_in_f3;

    # Prepare loop iteration
    addiu num_samples, total_samples, -8

    # SINGLE: 39 cycles / 8 samples
    # DUAL:   71 cycles / 8 samples
1:
    #emux_breakpoint
    #D(emux_log_string "loop\n")

    # Store previous loop result.
    sqv vouti, -0x10,dmem_out_i;            
    sqv voutf, -0x10,dmem_out_f; 

    # Load x[i-T-2]..x[i+T+2] into vsig
    # registers. Since the data is misaligned
    # we need both lqv and lrv here.
    # NOTE: when T=15 (samllest), vsig0/1 are partly
    # affected by previous loop results,
    # so we must load them *after* storing.
    lqv vsig0i, 0x00,dmem_in_i0;            D(vmudn vzero, vzero) 
    lqv vsig0f, 0x00,dmem_in_f0;            D(vmacf vgain0_inv, vwin, vgains.e4)
    lqv vsig1i, 0x00,dmem_in_i1;            D(vmudn vzero, vzero)
    lqv vsig1f, 0x00,dmem_in_f1;            D(vmacf vgain1, vwin_inv, vgains.e1)
    lqv vsig4i, 0x00,dmem_in_i4;            D(vmudn vzero, vzero)
    lqv vsig4f, 0x00,dmem_in_f4;            D(vmacf vgain2, vwin_inv, vgains.e2)
    lrv vsig0i, 0x10,dmem_in_i0;            D(vmudn vzero, vzero)
    lrv vsig0f, 0x10,dmem_in_f0;            D(vmacf vgain0, vwin_inv, vgains.e0)
    lrv vsig1i, 0x10,dmem_in_i1;            D(vmudn vzero, vzero)
    lrv vsig1f, 0x10,dmem_in_f1;            D(vmacf vgain1_inv, vwin, vgains.e5)
    lrv vsig2i, 0x10,dmem_in_i2;            D(vmudn vzero, vzero)             
    lrv vsig2f, 0x10,dmem_in_f2;            D(vmacf vgain2_inv, vwin, vgains.e6)
    #D(emux_dump_gpr dmem_in_i0, dmem_in_f0)
    #D(emux_dump_vpr vsig0f, vsig0i)      
                                            # Main MAC sequence for FIR filter
    lrv vsig3i, 0x10,dmem_in_i3;            vmudn v____, vsig0f, vgain2
    lrv vsig3f, 0x10,dmem_in_f3;            vmadh v____, vsig0i, vgain2
    lrv vsig4i, 0x10,dmem_in_i4;            vmadn v____, vsig1f, vgain1
    lrv vsig4f, 0x10,dmem_in_f4;            vmadh v____, vsig1i, vgain1
    addiu dmem_in_i0, 0x10;                 vmadn v____, vsig2f, vgain0
    addiu dmem_in_f0, 0x10;                 vmadh v____, vsig2i, vgain0
    # Load x[i] from output buffer
    lqv voutf, 0x00,dmem_out_f;             vmadn v____, vsig3f, vgain1
    lqv vouti, 0x00,dmem_out_i;             vmadh v____, vsig3i, vgain1
    addiu dmem_out_i, 0x10;                 vmadn v____, vsig4f, vgain2
    addiu dmem_out_f, 0x10;                 vmadh v____, vsig4i, vgain2

    D(lqv vsig0f, 0x00,dmem_in2_f0);       
    D(lrv vsig0f, 0x10,dmem_in2_f0);       
    D(lqv vsig0i, 0x00,dmem_in2_i0);        
    D(lrv vsig0i, 0x10,dmem_in2_i0);  
    D(lqv vsig1f, 0x00,dmem_in2_f1);        
    D(lrv vsig1f, 0x10,dmem_in2_f1);        
    D(lqv vsig1i, 0x00,dmem_in2_i1)
    D(lrv vsig1i, 0x10,dmem_in2_i1)
    D(lqv vsig2f, 0x00,dmem_in2_f2)
    D(lrv vsig2f, 0x10,dmem_in2_f2)
    D(lqv vsig2i, 0x00,dmem_in2_i2)
    D(lrv vsig2i, 0x10,dmem_in2_i2)
    D(lqv vsig3f, 0x00,dmem_in2_f3)
    D(lrv vsig3f, 0x10,dmem_in2_f3)
    D(lqv vsig3i, 0x00,dmem_in2_i3)
    D(lrv vsig3i, 0x10,dmem_in2_i3)
    D(lqv vsig4f, 0x00,dmem_in2_f4)
    D(lrv vsig4f, 0x10,dmem_in2_f4)
    D(lqv vsig4i, 0x00,dmem_in2_i4)
    D(lrv vsig4i, 0x10,dmem_in2_i4)
    #D(emux_dump_gpr dmem_in2_i0, dmem_in2_f0)
    #D(emux_dump_vpr vsig0f, vsig0i, vsig1f, vsig1i, vsig2f, vsig2i, vsig3f, vsig3i, vsig4f, vsig4i)
    D(addiu dmem_in2_i0, 0x10;)             D(vmadn v____, vsig0f, vgain2_inv)
    D(addiu dmem_in2_f0, 0x10;)             D(vmadh v____, vsig0i, vgain2_inv)
    D(addiu dmem_in2_i1, 0x10;)             D(vmadn v____, vsig1f, vgain1_inv)
    D(addiu dmem_in2_f1, 0x10;)             D(vmadh v____, vsig1i, vgain1_inv)
    D(addiu dmem_in2_i2, 0x10;)             D(vmadn v____, vsig2f, vgain0_inv)
    D(addiu dmem_in2_f2, 0x10;)             D(vmadh v____, vsig2i, vgain0_inv)
    D(addiu dmem_in2_i3, 0x10;)             D(vmadn v____, vsig3f, vgain1_inv)
    D(addiu dmem_in2_f3, 0x10;)             D(vmadh v____, vsig3i, vgain1_inv)
    D(addiu dmem_in2_i4, 0x10;)             D(vmadn v____, vsig4f, vgain2_inv)
    D(addiu dmem_in2_f4, 0x10;)             D(vmadh v____, vsig4i, vgain2_inv)

                                            # Add x[i] * 0x4000
    addiu dmem_in_i1, 0x10;                 vmadn v____, voutf, vgains.e3
    addiu dmem_in_f1, 0x10;                 vmadh v____, vouti, vgains.e3
                                            # Add x[i] * 0x4000 (total: 0x8000)
    addiu dmem_in_i2, 0x10;                 vmadn v____, voutf, vgains.e3
    addiu dmem_in_f2, 0x10;                 vmadh v____, vouti, vgains.e3
    addiu dmem_in_i3, 0x10;
    addiu dmem_in_f3, 0x10;
                                            # Extract result >> 16.
    addiu dmem_in_i4, 0x10;                 vsar voutf, COP2_ACC_MD
    addiu dmem_in_f4, 0x10;                 vsar vouti, COP2_ACC_HI

    D(lqv vwin, 0x00,window);

    # Start fetching next samples (we
    # have 2 cycles of dealy before voutf
    # is available, so better use them).
    # We start from vsig2/3, arbitrarily.
    # We can't load vsig0/1 here though because
    # we haven't yet stored vouf/vouti,
    # and when T=15 (minimum), the previous
    # loop result does affect vsig0/1
    lqv vsig2i, 0x00,dmem_in_i2             
    lqv vsig2f, 0x00,dmem_in_f2;            # We need result >> 15. Shift left by 1.
    lqv vsig3i, 0x00,dmem_in_i3;            vsll32f_sat voutf, voutf, 1
    lqv vsig3f, 0x00,dmem_in_f3;            vsll32i_sat vouti, vouti, 1

    #D(emux_dump_vpr voutf, vouti)

    D(addiu window, 0x10);                  D(vsub vwin_inv, vone, vwin)

    bgtz num_samples, 1b
    addiu num_samples, -8

    # Loop epilog
    sqv voutf, -0x10,dmem_out_f
    sqv vouti, -0x10,dmem_out_i

    j RSPQ_Loop
    nop

    #undef total_samples
    #undef num_samples  
    #undef gains_01     
    #undef gains_2      
    #undef dmem_in_i0   
    #undef dmem_in_f0   
    #undef dmem_in_i1   
    #undef dmem_in_f1   
    #undef dmem_in_i2   
    #undef dmem_in_f2   
    #undef dmem_in_i3   
    #undef dmem_in_f3   
    #undef dmem_in_i4   
    #undef dmem_in_f4   
    #undef dmem_out_i   
    #undef dmem_out_f   
    #undef dmem_in2_i0  
    #undef dmem_in2_f0  
    #undef dmem_in2_i1  
    #undef dmem_in2_f1  
    #undef dmem_in2_i2  
    #undef dmem_in2_f2  
    #undef dmem_in2_i3  
    #undef dmem_in2_f3  
    #undef dmem_in2_i4  
    #undef dmem_in2_f4  
    #undef window       
    #undef vsig0i       
    #undef vsig0f       
    #undef vsig1i       
    #undef vsig1f       
    #undef vsig2i       
    #undef vsig2f       
    #undef vsig3i       
    #undef vsig3f       
    #undef vsig4i       
    #undef vsig4f       
    #undef vouti        
    #undef voutf        
    #undef vgains       
    #undef v____        
    #undef vgain0       
    #undef vgain1       
    #undef vgain2       
    #undef vgain0_inv   
    #undef vgain1_inv   
    #undef vgain2_inv   
    #undef vwin         
    #undef vwin_inv     
    #undef vone         

#undef D
