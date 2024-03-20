#########################################################################
#
#  RDPQ_Triangle: assemble a RDP triangle command
#
##########################################################################

    .section .text.rdpq_triangle_send_async

#ifndef RDPQ_TRIANGLE_PROFILE
#define RDPQ_TRIANGLE_PROFILE     0
#endif

/* Set RDPQ_TRIANGLE_CUSTOM_VTX to 1 if you want to define a custom layout
   for your vertices. If you do so, you must define also all the VTX_ATTR
   macros below containing the offsets to the various vertex components. */
#ifndef RDPQ_TRIANGLE_CUSTOM_VTX
#define RDPQ_TRIANGLE_CUSTOM_VTX  0
#endif

#if !RDPQ_TRIANGLE_CUSTOM_VTX
    #define VTX_ATTR_XY          0
    #define VTX_ATTR_Z           4
    #define VTX_ATTR_CLIPFLAGS   6         // Clipping codes + Negated trivial-reject codes
    #define VTX_ATTR_RGBA        8
    #define VTX_ATTR_ST         12
    #define VTX_ATTR_Wi         16
    #define VTX_ATTR_Wf         18
    #define VTX_ATTR_INVWi      20
    #define VTX_ATTR_INVWf      22
#endif

/* Define these macros if you want to specify the input registers for the three
   vertices. */
#ifndef RDPQ_TRIANGLE_VTX1
#define RDPQ_TRIANGLE_VTX1  a1
#endif

#ifndef RDPQ_TRIANGLE_VTX2
#define RDPQ_TRIANGLE_VTX2  a2
#endif

#ifndef RDPQ_TRIANGLE_VTX3
#define RDPQ_TRIANGLE_VTX3  a3
#endif

/* Define these macros if the input vertex pointers, in addition to the input
   regigsters, are already available in DMEM somewhere. You must define the
   address to access each vertex. */
#ifndef RDPQ_TRIANGLE_VTX1_DMEM
#define RDPQ_TRIANGLE_VTX1_DMEM    0,s3
#define STORE_VTX1
#endif
#ifndef RDPQ_TRIANGLE_VTX2_DMEM
#define RDPQ_TRIANGLE_VTX2_DMEM    2,s3
#define STORE_VTX2
#endif
#ifndef RDPQ_TRIANGLE_VTX3_DMEM
#define RDPQ_TRIANGLE_VTX3_DMEM    4,s3
#define STORE_VTX3
#endif



    #####################################################################
    # RDPQ_Triangle_Send_Async
    #
    # Build a RDP triangle primitive from 3 screen space vertices, and
    # send it to the RDP for drawing (asynchronously). There is no need
    # to call RDPQ_Send after this call.
    #
    # After this function returns, the DMA to RDRAM of the RDP primitive
    # might be in progress. You can call this function multiple times for
    # multiple triangles. After you finish the batch, make sure to call
    # RDPQ_Triangle_Send_End to wait for the last triangle finish sending
    # the last triangle to the RDP.
    #
    # INPUT:
    # * a0: high 16-bit word of the triangle command. This will be
    #       completed with the left/right flag and the mipmap level.
    # * a1,a2,a3: pointer to the triangle structures in DMEM
    # * v0: 0=cull front, 1=cull back, any other value = culling disabled
    # * s3: output buffer pointer
    #
    # OUTPUT:
    # * v1: 1=clipping required, 0=nothing to do
    # * s3: pointer to the end of the primitive in the output buffer
    #
    # CLOBBER:
    # t0-t6, a0-a3
    #
    #####################################################################

    # Implementation limits of the RSP version. These are all edge cases that are probably
    # not necessary to get 100% right as they are really degenerate situations. Notice that
    # most extreme/degenerated/saturated cases are still handled correctly, as verified
    # by the fuzzing performed by test_rdpq_triangle; these are just the three leftovers.
    #
    # * Texture coordinates are accepted in s10.5 format, but a subtraction between two of them
    #   must not overflow a 16-bit number. This is a limit of the attribute calculation where the
    #   edges MA/HA are calculated with 16-bit numbers. It looks like it's not a real problem as
    #   it would mean having a triangle where either S or T spans more than 1024 texels within it.
    #   Fixing it wuold require changing MA/HA into 32-bit numbers, which has other annoying fallouts.
    # * In case of quasi-degenerate triangles (lines), when the Y difference between two vertices
    #   is just 0.25 (0x00000001), the correct normal reciprocal would be 1.0, but it is calculated
    #   as 0x7FFF8000 which is 0.5 (because it's basically saturating s15.16). This means that the calculated
    #   edge is twice as big. Again, it doesn't matter as it can't really be seen within a 0.25 slope.
    #   test_rdpq_triangle has a triangle that triggers this, commented out.
    # * In some cases, Z/W-related derivates (DwDx, DwDy, DzDx, DzDy) can saturate during calculation.
    #   in this case, the dependent D*De derivates will be wrong (how much it will depend on how far
    #   the real result is from the saturated number). In any case, much better than an overflow.
    #   test_rdpq_triangle checks if there's a saturation and skip checks for known-wrong values.

    .func RDPQ_Triangle_Send_Async
RDPQ_Triangle_Send_Async:
    #define tricmd a0
    #define vtx1   RDPQ_TRIANGLE_VTX1   // a1
    #define vtx2   RDPQ_TRIANGLE_VTX2   // a2
    #define vtx3   RDPQ_TRIANGLE_VTX3   // a3
    #define cull   v0
    #define out_clip_flag   v1

    # r, g, b, a, s, t, w, z
    #define vfinal_i         $v01
    #define vfinal_f         $v02
    #define vdx_i            $v03
    #define vdx_f            $v04
    #define vde_i            $v05
    #define vde_f            $v06
    #define vdy_i            $v07
    #define vdy_f            $v08

    #define vattr1           $v09
    #define vattr2           $v10
    #define vattr3           $v11
    #define attr1_r     vattr1.e0
    #define attr2_r     vattr2.e0
    #define attr3_r     vattr3.e0
    #define attr1_s     vattr1.e4
    #define attr2_s     vattr2.e4
    #define attr3_s     vattr3.e4
    #define attr1_invw  vattr1.e6
    #define attr2_invw  vattr2.e6
    #define attr3_invw  vattr3.e6
    #define attr1_z     vattr1.e7
    #define attr2_z     vattr2.e7
    #define attr3_z     vattr3.e7
    #define vma              $v12
    #define vha              $v13

    #define vw_i             $v07
    #define vw_f             $v08

    #define vinvw_i          $v14
    #define vinvw_f          $v15

    #define vedges_i         $v16
    #define vedges_f         $v17
    #define vnz_i            $v18
    #define vnz_f            $v19
    #define vslope_i         $v20
    #define vslope_f         $v21
    #define vx12_i           $v22
    #define vx12_f           $v23

    #define vhml             $v24
    #define vfy_i            $v25
    #define vfy_f            $v26

    #define vmconst          $v27
    #define VKM1             vmconst.e7
    #define VKM4             vmconst.e5

    #define vtmp             $v28
    #define v__              $v29
    #define invn_i           $v31.e4
    #define invn_f           $v31.e5
    #define invsh_i          $v31.e6
    #define invsh_f          $v31.e7

#if RDPQ_TRIANGLE_PROFILE
    mfc0 fp, COP0_DP_CLOCK
    emux_trace_start
#endif

    #define vall1    $v01
    #define vall2    $v02
    #define vall3    $v03
    #define valltmp1 $v04
    #define valltmp2 $v05
    #define vy1      $v06
    #define vy2      $v07
    #define vy3      $v08
    #define vytmp1   $v09
    #define vytmp2   $v10
  
    #define vm      valltmp2
    #define vl      valltmp1
    #define hx      vhml.e0
    #define hy      vhml.e1
    #define mx      vm.e0
    #define my      vm.e1
    #define lx      vl.e0
    #define ly      vl.e1
    #define vhmlupp vtmp

    #define vk1     $v12

    #define vstall
    #define stall

    #define clip1  t3
    #define clip2  t4
    #define clip3  t5
    #define did_swap_0     t0
    #define did_swap_1     t1
    #define did_swap_2     t2


    # vall registers:
    # X1 Y1 [Z1 ADDR1  RG1 BA1 S1 T1]
    # X2 Y2 [Z2 ADDR2  RG2 BA2 S2 T2]
    # X3 Y3 [Z3 ADDR3  RG3 BA3 S3 T3]

    # TODO:
    # Should we do backface culling before clipping check?
    # If so, should we do that BEFORE or AFTER the sort-of-three?

    #ifdef STORE_VTX1
    sh vtx1, 0(s3)
    #endif
    #ifdef STORE_VTX2
    sh vtx2, 2(s3)
    #endif
    #ifdef STORE_VTX3
    sh vtx3, 4(s3)
    #endif
    llv vall1, VTX_ATTR_XY,vtx1
    llv vall2, VTX_ATTR_XY,vtx2
    llv vall3, VTX_ATTR_XY,vtx3;                # Create a vector for negative pow2 constants
    lhu clip1, VTX_ATTR_CLIPFLAGS(vtx1);        vsubc vmconst, vzero, vshift;

    vcopy vy1, vall1.e1;                        lsv vall1.e3, RDPQ_TRIANGLE_VTX1_DMEM
    vcopy vy2, vall2.e1;                        lsv vall2.e3, RDPQ_TRIANGLE_VTX2_DMEM
    vcopy vy3, vall3.e1;                        lsv vall3.e3, RDPQ_TRIANGLE_VTX3_DMEM

    # Create vector with constants      
    # 1 and -1 (to be used for
    # cross product)                            # Finish loading clipping codes
    vcopy vk1, K1;                              lhu clip3, VTX_ATTR_CLIPFLAGS(vtx3)
    # Clear VCO to make sure the first
    # VGE does the right thing if 
    # vy1==vy2
    vsubc vzero, vzero, vzero;                  lhu clip2, VTX_ATTR_CLIPFLAGS(vtx2)

    # Do a sort-of-three (min, mid, max), using vyN as key of the sort,
    # and vallN as values to sort. We compare the keys with vlt/vge,
    # and then use vmrg to apply the swap.
    li out_clip_flag, 0;                        vge vytmp1, vy1, vy2;               
    or clip1, clip3;                            vmrg valltmp1, vall1, vall2;        
    or clip1, clip2;                            vlt vy1, vy1, vy2;                  
    cfc2 did_swap_0, COP2_CTRL_VCC;             vmrg vall1, vall1, vall2;

    andi t4, clip1, 0xFF;                       
    xori t4, 0xFF;                              vxor vhmlupp, vhmlupp

    beqz t4, JrRa;                              vge vytmp2, vy1, vy3;               
    andi clip1, 0x3F00;                         vmrg valltmp2, vall1, vall3;
    lbu t4, %lo(RDPQ_OTHER_MODES) + 0;          vlt vy1, vy1, vy3;                  
    cfc2 did_swap_1, COP2_CTRL_VCC;             vmrg vall1, vall1, vall3;           

    nop;                                        vge vy3, vytmp1, vytmp2;            
    andi t4, 0x38;                              vmrg vall3, valltmp1, valltmp2;     
    or tricmd, t4;                              vlt vy2, vytmp1, vytmp2;            
    cfc2 did_swap_2, COP2_CTRL_VCC;             vmrg vall2, valltmp1, valltmp2;     

    # Copy Y1, Y2 into vy3, so that we get the three sorted Y in the same
    # vector.
    #  vy3:   Y1 Y2 Y3 Y3  Y3 Y3 Y3 Y3 

    # Build vhml:
    #    vhml      =   HX HY MX MY    LX LY  0  NZf
    vmov vy3.e3, vy1.e0;                        xor did_swap_0, did_swap_1
    vsubc vhml, vall3, vall1;
    vmov vy3.e2, vy2.e0;                        xor did_swap_0, did_swap_2
    vsubc vm, vall2, vall1;                     mfc2 vtx1, vall1.e3
    vsubc vl, vall3, vall2;                     mfc2 vtx2, vall2.e3

    # vx12 = X1 -- X2 --  -- -- -- --
    vmov vx12_f.e0, vall1.e0;                   andi did_swap_0, 1
    vmov vx12_f.e2, vall2.e0;                   xor cull, did_swap_0

    # Prepare -MY for crossprod
    vsubc v__, vzero, my;                       mfc2 vtx3, vall3.e3
    
    # FY.e4 = Y1 (s15.16)
    vsll8 vfy_f, vy1, 14;                       sdv vy3.e0, 0,s3
    vsra  vfy_i, vy1, 2;                        bnez clip1, JrRa
                                                li out_clip_flag, 1     # report clipping required
    vmudh vnz_f, vm, hy;                        li out_clip_flag, 0     # report no clipping required
    vmadh vnz_f, vhml, v__;                     slv vm.e0,   8,s3
    vsar vnz_i, COP2_ACC_HI;                    slv vl.e0,  12,s3
    vsar vnz_f, COP2_ACC_MD;                    ldv vhml.e2, 8,s3

    #undef clip1
    #undef clip2
    #undef clip3
    #define prim_size       t3
    #define color_dmem      t4
    #define tex_dmem        t5
    #define z_dmem          t6

    # FY.e0 = floorf(y1) - y1
    # TODO: this is always a negative
    # fraction, so fy_i is always 0xFFFF
    # (or fy_i=fy_f=0). See if we can           # Begin calculating DMEM offsets:
    # take advantage of this somehow            # color_dmem, tex_dmem, z_dmem
    # to simplify later.                        # This will happen throughout the whole function
    vsubc vfy_f, vzero, vfy_f;                  addu color_dmem, s3, 0x20
    vsub  vfy_i, vfy_i;                         lsv vw_f.e0, VTX_ATTR_Wf,vtx1

    # Compute SLOPE vector
    # slope    =  1/HY -- 1/MY --   1/LY -- -- 1/NZ

    # Calculate reciprocal of normal            # Backface culling
    vrcph vslope_i.e7, vnz_i.e0;                mfc2 t0, vnz_i.e0
    vrcpl vslope_f.e7, vnz_f.e0;                lsv vw_f.e2, VTX_ATTR_Wf,vtx2
    vrcph vslope_i.e7, vzero.e0;                lsv vw_f.e4, VTX_ATTR_Wf,vtx3
                                        
    # Compute ISH (H slope). 1/HY  (s14.1)
    vrcp  vslope_f.e0, hy;                      slt t0, t0, zero
    vrcph vslope_i.e0, hy;                      beq t0, cull, JrRa
    # Compute ISM (M slope). 1/MY  (s14.1)
    vrcp  vslope_f.e2, my;                      # <delay slot>
    vrcph vslope_i.e2, my;                      lsv vw_i.e0, VTX_ATTR_Wi,vtx1
    # Compute ISL (L slope). 1/LY  (s14.1)      
    vrcp  vslope_f.e4, ly;                      lsv vw_i.e2, VTX_ATTR_Wi,vtx2
    vrcph vslope_i.e4, ly;                      lsv vw_i.e4, VTX_ATTR_Wi,vtx3

    #undef vm
    #undef vl
    #undef hx
    #undef hy
    #undef mx
    #undef my
    #undef lx
    #undef ly
    #define hx      vhml.e0
    #define hy      vhml.e1
    #define mx      vhml.e2
    #define my      vhml.e3
    #define lx      vhml.e4
    #define ly      vhml.e5

    ##################
    # 1 NR pass
    ##################
    #define vtmp_f       $v03
    #define vtmp_i       $v04
    #define vslopem2_f   $v01
    #define vslopem2_i   $v02

    # Prepare 32-bit number containing the source of the reciprocal
    # Notice that we're calculating NR over 1 32-bit input (NZ) and
    # 3 16-bit inputs (HY, MY, LY), for which we provide 0 in the lower
    # part.
    #    vhml      =   HX HY MX MY    LX LY  0  NZf
    #    vhmlupp   =    0 0  0  0     0  0   0  NZi
                                                # Write left flag into tricmd
    vmov vhml.e7, vnz_f.e0;                     xori t0, 1 
    vmov vhmlupp.e7, vnz_i.e0;                  sll t0, 7

    # vslopem2  = X/2 * (-4) = -2*X             
    vmudn vslopem2_f, vslope_f, VKM4;           luv attr3_r, VTX_ATTR_RGBA,vtx3
    vmadh vslopem2_i, vslope_i, VKM4;           luv attr1_r, VTX_ATTR_RGBA,vtx1

    # Calculate minimum W (between e0 and e2) 
    vsubc v__,  vw_f, vw_f.e2;                  luv attr2_r, VTX_ATTR_RGBA,vtx2
    vlt   vw_i, vw_i, vw_i.e2;                  or tricmd, t0
    vmrg  vw_f, vw_f, vw_f.e2;                  lsv vinvw_f.e6, VTX_ATTR_INVWf,vtx3

    # NR: R*(X/2)                               # Load other invw components
    vmudl vtmp_f, vslope_f, vhml.q1;            lsv vinvw_f.e4, VTX_ATTR_INVWf,vtx1           
    vmadm vtmp_f, vslope_i, vhml.q1;            lsv vinvw_f.e5, VTX_ATTR_INVWf,vtx2
                                                
                                                # Calculate pointers to color, tex
                                                # and z attributes, based on tricmd    
    vmadn vtmp_f, vslope_f, vhmlupp;            andi tex_dmem, tricmd, 0x400
    vmadh vtmp_i, vslope_i, vhmlupp;            srl tex_dmem, 4


    # NR: R*X/2 - 1                             # Calculate -MY/-HX
    vmadn vtmp_f, vk1, VKM1;                    mfc2 t0, my
    vmadh vtmp_i, vk1, VKM1;                    mfc2 t1, hx

    # Calculate minimum W (between e0 and e4)   
    # Now vw_if.e0 contains the minimum W       # Load invw components    
    vsubc v__,  vw_f, vw_f.e4;                  lsv vinvw_i.e5, VTX_ATTR_INVWi,vtx2
    vlt   vw_i, vw_i, vw_i.e4;                  lsv vinvw_i.e6, VTX_ATTR_INVWi,vtx3
    vmrg  vw_f, vw_f, vw_f.e4;                  lsv vinvw_i.e4, VTX_ATTR_INVWi,vtx1

    # NR: -2*X * (R*X/2 - 1)
    #   =  2*X * (1 - R*X/2)
    #   =    X * (2 - R*X)             
    vmudl v__,      vtmp_f, vslopem2_f;         addu tex_dmem, color_dmem
    vmadm v__,      vtmp_i, vslopem2_f;         andi z_dmem, tricmd, 0x200
    vmadn vnz_f,    vtmp_f, vslopem2_i;         srl z_dmem, 3                   
    vmadh vnz_i,    vtmp_i, vslopem2_i;         sh tricmd, 0(s3)

    #undef vtmp_i
    #undef vtmp_f
    #define vtex1 vdx_f
    #define vtex2 vdx_i
    #define vtex3 vde_f

    # We need to normalize INV_W in [0..1], by dividing them by the maximum INV_W.
    # We will multiply by W instead. We have now prepared the minium w in vw_i.e0.
    #
    #    vw:     minw -- -- --    ----- ----- ----- --
    # vinvw:     ---- -- -- --    invw1 invw2 invw3 --
    #
    # We need to multiply minw with the three invw. All numbers are positive s16.16,
    # and the result is known to fit 0..1. By doing a standard 32-bit multiplication
    # on RSP, we end up with a positive s16.16 number, where the integer word is zero.
    # The only exception is when W = 1/W = 1.0. In that case the result of the
    # multiplication will be 1.0, which means that vinw_f will be 0. To handle
    # this case, we later subtract 1 from it, so that it becomes 0xffff, which
    # is a fractional "almost 1".
    #                                           # Load S/T coordinates
    vmudl v__,     vinvw_f, vw_f.e0;            llv vtex1.e4, VTX_ATTR_ST,vtx1
    vmadm v__,     vinvw_i, vw_f.e0;            llv vtex2.e4, VTX_ATTR_ST,vtx2
    vmadn vinvw_f, vinvw_f, vw_i.e0;            llv vtex3.e4, VTX_ATTR_ST,vtx3

    # Finalize slope divisions by multiplying by the reciprocal.
    # vhml      =   HX  HY   MX  MY     LX   LY   0   NZf
    #  *
    # vnz       =  1/HY --  1/MY --    1/LY --   --- 1/NZ
    #    =
    # vslope    =  HX/HY --   MX/MY  --   LX/LY  --     --   --
                                                # Store negated my/hx into vhml
    vmudn v__,  vnz_f, vhml;                    neg t0
    vmadh v__,  vnz_i, vhml;                    neg t1
    vsar  vslope_f, COP2_ACC_MD;                mtc2 t0, my
    vsar  vslope_i, COP2_ACC_HI;                mtc2 t1, hx
    #define ish_f   vslope_f.e0
    #define ish_i   vslope_i.e0
    #define ism_f   vslope_f.e2
    #define ism_i   vslope_i.e2
    #define isl_f   vslope_f.e4
    #define isl_i   vslope_i.e4
                                               
    # Subtract INVW by 1. This workarounds      # Prepare for vmrg later (to merge
    # an overflow when INVW is exactly
    # 1.0.                                       # tex coords inside the attribute)
    vsubc vinvw_f, K1;                          li t0, 0xCF

    # vattr now contains only RGBA, loaded
    # via suv. Shift them back to raw bytes.    # Load Z components into attribute vector
    vsrl vattr1, vattr1, 7;                     lsv attr1_z, VTX_ATTR_Z,vtx1;
    vsrl vattr2, vattr2, 7;                     ctc2 t0, COP2_CTRL_VCC                                          
    vsrl vattr3, vattr3, 7;                     lsv attr2_z, VTX_ATTR_Z,vtx2 

    # Divide vinw_f by 2. This is needed
    # because it is going to be used in a
    # vmulf which expects a 0.15 number.        
    vsrl vinvw_f, vinvw_f, 1;                   lsv attr3_z, VTX_ATTR_Z,vtx3

    #  vxy21 =   X1   --    X2   --       Y1 --   Y2 --
    #  slope = HX/HY  --   MX/MY --     LX/LY --  -- --

    # Compute XH and XM
    # TODO: fy_i is always 0xFFFFFFFF here.
    # See if we can benefit from this.    
    vmudl v__,      vslope_f, vfy_f.e4;         addu z_dmem, tex_dmem
    vmadm v__,      vslope_i, vfy_f.e4;         andi prim_size, tricmd, 0x100
    vmadn vedges_f, vslope_f, vfy_i.e4;         mfc0 t1, COP0_DMA_BUSY
                                                # Copy normalized invw into the attribute vector
    vmadh vedges_i, vslope_i, vfy_i.e4;         sdv vinvw_f.e4, 16,s3

    # Multiply S/T by INVW
    vmulf vtex1, vinvw_f.h0;                    lsv attr1_invw, 16,s3
    vmulf vtex2, vinvw_f.h1;                    lsv attr2_invw, 18,s3
    vmulf vtex3, vinvw_f.h2;                    lsv attr3_invw, 20,s3

    # Convert X1 and X2 into                    # Check if DMA is busy.
    # 16.16 precision                           # If it is, jump to a busy loop, otherwise
    # vx12 = X1 -- X2 --  -- -- -- --           # get on
    vsra  vx12_i, vx12_f, 2;                    bnez t1, rdpq_triangle_dma_busy
                                                 lbu t0, %lo(RDPQ_SYNCFULL_ONGOING)
                                                rdpq_triangle_dma_idle:
    vsll8 vx12_f, vx12_f, 14;                   srl prim_size, 4

                                                #undef tricmd
                                                #define rdram_cur       a0
    # Move S/T into the attribute vector,
    # preserving the other components. VCC
    # was set to 0xCF before, so that only
    # the S/T elements (.e4/.e5) are copied
    vmrg vattr1, vtex1;                         addu prim_size, z_dmem
    vmrg vattr2, vtex2;                         ssv isl_i, 12,s3
    vmrg vattr3, vtex3;                         lw rdram_cur, %lo(RDPQ_CURRENT)        

    # Finish computing XH/XM by adidng
    # X0/X1                                     # Store X2 value in output (as XL)
    vaddc vedges_f, vx12_f.e0;                  ssv vx12_i.e2,  8,s3  # XL_I

                                                # Check if SYNC_FULL is ongoing. If it is not,
                                                # get on with the flow, otherwise skip writing
                                                # to DP_END (we can't do that while SYNC_FULL is ongoing)
    vadd  vedges_i, vx12_i.e0;                  bnez t0, 1f
                                                 ssv isl_f, 14,s3

    # Write the current RDRAM pointer to
    # DP_END; anything before that is already
    # in RAM at this point (because we
    # checked that DMA is idle), the RDP
    # can start running that.                   # Clear the VCC for the next vsub calls
    mtc0 rdram_cur, COP0_DP_END;                1: vsubc vzero, vzero;
                                                   
    ########################################################
    # ATTRIBUTES
    ########################################################
    #undef vtex1
    #undef vtex2
    #undef vtex3
    #undef vtx1
    #undef vtx2
    #undef vtx3
    #define rdram_sentinel  a2

    # MA = A2 - A1
    # HA = A3 - A1
    # NOTE: S/T coordinates are kept as
    # s10.5, so they can overflow here.
    # The subtraction is saturated so
    # the error is minimized, but it is 
    # indeed there. To fix this, we would
    # have to produce a 32-bit result here
    # and then change the DX/DY calcs
    # to use 32-bit numbers as well.
    vsub vma, vattr2, vattr1;                       ssv vx12_f.e2, 10,s3  # XL_F
    vsub vha, vattr3, vattr1;                       sub prim_size, s3

    #define vzout1      $v10
    #undef vattr2

    # Shift left NZ (that contains INVNZ)
    # by 2, to align with the fixed point
    # precision that will be required later.
    vmudn vnz_f, K4;                                ssv ism_i, 28,s3
    vmadh vnz_i, K4;                                ssv ism_f, 30,s3
    #define inz_f   vnz_f.e7
    #define inz_i   vnz_i.e7
               
    # DX = MA * HY + HA * -MY
    vmudh vdx_f, vma, hy;                           ssv vedges_i.e2, 24,s3  # XM_I
    vmadh vdx_f, vha, my;                           ssv vedges_f.e2, 26,s3  # XM_F
    vsar vdx_f, COP2_ACC_MD;                        ssv vedges_i.e0, 16,s3  # XH_I
    vsar vdx_i, COP2_ACC_HI;                        ssv vedges_f.e0, 18,s3  # XH_F

    # DY = HA * MX + MA * -HX                       
    vmudh vdy_f, vha, mx;                           ssv ish_i, 20,s3
    vmadh vdy_f, vma, hx;                           ssv ish_f, 22,s3
    vsar vdy_f, COP2_ACC_MD;                        lw rdram_sentinel, %lo(RDPQ_SENTINEL)
                                                    # Clear MSB of rdram_cur (contains garbage)
    vsar vdy_i, COP2_ACC_HI;                        sll rdram_cur, 8

    # DX * 1/N
    vmudl v__,  vdx_f, inz_f;                       srl rdram_cur, 8
                                                    # Setup DMA addresses
    vmadm v__,  vdx_i, inz_f;                       mtc0 s3, COP0_DMA_SPADDR
    vmadn vdx_f, vdx_f, inz_i;                      mtc0 rdram_cur, COP0_DMA_RAMADDR
                                                    # Calculate final s3 value
    vmadh vdx_i, vdx_i, inz_i;                      addu s3, prim_size

    # DY * 1/N                                      # Compute final RDRAM pointer, and 
                                                    # check if it goes past the sentinel
    vmudl v__,  vdy_f, inz_f;                       add rdram_cur, prim_size
    vmadm v__,  vdy_i, inz_f;                       slt t0, rdram_sentinel, rdram_cur
                                                    # Store Dx slopes
    vmadn vdy_f, vdy_f, inz_i;                      sdv vdx_f.e0, 0x18,color_dmem
    vmadh vdy_i, vdy_i, inz_i;                      sdv vdx_f.e4, 0x18,tex_dmem

    # DE = DX * invsh + DY
    vmadl v__,  vdx_f, ish_f;                       sdv vdx_i.e0, 0x08,color_dmem
    vmadm v__,  vdx_i, ish_f;                       sdv vdx_i.e4, 0x08,tex_dmem
                                                    # Store Dy slopes
    vmadn vde_f, vdx_f, ish_i;                      sdv vdy_f.e0, 0x38,color_dmem
                                                    # Check if we need to change RDP buffer
    vmadh vde_i, vdx_i, ish_i;                      bnez t0, rdpq_triangle_change_buffer
                                                    sdv vdy_f.e4, 0x38,tex_dmem
rdpq_triangle_change_buffer_done:

    # FINAL = vATTR1 + DE * FY
    # TODO: fy_i is always 0xFFFFFFFF here.
    # See if we can benefit from this.
    # TODO: actually, it can also be 
    # fy_i = fy_f = 0.
    vmudh v__,      vattr1, K1;                     sdv vdy_i.e0, 0x28,color_dmem
    vmadl v__,      vde_f, vfy_f.e4;                sdv vdy_i.e4, 0x28,tex_dmem
                                                    # Store De slopes
    vmadm v__,      vde_i, vfy_f.e4;                sdv vde_f.e0, 0x30,color_dmem
    vmadn vfinal_f, vde_f, vfy_i.e4;                sdv vde_f.e4, 0x30,tex_dmem
    vmadh vfinal_i, vde_i, vfy_i.e4;                sdv vde_i.e0, 0x20,color_dmem

    # Since we need to wait 2 cycles before
    # being able to read vfinal_f contents,
    # use the time to compact the Z values,
    # so that we can use slv to store them later.   
    vmov vdy_f.e6, vdy_i.e7;                        sdv vde_i.e4, 0x20,tex_dmem
                                                    # Store updated RDRAM pointer.
    vmov vde_f.e6, vde_i.e7;                        sw rdram_cur, %lo(RDPQ_CURRENT)
                                                    # Store Z values
    vmov vzout1.e1, vfinal_f.e7;                    sdv vfinal_f.e0, 0x10,color_dmem
    vmov vzout1.e0, vfinal_i.e7;                    sdv vfinal_f.e4, 0x10,tex_dmem
    vmov vzout1.e3, vdx_f.e7;                       sdv vfinal_i.e0, 0x00,color_dmem    
    vmov vzout1.e2, vdx_i.e7;                       sdv vfinal_i.e4, 0x00,tex_dmem
                                                    # Store Z slopes, that were
                                                    # compacted via vmov.
                                                    slv vde_f.e6, 0x08,z_dmem
                                                    slv vdy_f.e6, 0x0C,z_dmem
                                                    addiu prim_size, -1
                                                    sdv vzout1.e0, 0x00,z_dmem

#if RDPQ_TRIANGLE_PROFILE
    emux_trace_stop
    mfc0 t0, COP0_DP_CLOCK
    sub t0, t0, fp
    emux_log_string "RDPQ_Triangle:  "
    emux_dump_gpr t0
#endif

    jr ra
    mtc0 prim_size, COP0_DMA_WRITE


    #undef tricm
    #undef vtx1 
    #undef vtx2 
    #undef vtx3 
    #undef cull 

    #undef y1   
    #undef y2   
    #undef y3   
    #undef x1   
    #undef x2   
    #undef x3   

    # r, g, b, a, s, t, w, z
    #undef vfinal_i         
    #undef vfinal_f         
    #undef vdx_i            
    #undef vdx_f            
    #undef vde_i            
    #undef vde_f            
    #undef vdy_i            
    #undef vdy_f            

    #undef vattr1           
    #undef vattr2           
    #undef vattr3           
    #undef vma              
    #undef vha              

    #undef vinvw_i          
    #undef vinvw_f          
    #undef inz_i
    #undef inz_f

    #undef hx
    #undef hy
    #undef mx
    #undef my
    #undef lx
    #undef ly

    #undef ish_f
    #undef ish_i
    #undef ism_f
    #undef ism_i
    #undef isl_f
    #undef isl_i

    #undef vedges_i         
    #undef vedges_f         
    #undef vnz_i            
    #undef vnz_f            
    #undef vslope_i         
    #undef vslope_f         
    #undef vxy32            
    #undef vxy21            
    #undef vhml             
    #undef vfy_i            
    #undef vfy_f            

    #undef vtmp
    #undef v__         
    #undef invn_i      
    #undef invn_f      
    #undef invsh_i     
    #undef invsh_f     

    #if !RDPQ_TRIANGLE_CUSTOM_VTX
    #undef VTX_ATTR_X   
    #undef VTX_ATTR_Y   
    #undef VTX_ATTR_Z   
    #undef VTX_ATTR_RGBA
    #undef VTX_ATTR_S    
    #undef VTX_ATTR_T    
    #undef VTX_ATTR_W    
    #undef VTX_ATTR_INVWi
    #undef VTX_ATTR_INVWf
    #endif

    #undef STORE_VTX1
    #undef STORE_VTX2
    #undef STORE_VTX3

    # Slow path: when the RDRAM buffer is full (we reached the sentinel),
    # we need to switch buffer. This is basicaly an inlined version of the same
    # slow-path in RDPQ_Send.
rdpq_triangle_change_buffer:
    move a3, ra
    lw rdram_cur, %lo(RDPQ_DYNAMIC_BUFFERS) + 4
    lw t1, %lo(RDPQ_DYNAMIC_BUFFERS) + 0
    sw rdram_cur, %lo(RDPQ_DYNAMIC_BUFFERS) + 0
    sw t1, %lo(RDPQ_DYNAMIC_BUFFERS) + 4

    # We are going to call RSPQCmd_RdpSetBuffer. Since it's going to destroy
    # t0-t3, we need to save prim_size (t3). Notice that we rely on the fact
    # that it doesn't destroy t4-t6 (which contain the various DMEM pointers).
    # As scratch buffer, we use a part of the primitive buffer that will be
    # written later, after we return to the happy-path.
    sb prim_size, 0x00(color_dmem)

    add rdram_sentinel, rdram_cur, RDPQ_DYNAMIC_BUFFER_SIZE
    jal RSPQCmd_RdpSetBuffer
    move a1, rdram_cur
    
    # Go back to RDPQ_Triangle_Send_Async happy path. Restore the primitive
    # size that was stashed away, and change DMA RDRAM registter to use the
    # new buffer instead of the old one.
    mtc0 rdram_cur, COP0_DMA_RAMADDR
    lbu prim_size, 0x00(color_dmem)
    addu rdram_cur, prim_size
    j rdpq_triangle_change_buffer_done
    move ra, a3

    # Slow path: if RSP DMA is still busy by the time we check for it
    # (which is "as late as possible" within the happy-path), we need to
    # wait for it to become idle before continuing.
    # Notice that we only handle one background transfer at the time for
    # sanity, so we can't really check for COP0_DMA_FULL here.
rdpq_triangle_dma_busy:
    bnez t1, rdpq_triangle_dma_busy
    mfc0 t1, COP0_DMA_BUSY
    j rdpq_triangle_dma_idle
    nop

    .endfunc

    #undef rdram_cur
    #undef rdram_sentinel


    #####################################################################
    # RDPQ_Triangle_Send_End
    #
    # Call this function to correctly terminate a batch of subsequent
    # calls to RDPQ_Triangle_Send_Async.
    #
    # RDPQ_Triangle_Send_Async leaves a RSP DMA running for the last
    # triangle, to copy it to RDRAM; moreover the RDP is not informed
    # of it yet. So this function first waits for the DMA to be finished,
    # and then notifies the RDP that the triangle is ready and can be
    # drawn.
    #
    #####################################################################

    .func RDPQ_Triangle_Send_End
RDPQ_Triangle_Send_End:
    mfc0 t0, COP0_DMA_BUSY
    bnez t0, RDPQ_Triangle_Send_End
    nop
    j RSPQCmd_RdpAppendBuffer
    lw a0, %lo(RDPQ_CURRENT)
    .endfunc
