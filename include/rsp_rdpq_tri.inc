#########################################################################
#
#  RDPQ_Triangle: assemble a RDP triangle command
#
##########################################################################

    .section .data.rdpq_triangle
    .align 4
TRICONST1: .half 0,0,0,0,0x200,0x200,0x200,0x200


    .section .text.rdpq_triangle

    #####################################################################
    # RDPQ_Triangle
    #
    # INPUT:
    # * a0: high 32-bit word of the triangle command. This will be
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
    # t0-t5, a0-a3
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

    .func RDPQ_Triangle
RDPQ_Triangle:
    #define tricmd a0
    #define vtx1   a1
    #define vtx2   a2
    #define vtx3   a3
    #define cull   v0
    #define out_clip_flag   v1

    #define y1     t4
    #define y2     t5
    #define y3     t6
    #define x1     t7
    #define x2     t8
    #define x3     v0

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

    #define VTX_ATTR_X      0
    #define VTX_ATTR_Y      2
    #define VTX_ATTR_Z      4
    #define VTX_ATTR_CLIPCODE 6         // Clipping codes
    #define VTX_ATTR_TRCODE   7         // Negated trivial-reject codes
    #define VTX_ATTR_RGBA   8
    #define VTX_ATTR_S      12
    #define VTX_ATTR_T      14
    #define VTX_ATTR_W      16
    #define VTX_ATTR_INVWi  20
    #define VTX_ATTR_INVWf  22

#if 1
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
    #define vhmlupp v__


    #define vk1     $v12
    #define vk2     $v13

    #define vstall
    #define stall

    #define clip1  t3
    #define clip2  t4
    #define clip3  t5
    #define did_swap_0     t0
    #define did_swap_1     t1
    #define did_swap_2     t2


    # vall registers:
    # X1 Y1 Z1 ADDR1  RG1 BA1 S1 T1
    # X2 Y2 Z2 ADDR2  RG2 BA2 S2 T2
    # X3 Y3 Z3 ADDR3  RG3 BA3 S3 T3

    # TODO:
    # Should we do backface culling before clipping check?
    # If so, should we do that BEFORE or AFTER the sort-of-three?

    sh vtx1, 0(s3)
    sh vtx2, 2(s3)
    sh vtx3, 4(s3)
    lqv vall1, VTX_ATTR_X,vtx1
    lqv vall2, VTX_ATTR_X,vtx2
    lqv vall3, VTX_ATTR_X,vtx3
    lhu clip1, VTX_ATTR_CLIPCODE(vtx1); vnop

    vcopy vy1, vall1.e1;                lsv vall1.e3, 0,s3
    vcopy vy2, vall2.e1;                lsv vall2.e3, 2,s3
    vcopy vy3, vall3.e1;                lsv vall3.e3, 4,s3

    # Create vector with constants      
    # 1 and -1 (to be used for
    # cross product)                    # Finish loading clipping codes
    vcopy vk1, K1;                      lhu clip3, VTX_ATTR_CLIPCODE(vtx3)
    vsubc vmconst, vzero, vshift;       lhu clip2, VTX_ATTR_CLIPCODE(vtx2)

    # Do a sort-of-three (min, mid, max), using vyN as key of the sort,
    # and vallN as values to sort. We compare the keys with vlt/vge,
    # and then use vmrg to apply the swap.

    li out_clip_flag, 0;                vge vytmp1, vy1, vy2;               
    or clip1, clip3;                    vmrg valltmp1, vall1, vall2;        
    or clip1, clip2;                    vlt vy1, vy1, vy2;                  
    cfc2 did_swap_0, COP2_CTRL_VCC;     vmrg vall1, vall1, vall2;

    andi t4, clip1, 0xFF;               vcopy vk2, K2
    xori t4, 0xFF;                      vxor vhmlupp, vhmlupp

    beqz t4, JrRa;                      vge vytmp2, vy1, vy3;               
    andi clip1, 0x3F00;                 vmrg valltmp2, vall1, vall3;
    lbu t4, %lo(RDPQ_OTHER_MODES) + 0;  vlt vy1, vy1, vy3;                  
    cfc2 did_swap_1, COP2_CTRL_VCC;     vmrg vall1, vall1, vall3;           

    nop;                                vge vy3, vytmp1, vytmp2;            
    andi t4, 0x38;                      vmrg vall3, valltmp1, valltmp2;     
    or tricmd, t4;                      vlt vy2, vytmp1, vytmp2;            
    cfc2 did_swap_2, COP2_CTRL_VCC;     vmrg vall2, valltmp1, valltmp2;     

    # Copy Y1, Y2 into vy3, so that we get the three sorted Y in the same
    # vector.
    #  vy3:   Y1 Y2 Y3 Y3  Y3 Y3 Y3 Y3 

    # Build vhml:
    #    vhml      =   HX HY MX MY    LX LY  0  NZf

    vmov vy3.e3, vy1.e0;                xor did_swap_0, did_swap_1
    vsubc vhml, vall3, vall1;           nop
    vmov vy3.e2, vy2.e0;                xor did_swap_0, did_swap_2
    vsubc vm, vall2, vall1;             mfc2 vtx1, vall1.e3
    vsubc vl, vall3, vall2;             mfc2 vtx2, vall2.e3

    # vx12 = X1 -- X2 --  -- -- -- --
    vmov vx12_f.e0, vall1.e0;           xor cull, did_swap_0
    vmov vx12_f.e2, vall2.e0;           andi did_swap_0, 1

    # Prepare -MY for crossprod
    vsubc vtmp, vzero, my;              mfc2 vtx3, vall3.e3
    
    # FY.e4 = Y1 (s15.16)
    vsll8 vfy_f, vy1, 14;               sdv vy3.e0, 0,s3
    vsra  vfy_i, vy1, 2;                bnez clip1, JrRa
                                        li out_clip_flag, 1     # report clipping required
    vmudh vnz_f, vm, hy;                li out_clip_flag, 0     # report no clipping required
    vmadh vnz_f, vhml, vtmp;            slv vm.e0,   8,s3
    vsar vnz_i, COP2_ACC_HI;            slv vl.e0,  12,s3
    vsar vnz_f, COP2_ACC_MD;            ldv vhml.e2, 8,s3

    # Convert X1 and X2 into
    # 16.16 precision
    # vx12 = X1 -- X2 --  -- -- -- --   # Read 32-bit W into vw_i (e0/e2/e4)
    vsra  vx12_i, vx12_f, 2;                       
    vsll8 vx12_f, vx12_f, 14;           llv vw_i.e0, VTX_ATTR_W,vtx1

    # FY.e0 = floorf(y1) - y1
    # TODO: this is always a negative
    # fraction, so fy_i is always 0xFFFF
    # (or fy_i=fy_f=0). See if we can
    # take advantage of this somehow
    # to simplify later.                
                                        # Extract sign of Z
    vsubc vfy_f, vzero, vfy_f;          mfc2 t0, vnz_i.e0
    vsub  vfy_i, vfy_i;                 llv vw_i.e2, VTX_ATTR_W,vtx2

    # Compute SLOPE vector
    # slope    =  1/HY -- 1/MY --   1/LY -- -- 1/NZ

    # Calculate reciprocal of normal    # Backface culling
    vrcph vslope_i.e7, vnz_i.e0;        stall
    vrcpl vslope_f.e7, vnz_f.e0;        slt t0, t0, zero 
    vrcph vslope_i.e7, vzero.e0;        xori t0, 1
                                        beq t0, cull, JrRa
    # Compute ISH (H slope). 1/HY  (s14.1)
    vrcp  vslope_f.e0, hy;              # DELAY SLOT
                                        
    vrcph vslope_i.e0, hy;              luv attr1_r, VTX_ATTR_RGBA,vtx1
    # Compute ISM (M slope). 1/MY  (s14.1)
    vrcp  vslope_f.e2, my;              luv attr2_r, VTX_ATTR_RGBA,vtx2
    vrcph vslope_i.e2, my;              luv attr3_r, VTX_ATTR_RGBA,vtx3
    # Compute ISL (L slope). 1/LY  (s14.1)      
    vrcp  vslope_f.e4, ly;              llv vw_i.e4, VTX_ATTR_W,vtx3
    vrcph vslope_i.e4, ly;              

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
    vmov vhml.e7, vnz_f.e0;                     sll t0, 7
    vmov vhmlupp.e7, vnz_i.e0;                  or tricmd, t0

    # vslopem2  = X/2 * (-4) = -2*X             
    vmudn vslopem2_f, vslope_f, VKM4;           
    vmadh vslopem2_i, vslope_i, VKM4;           
            
    # Split 32-bit W into two 16-bit halves
    vcopy vw_f, vw_i.q1;                        

    # NR: R*(X/2)                               
    vmudl vtmp_f, vslope_f, vhml.q1;            
    vmadm vtmp_f, vslope_i, vhml.q1;            # Store X2 value in output (as XL)
    vmadn vtmp_f, vslope_f, vhmlupp;            ssv vx12_i.e2,  8,s3  # XL_I
    vmadh vtmp_i, vslope_i, vhmlupp;            ssv vx12_f.e2, 10,s3  # XL_F

    # NR: R*X/2 - 1                             # Calculate -MY/-HX
    vmadn vtmp_f, vk1, VKM1;                    mfc2 t3, my
    vmadh vtmp_i, vk1, VKM1;                    mfc2 t4, hx

    # Calculate minimum W (between e0 and e2)   # Load invw components
    vsubc v__,  vw_f, vw_f.e2;                  lsv vinvw_i.e4, VTX_ATTR_INVWi,vtx1
    vlt   vw_i, vw_i, vw_i.e2;                  lsv vinvw_i.e5, VTX_ATTR_INVWi,vtx2
    vmrg  vw_f, vw_f, vw_f.e2;                  lsv vinvw_i.e6, VTX_ATTR_INVWi,vtx3

    # NR: -2*X * (R*X/2 - 1)
    #   =  2*X * (1 - R*X/2)
    #   =    X * (2 - R*X)                      # Load S compoenents
    vmudl v__,      vtmp_f, vslopem2_f;         llv attr1_s, VTX_ATTR_S,vtx1  # S & T
    vmadm v__,      vtmp_i, vslopem2_f;         llv attr2_s, VTX_ATTR_S,vtx2
    vmadn vnz_f,    vtmp_f, vslopem2_i;         llv attr3_s, VTX_ATTR_S,vtx3
    vmadh vnz_i,    vtmp_i, vslopem2_i;

    # Calculate minimum W (between e0 and e4)
    # Now vw_if.e0 contains the minimum W       # Load other invw components
    vsubc v__,  vw_f, vw_f.e4;                  lsv vinvw_f.e4, VTX_ATTR_INVWf,vtx1
    vlt   vw_i, vw_i, vw_i.e4;                  lsv vinvw_f.e5, VTX_ATTR_INVWf,vtx2
    vmrg  vw_f, vw_f, vw_f.e4;                  lsv vinvw_f.e6, VTX_ATTR_INVWf,vtx3

    # Finalize slope divisions by multiplying by the reciprocal.
    # vhml      =   HX  HY   MX  MY     LX   LY   0   NZf
    #  *
    # vnz       =  1/HY --  1/MY --    1/LY --   --- 1/NZ
    #    =
    # vslope    =  HX/HY --   MX/MY  --   LX/LY  --     --   --
                                                # Store negated my/hx into vhml
    vmudn v__,  vnz_f, vhml;                    neg t3
    vmadh v__,  vnz_i, vhml;                    neg t4
    vsar  vslope_f, COP2_ACC_MD;                mtc2 t3, my
    vsar  vslope_i, COP2_ACC_HI;                mtc2 t4, hx
    #define ish_f   vslope_f.e0
    #define ish_i   vslope_i.e0
    #define ism_f   vslope_f.e2
    #define ism_i   vslope_i.e2
    #define isl_f   vslope_f.e4
    #define isl_i   vslope_i.e4
                                                # Prepare for vmrg later (to merge
    # 32bit vsra by 1                           # tex coords inside the attribute
    vmudl vw_f, vw_f, K32768;                   
    vmadm vw_i, vw_i, K32768;                   li t0, 0xCF
    vmadn vw_f, vzero, vzero;                   ctc2 t0, COP2_CTRL_VCC

    #define vtmp1 vdx_f
    #define vtmp2 vdx_i
    #define vtmp3 vde_f

    vsrl vattr1, vattr1, 7;                     llv vtmp1.e4, VTX_ATTR_S,vtx1                       
    vsrl vattr2, vattr2, 7;                     llv vtmp2.e4, VTX_ATTR_S,vtx2                   
    vsrl vattr3, vattr3, 7;                     llv vtmp3.e4, VTX_ATTR_S,vtx3                   

    # We need to normalize INV_W in [0..1], by dividing them by the maximum INV_W.
    # We will multiply by W instead. We have now prepared the minium w in vw_i.e0.
    #
    #    vw:     minw -- -- --    ----- ----- ----- --
    # vinvw:     ---- -- -- --    invw1 invw2 invw3 --
    #
    # We need to multiply minw with the three invw. All numbers are positive s16.16,
    # and the result is known to fit 0..1. By doing a standard 32-bit multiplication
    # on RSP, we end up with a positive s16.16 number, where the integer word is zero.
    # In fact, in theory W * 1/W = 1, but both numbers are likely missing enough bits
    # of precision that the result will always be slightly lower than 1 (and thus the
    # integer part will be 0).
    vmudl v__,     vinvw_f, vw_f.e0;            lsv attr1_z, VTX_ATTR_Z,vtx1
    vmadm v__,     vinvw_i, vw_f.e0;            lsv attr2_z, VTX_ATTR_Z,vtx2 
    vmadn vinvw_f, vinvw_f, vw_i.e0;            lsv attr3_z, VTX_ATTR_Z,vtx3

    #  vxy21 =   X1   --    X2   --       Y1 --   Y2 --
    #  slope = HX/HY  --   MX/MY --     LX/LY --  -- --

    # Compute XH and XM
    # TODO: fy_i is always 0xFFFFFFFF here.
    # See if we can benefit from this.    
    vmudl v__,      vslope_f, vfy_f.e4;         
    vmadm v__,      vslope_i, vfy_f.e4;         
    vmadn vedges_f, vslope_f, vfy_i.e4;         ssv ism_f, 30,s3
    vmadh vedges_i, vslope_i, vfy_i.e4;         ssv ism_i, 28,s3

    vmulf vtmp1, vinvw_f.h0;                    sdv vinvw_f.e4, 16,s3
    vmulf vtmp2, vinvw_f.h1;                    lsv attr1_invw, 16,s3
    vmulf vtmp3, vinvw_f.h2;                    lsv attr2_invw, 18,s3

    vaddc vedges_f, vx12_f.e0;                  lsv attr3_invw, 20,s3
    vadd  vedges_i, vx12_i.e0;

    vmrg vattr1, vtmp1;                         
    vmrg vattr2, vtmp2;                         ssv isl_f, 14,s3
    vmrg vattr3, vtmp3;                         ssv isl_i, 12,s3

    ########################################################
    # ATTRIBUTES
    ########################################################

    #define color_dmem     t3
    #define tex_dmem       t4
    #define z_dmem         t5

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
    # Note also that we need
    # "vsubc zero,zero" to clear the VCC
    # (carry) bit which vsub reads as input.            
                                                    # Store tricmd and last coord slopes
    vsubc vzero, vzero;                             ssv ish_f, 22,s3
    vsub vma, vattr2, vattr1;                       ssv ish_i, 20,s3
    vsub vha, vattr3, vattr1;                       sh tricmd, 0(s3)

    # Shift left NZ (that contains INVNZ)
    # by 2, to align with the fixed point
    # precision that will be required later.
    vmudn vnz_f, K4;                  
    vmadh vnz_i, K4;                  
    #define inz_f   vnz_f.e7
    #define inz_i   vnz_i.e7
               
    # DX = MA * HY + HA * -MY
    vmudh vdx_f, vma, hy;                           ssv vedges_i.e2, 24,s3  # XM_I
    vmadh vdx_f, vha, my;                           ssv vedges_f.e2, 26,s3  # XM_F
    vsar vdx_f, COP2_ACC_MD;                        ssv vedges_i.e0, 16,s3  # XH_I
    vsar vdx_i, COP2_ACC_HI;                        ssv vedges_f.e0, 18,s3  # XH_F

    # DY = HA * MX + MA * -HX                       # Calculate pointers to color, tex
                                                    # and z attributes, based on tricmd
    vmudh vdy_f, vha, mx;                           addu color_dmem, s3, 0x20
    vmadh vdy_f, vma, hx;                           andi t0, tricmd, 0x400
    vsar vdy_f, COP2_ACC_MD;                        srl t0, 4
    vsar vdy_i, COP2_ACC_HI;                        addu tex_dmem, color_dmem, t0

    # DX * 1/N
    vmudl v__,  vdx_f, inz_f;                       andi t0, tricmd, 0x200
    vmadm v__,  vdx_i, inz_f;                       srl t0, 3
    vmadn vdx_f, vdx_f, inz_i;                      addu z_dmem, tex_dmem, t0
    vmadh vdx_i, vdx_i, inz_i;                      andi t0, tricmd, 0x100

    # DY * 1/N
    vmudl v__,  vdy_f, inz_f;                       srl t0, 4
    vmadm v__,  vdy_i, inz_f;                       addu s3, z_dmem, t0
                                                    # Store Dx slopes
    vmadn vdy_f, vdy_f, inz_i;                      sdv vdx_f.e0, 0x18,color_dmem
    vmadh vdy_i, vdy_i, inz_i;                      sdv vdx_f.e4, 0x18,tex_dmem

    # DE = DX * invsh + DY
    vmadl v__,  vdx_f, ish_f;                       sdv vdx_i.e0, 0x08,color_dmem
    vmadm v__,  vdx_i, ish_f;                       sdv vdx_i.e4, 0x08,tex_dmem
                                                    # Store Dy slopes
    vmadn vde_f, vdx_f, ish_i;                      sdv vdy_f.e0, 0x38,color_dmem
    vmadh vde_i, vdx_i, ish_i;                      sdv vdy_f.e4, 0x38,tex_dmem
    vnop;                                           sdv vdy_i.e0, 0x28,color_dmem

    # FINAL = vATTR1 + DE * FY
    # TODO: fy_i is always 0xFFFFFFFF here.
    # See if we can benefit from this.
    # TODO: actually, it can also be 
    # fy_i = fy_f = 0.
    vmudh v__,      vattr1, K1;                     sdv vdy_i.e4, 0x28,tex_dmem
                                                    # Store De slopes
    vmadl v__,      vde_f, vfy_f.e4;                sdv vde_f.e0, 0x30,color_dmem
    vmadm v__,      vde_i, vfy_f.e4;                sdv vde_f.e4, 0x30,tex_dmem
    vmadn vfinal_f, vde_f, vfy_i.e4;                sdv vde_i.e0, 0x20,color_dmem
    vmadh vfinal_i, vde_i, vfy_i.e4;                sdv vde_i.e4, 0x20,tex_dmem

    # Since we need to wait 3 cycles before
    # being able to store vfinal_f, use the time
    # to compact the Z values, so that we can
    # use slv to store them later.
    vmov vdy_f.e6, vdy_i.e7;                        ssv vdx_f.e7, 0x06,z_dmem
    vmov vde_f.e6, vde_i.e7;                        ssv vdx_i.e7, 0x04,z_dmem
    vmov vdx_f.e7, vfinal_f.e7;                     sdv vfinal_f.e0, 0x10,color_dmem
    vmov vdx_f.e6, vfinal_i.e7;                     sdv vfinal_f.e4, 0x10,tex_dmem
                                                    sdv vfinal_i.e0, 0x00,color_dmem    
                                                    sdv vfinal_i.e4, 0x00,tex_dmem
                                                    # Store Z slopes, that were
                                                    # compacted via vmov.
                                                    slv vde_f.e6, 0x08,z_dmem
                                                    slv vdy_f.e6, 0x0C,z_dmem
                                                    slv vdx_f.e6, 0x00,z_dmem
                                                    


    emux_trace_stop

#if 1
    mfc0 t0, COP0_DP_CLOCK
    sub t0, t0, fp
    emux_log_string "RDPQ_Triangle:  "
    emux_dump_gpr t0
#endif

    jr ra
    nop

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

    #undef VTX_ATTR_X   
    #undef VTX_ATTR_Y   
    #undef VTX_ATTR_Z   
    #undef VTX_ATTR_RGBA
    #undef VTX_ATTR_S    
    #undef VTX_ATTR_T    
    #undef VTX_ATTR_W    
    #undef VTX_ATTR_INVWi
    #undef VTX_ATTR_INVWf

    .endfunc



