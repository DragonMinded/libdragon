#include <rsp_magma.inc>
#include <mgfx_constants.h>

#define MGFX_TRACE_VTX_LOOP 0

MgBeginShaderUniforms
    MgBeginUniform FOG, MGFX_BINDING_FOG
        FOG_FACTOR_INT:     .half   0
        FOG_OFFSET_INT:     .half   0
        FOG_FACTOR_FRAC:    .half   0
        FOG_OFFSET_FRAC:    .half   0
    MgEndUniform

    MgBeginUniform LIGHTING, MGFX_BINDING_LIGHTING
        LIGHTING_LIGHTS:    .dcb.b  MGFX_LIGHT_SIZE * MGFX_LIGHT_COUNT_MAX
        LIGHTING_AMBIENT:   .dcb.w  4
        LIGHTING_COUNT:     .word   0
    MgEndUniform

    MgBeginUniform TEXTURING, MGFX_BINDING_TEXTURING
        TEXTURING_SCALE:    .dcb.w  2
        TEXTURING_OFFSET:   .dcb.w  2
    MgEndUniform

    MgBeginUniform MODES, MGFX_BINDING_MODES
        MODES_FLAGS:        .word   0
    MgEndUniform

    MgBeginUniform MATRICES, MGFX_BINDING_MATRICES
        MATRICES_MVP:       .dcb.b  MGFX_MATRIX_SIZE
        MATRICES_MV:        .dcb.b  MGFX_MATRIX_SIZE
        MATRICES_NORMAL:    .dcb.b  MGFX_MATRIX_SIZE
    MgEndUniform
MgEndShaderUniforms

MgBeginVertexInput
    MgBeginVertexAttribute MGFX_ATTRIBUTE_POS_NORM
        MgVertexAttributeLoaders LOAD_POS_NORM0, LOAD_POS_NORM1, LOAD_POS_NORM2, LOAD_POS_NORM3, LOAD_POS_NORM4, LOAD_POS_NORM5, LOAD_POS_NORM6, LOAD_POS_NORM7
    MgEndVertexAttribute 

    MgBeginVertexAttribute MGFX_ATTRIBUTE_COLOR, 1
        MgVertexAttributeLoaders LOAD_COLOR0, LOAD_COLOR1
        MgBeginVertexAttributePatch PATCH_COLOR0
            vnop
        MgEndVertexAttributePatch
    MgEndVertexAttribute 

    MgBeginVertexAttribute MGFX_ATTRIBUTE_TEXCOORD, 1
        MgVertexAttributeLoaders LOAD_TEXCOORD0, LOAD_TEXCOORD1
    MgEndVertexAttribute 
MgEndVertexInput

MgBeginShader
    #define flags           v0
    #define vtx_size        k0
    #define vtx_size2       k1
    #define vtx_in0_ptr     s1
    #define vtx_in1_ptr     s5
    #define vtx_in_end      s2
    #define vtx_out_ptr     s3
    #define v___            $v29

    li t0, %lo(MG_VERTEX_SIZE)
    addi t1, rspq_dmem_buf_ptr, %lo(RSPQ_DMEM_BUFFER) - 6

    #define vvtx_size       $v01
    #define vinputs         $v02
    #define vproducts       $v03
    #define vbuf_offset_hi  $v04
    #define vbuf_offset_lo  $v05

    # vertex_size, MG_VTX_SIZE, -vertex_size, vertex_size
    ldv vvtx_size, 0,t0

    # buffer_offset, cache_offset, vertex_count, vertex_count
    ldv vinputs.e0, 0,t1
    lsv vinputs.e3, 4,t1

    lhu vtx_size, %lo(MG_VERTEX_SIZE)
    lw s0, %lo(MG_VERTEX_BUFFER)
    li t5, %lo(MG_NORMAL_MASK)

    #define vnormmask       $v28
    #define vnormfactor     $v27

    # buffer_offset * vertex_size, cache_offset * MG_VTX_SIZE, vertex_count * -vertex_size, vertex_count * vertex_size
    ldv vnormmask.e0,   0x0,t5;              vmudh vproducts, vinputs, vvtx_size
    ldv vnormmask.e4,   0x0,t5;              vsar vbuf_offset_hi, COP2_ACC_HI
    ldv vnormfactor.e0, 0x8,t5;              vsar vbuf_offset_lo, COP2_ACC_MD
    ldv vnormfactor.e4, 0x8,t5

    sll vtx_size2, vtx_size, 1
    li t4, %lo(RSPQ_SCRATCH_MEM)
    sdv vproducts, 0,t4
    ssv vbuf_offset_hi.e0, 8,t4
    ssv vbuf_offset_lo.e0, 10,t4

    #undef vvtx_size
    #undef vinputs
    #undef vproducts
    #undef vbuf_offset_hi
    #undef vbuf_offset_lo

    lw  t0,          8(t4)
    lhu vtx_out_ptr, 2(t4)
    lh  s4,          4(t4)
    lhu t1,          6(t4)

    add s0, t0

    # Compensate for misalignment in RDRAM pointer
    andi t0, 0x7
    add t0, t1

    # Round up to next multiple of 8
    addi t0, 0x7
    andi t0, 0xFF8
    addi t0, -1

    addi vtx_out_ptr, %lo(MG_VERTEX_CACHE)

    addi s4, %lo(MG_VERTEX_OVERFLOW)
    
    jal DMAIn
    andi s4, 0xFF8
    
    move vtx_in0_ptr, s4
    add vtx_in_end, vtx_in0_ptr, t1
    add vtx_in1_ptr, vtx_in0_ptr, vtx_size

    #define vpos_eye        $v26
    #define vnorm           $v25
    #define vrgba_in        $v24
    #define vtex            $v23
    #define vpos_clip_f     $v22
    #define vpos_clip_i     $v21
    #define vpos_in         $v19

    LOAD_POS_NORM0: ldv vpos_in.e0, 0,vtx_in0_ptr
    LOAD_POS_NORM1: ldv vpos_in.e4, 0,vtx_in1_ptr
    LOAD_POS_NORM2: ldv vnorm.e0, 0,vtx_in0_ptr
    LOAD_POS_NORM3: ldv vnorm.e4, 0,vtx_in1_ptr
    
    lsv vpos_in.e3, 0xA,t5;                                             
    lsv vpos_in.e7, 0xA,t5;                                             

    #define vmvp0_i     $v03
    #define vmvp0_f     $v04
    #define vmvp1_i     $v05
    #define vmvp1_f     $v06
    #define vmvp2_i     $v07
    #define vmvp2_f     $v08
    #define vmvp3_i     $v09
    #define vmvp3_f     $v10

    #define vmmv0_i     $v11
    #define vmmv0_f     $v12
    #define vmmv1_i     $v13
    #define vmmv1_f     $v14

    li t0, %lo(MATRICES)
    ldv vmvp0_f.e0, 0x20,t0;
    ldv vmvp0_f.e4, 0x20,t0;
    ldv vmvp0_i.e0, 0x00,t0;
    ldv vmvp0_i.e4, 0x00,t0;
    ldv vmvp1_f.e0, 0x28,t0;
    ldv vmvp1_f.e4, 0x28,t0;
    ldv vmvp1_i.e0, 0x08,t0;
    ldv vmvp1_i.e4, 0x08,t0;
    ldv vmvp2_f.e0, 0x30,t0;
    ldv vmvp2_f.e4, 0x30,t0;
    ldv vmvp2_i.e0, 0x10,t0;
    ldv vmvp2_i.e4, 0x10,t0;
    ldv vmvp3_f.e0, 0x38,t0;
    ldv vmvp3_f.e4, 0x38,t0;
    ldv vmvp3_i.e0, 0x18,t0;    
    ldv vmvp3_i.e4, 0x18,t0;    
    ldv vmmv0_f.e0, 0x60,t0;                                            
    ldv vmmv0_f.e4, 0x60,t0;                                            
    ldv vmmv0_i.e0, 0x40,t0;                                            
    ldv vmmv0_i.e4, 0x40,t0;                                            
    ldv vmmv1_f.e0, 0x68,t0;                                            
    ldv vmmv1_f.e4, 0x68,t0;                                            
    ldv vmmv1_i.e0, 0x48,t0;                                            
    ldv vmmv1_i.e4, 0x48,t0;                                            

    #define tmp_out     t6
    li tmp_out, %lo(MG_CLIP_CACHE)

#if MGFX_TRACE_VTX_LOOP
    emux_trace_start
#endif

    .align 3
vertex_loop:
    
    #define vmmv2_i     $v15
    #define vmmv2_f     $v16
    #define vmmv3_i     $v17
    #define vmmv3_f     $v18

    #define vmn0_i      $v02
    #define vmn0_f      $v03
    #define vmn1_i      $v04
    #define vmn1_f      $v05
    #define vmn2_i      $v06
    #define vmn2_f      $v07

    #define vinvd_f      $v20
    #define vtmp         $v26

    ldv vmmv2_f.e0, 0x70,t0;                                            vand vtmp, vnormmask, vnorm.h3
    ldv vmmv2_f.e4, 0x70,t0;                                            vmudn v___,        vmvp0_f, vpos_in.h0
    ldv vmmv2_i.e0, 0x50,t0;                                            vmadh v___,        vmvp0_i, vpos_in.h0
    ldv vmmv2_i.e4, 0x50,t0;                                            vmadn v___,        vmvp1_f, vpos_in.h1
    ldv vmmv3_f.e0, 0x78,t0;                                            vmadh v___,        vmvp1_i, vpos_in.h1
    ldv vmmv3_f.e4, 0x78,t0;                                            vmadn v___,        vmvp2_f, vpos_in.h2
    ldv vmmv3_i.e0, 0x58,t0;                                            vmadh v___,        vmvp2_i, vpos_in.h2
    ldv vmmv3_i.e4, 0x58,t0;                                            vmadn vpos_clip_f, vmvp3_f, vpos_in.h3
    ldv vmn0_f.e0, 0xA0,t0;                                             vmadh vpos_clip_i, vmvp3_i, vpos_in.h3
    #undef vmvp0_i
    #undef vmvp0_f
    #undef vmvp1_i
    #undef vmvp1_f
    #undef vmvp2_i
    #undef vmvp2_f
    #undef vmvp3_i
    #undef vmvp3_f

    ldv vmn0_f.e4, 0xA0,t0;                                             vmudn vnorm, vtmp, vnormfactor
    ldv vmn0_i.e0, 0x80,t0;                                             vmudn v___,     vmmv0_f, vpos_in.h0
    ldv vmn0_i.e4, 0x80,t0;                                             vmadh v___,     vmmv0_i, vpos_in.h0
    ldv vmn1_f.e0, 0xA8,t0;                                             vmadn v___,     vmmv1_f, vpos_in.h1
    ldv vmn1_f.e4, 0xA8,t0;                                             vmadh v___,     vmmv1_i, vpos_in.h1
    ldv vmn1_i.e0, 0x88,t0;                                             vmadn v___,     vmmv2_f, vpos_in.h2
    ldv vmn1_i.e4, 0x88,t0;                                             vmadh v___,     vmmv2_i, vpos_in.h2
    ldv vmn2_f.e0, 0xB0,t0;                                             vmadn v___,     vmmv3_f, vpos_in.h3
    ldv vmn2_f.e4, 0xB0,t0;                                             vmadh vpos_eye, vmmv3_i, vpos_in.h3
    #undef vmmv0_i
    #undef vmmv0_f
    #undef vmmv1_i
    #undef vmmv1_f
    #undef vmmv2_i
    #undef vmmv2_f
    #undef vmmv3_i
    #undef vmmv3_f                 
    #undef vpos_in
    #undef vtmp
    #undef vnormmask
    #undef vnormfactor

    #define vrgba           $v01
    #define vlpos           $v02
    #define vviewoff        $v03
    #define vviewscale      $v04
    #define vclip_factors   $v11

    ldv vmn2_i.e0, 0x90,t0;                                             vmudn v___,  vmn0_f, vnorm.h0
    ldv vmn2_i.e4, 0x90,t0;                                             vmadh v___,  vmn0_i, vnorm.h0
    li t0, %lo(MG_STATE);                                               vmadn v___,  vmn1_f, vnorm.h1
    ldv vclip_factors.e0, 0x10,t0;                                      vmadh v___,  vmn1_i, vnorm.h1
    ldv vclip_factors.e4, 0x10,t0;                                      vmadn v___,  vmn2_f, vnorm.h2
    ldv vviewscale.e0, 0x0,t0;                                          vmadh vnorm, vmn2_i, vnorm.h2
    ldv vviewscale.e4, 0x0,t0;                                          vxor vinvd_f, vinvd_f
    
    #undef vmn0_i
    #undef vmn0_f
    #undef vmn1_i
    #undef vmn1_f
    #undef vmn2_i
    #undef vmn2_f

    #define vfog_i          $v05
    #define vfog_f          $v06

    #define vinvdist_i      $v07
    #define vinvdist_f      $v08
    #define vsqdist_i       $v09
    #define vsqdist_f       $v10
    #define vguard_i        $v12
    #define vguard_f        $v13


    #define vfogtmp         $v17

    #define tr1             a0
    #define tr2             a1
    #define clip1           a2
    #define clip2           a3
    #define light_count     t9

    ldv vviewoff.e0,   0x8,t0;                                          vsubc vfogtmp, vzero, vpos_eye.h2
    ldv vviewoff.e4,   0x8,t0;                                          vnop # <stall>
    LOAD_COLOR0: llv vrgba_in.e0, 0, vtx_in0_ptr;                       vmudh v___, vnorm, vnorm
    LOAD_COLOR1: llv vrgba_in.e2, 0, vtx_in1_ptr;                       vsar  vsqdist_f, COP2_ACC_MD
    li t1, %lo(FOG);                                                    vsar  vsqdist_i, COP2_ACC_HI
    li s0, %lo(LIGHTING_LIGHTS);                                        vmudm vpos_clip_i, vpos_clip_i, vshift8.e4
    suv vrgba.e0, MG_VTX_RGBA + 0,          tmp_out;                    vmadl vpos_clip_f, vpos_clip_f, vshift8.e4
    suv vrgba.e4, MG_VTX_RGBA + MG_VTX_SIZE,tmp_out;                    vaddc vinvdist_f, vsqdist_f, vsqdist_f.h1
    slv vtex.e0, MG_VTX_ST + 0,          tmp_out;                       vadd  vinvdist_i, vsqdist_i, vsqdist_i.h1
    slv vtex.e4, MG_VTX_ST + MG_VTX_SIZE,tmp_out;                       vch v___, vpos_clip_i, vpos_clip_i.h3
    lw light_count, %lo(LIGHTING_COUNT);                                vcl v___, vpos_clip_f, vpos_clip_f.h3
    cfc2 t0, COP2_CTRL_VCC;                                             vaddc vsqdist_f, vinvdist_f, vsqdist_f.h2
    llv vfog_i.e0, 0,t1;                                                vadd  vsqdist_i, vinvdist_i, vsqdist_i.h2
    llv vfog_i.e4, 0,t1;                                                vmudn vguard_f, vpos_clip_f, vclip_factors
    llv vfog_f.e0, 4,t1;                                                vmadh vguard_i, vpos_clip_i, vclip_factors
    llv vfog_f.e4, 4,t1;                                                vge vfogtmp, vpos_eye.h2
    lw flags, %lo(MODES_FLAGS);                                         vrsqh v___.e0,       vsqdist_i.e0
    srl t2, t0, 4;                                                      vrsql vinvdist_f.e0, vsqdist_f.e0
    andi t0, 0x707;                                                     vrsqh vinvdist_i.e0, vsqdist_i.e4
    andi t2, 0x707;                                                     vrsql vinvdist_f.e4, vsqdist_f.e4
    srl t1, t0, 5;                                                      vrsqh vinvdist_i.e4, vzero.e0
    srl t3, t2, 5;                                                      vsubc vfogtmp, vfog_i.h1
    nor tr1, t0, t1;                                                    vch v___, vguard_i, vguard_i.h3
    nor tr2, t2, t3;                                                    vcl v___, vguard_f, vguard_f.h3
    cfc2 t0, COP2_CTRL_VCC;                                             vmudm v___,  vnorm, vinvdist_f.h0
    sdv vrgba_in.e0, 0,vtx_out_ptr;                                     vmadh vnorm, vnorm, vinvdist_i.h0
                                                                        
    #undef vinvdist_i
    #undef vinvdist_f
    #undef vsqdist_i
    #undef vsqdist_f
    #undef vclip_factors
    #undef vguard_i
    #undef vguard_f

    #define vviewscale_i    $v07
    #define vviewscale_f    $v08
    #define vinvw_i         $v09
    #define vinvw_f         $v10
    #define vnormw_i        $v11
    #define vnormw_f        $v12

    luv vrgba_in.e0, 0,vtx_out_ptr;                                     vmudl v___,     vpos_clip_f, vviewscale.h3
    ldv vrgba.e0, MGFX_LIGHT_SIZE * MGFX_LIGHT_COUNT_MAX,s0;            vmadm vnormw_i, vpos_clip_i, vviewscale.h3
    ldv vrgba.e4, MGFX_LIGHT_SIZE * MGFX_LIGHT_COUNT_MAX,s0;            vmadn vnormw_f, vzero, vzero
    ldv vlpos.e0, MGFX_LIGHT_POSITION,s0;                               vmudm vviewscale_i, vviewscale, vviewscale.h3
    ldv vlpos.e4, MGFX_LIGHT_POSITION,s0;                               vmadn vviewscale_f, vzero, vzero
    LOAD_TEXCOORD0: llv vtex.e0, 0,vtx_in0_ptr;                         vrcph v___.e3,    vnormw_i.e3
    LOAD_TEXCOORD1: llv vtex.e4, 0,vtx_in1_ptr;                         vrcpl vinvw_f.e3, vnormw_f.e3
    srl t2, t0, 4;                                                      vrcph vinvw_i.e3, vnormw_i.e7
    andi t0, 0x707;                                                     vrcpl vinvw_f.e7, vnormw_f.e7
    andi t2, 0x707;                                                     vrcph vinvw_i.e7, vzero.e0
    srl t1, t0, 5;                                                      vmudm v___,    vfogtmp, vfog_f.h0
    srl t3, t2, 5;                                                      vmadh vfogtmp, vfogtmp, vfog_i.h0

    #undef vviewscale
    #undef vfog_i
    #undef vfog_f

    #define vpos_scr_i      $v04
    #define vpos_scr_f      $v05

    or clip1, t0, t1;                                                   vmudl v___,       vpos_clip_f, vinvw_f.h3
    or clip2, t2, t3;                                                   vmadm v___,       vpos_clip_i, vinvw_f.h3
    sdv vpos_clip_i.e0,  MG_VTX_CS_POSi + 0,vtx_out_ptr;                vmadn vpos_scr_f, vpos_clip_f, vinvw_i.h3
    sdv vpos_clip_f.e0,  MG_VTX_CS_POSf + 0,vtx_out_ptr;                vmadh vpos_scr_i, vpos_clip_i, vinvw_i.h3
    sdv vpos_clip_i.e4,  MG_VTX_CS_POSi + MG_VTX_SIZE,vtx_out_ptr;      vge   vfogtmp, vzero
    sdv vpos_clip_f.e4,  MG_VTX_CS_POSf + MG_VTX_SIZE,vtx_out_ptr;      vmudh v___,       vviewoff, K1
    ssv vnormw_f.e3, MG_VTX_Wf + 0,          vtx_out_ptr;               vmadl v___,       vpos_scr_f, vviewscale_f
    ssv vnormw_i.e3, MG_VTX_Wi + 0,          vtx_out_ptr;               vmadm v___,       vpos_scr_i, vviewscale_f
    ssv vnormw_i.e7, MG_VTX_Wi + MG_VTX_SIZE,vtx_out_ptr;               vmadn vpos_scr_f, vpos_scr_f, vviewscale_i
    move tmp_out, vtx_out_ptr;                                          vmadh vpos_scr_i, vpos_scr_i, vviewscale_i                                                 

    
    #undef vnormw_i
    #undef vviewscale_i
    #undef vviewscale_f
    #undef vviewoff

    #undef vpos_clip_i
    #undef vpos_clip_f
    
    #define env_enabled     t7
    #define fog_enabled     t8

    .align 3
light_loop:

    #define scratch_ptr     t1
    #define is_point        t2

    # Light is positional: We need to compute light vector, normalize it, and apply attenuation

    # Load attenuation coefficients k0, k1, k2 (constant, linear, quadratic)
    # vattenuation: k0  k1  k2  --  k0  k1  k2  --

    # If light is positional, the light vector points from the vertex to the light position
    # This is shifted left by 5 because both values are in s10.5 format
                                                                        
    #define vdiff        $v03
    li scratch_ptr, %lo(RSPQ_SCRATCH_MEM);                              vsubc vdiff, vlpos, vpos_eye

    #define vtsqdist_i   $v06
    #define vtsqdist_f   $v07
    li t0, 0x7FFF;                                                      vmudh v___, vpos_eye, vpos_eye
    sh t0, 4(scratch_ptr);                                              vsar  vtsqdist_f, COP2_ACC_MD
    sdv vpos_scr_i.e0,  MG_VTX_XYZ + 0,tmp_out;                         vsar  vtsqdist_i, COP2_ACC_HI
    
    # Dot product of light vector with itself
    # Product is shifted left by 10 because two s10.5 values were multiplied,
    # and then shifted right by 16 because of vsar.
    # This means the result is shifted right by 6
                                                                        
    sdv vpos_scr_i.e4,  MG_VTX_XYZ + MG_VTX_SIZE,tmp_out;               vmudh v___, vdiff, vdiff
    #undef vpos_scr_i
    #undef vpos_scr_f

    #define vlsqdist_i    $v04
    #define vlsqdist_f    $v05
    ssv vinvw_i.e3, MG_VTX_INVWi + 0,          tmp_out;                 vsar  vlsqdist_f, COP2_ACC_MD
    ssv vinvw_i.e7, MG_VTX_INVWi + MG_VTX_SIZE,tmp_out;                 vsar  vlsqdist_i, COP2_ACC_HI
    #undef vinvw_i

    #define vtmp_i       $v08
    #define vtmp_f       $v09

    ssv vinvw_f.e3, MG_VTX_INVWf + 0,          tmp_out;                 vaddc vtmp_f, vtsqdist_f, vtsqdist_f.h1
    ssv vinvw_f.e7, MG_VTX_INVWf + MG_VTX_SIZE,tmp_out;                 vadd  vtmp_i, vtsqdist_i, vtsqdist_i.h1
    #undef vinvw_f

    #define vdist_i      $v10
    #define vdist_f      $v11

    ssv vnormw_f.e7, MG_VTX_Wf + MG_VTX_SIZE,tmp_out;                   vaddc vdist_f, vlsqdist_f, vlsqdist_f.h1
                                                                        vadd  vdist_i, vlsqdist_i, vlsqdist_i.h1
    #undef vnormw_f

    ssv vfogtmp.e0,  0,scratch_ptr;                                     vaddc vtsqdist_f, vtmp_f, vtsqdist_f.h2
    ssv vfogtmp.e4,  2,scratch_ptr;                                     vadd  vtsqdist_i, vtmp_i, vtsqdist_i.h2

    #undef vtmp_i
    #undef vtmp_f

    #define vinvd_i      $v12
    lsv vrgba.e3, 4,scratch_ptr;                                        vaddc vlsqdist_f, vdist_f, vlsqdist_f.h2
    lsv vrgba.e7, 4,scratch_ptr;                                        vadd  vlsqdist_i, vdist_i, vlsqdist_i.h2

    #define vtinvdist_i  $v09
    #define vtinvdist_f  $v13
    li t0, %lo(MG_NORMAL_MASK);                                         # <stall>
    lsv vinvd_i.e2, 0x8,t0;                                             vrsqh v___.e0,        vtsqdist_i.e0
    lsv vinvd_i.e6, 0x8,t0;                                             vrsql vtinvdist_f.e0, vtsqdist_f.e0
                                                                        vrsqh vtinvdist_i.e0, vtsqdist_i.e4
                                                                        vrsql vtinvdist_f.e4, vtsqdist_f.e4
                                                                        vrsqh vtinvdist_i.e4, vlsqdist_i.e0


    # Compute inverse distance (reciprocal square root)
    # Result is shifted left by 10:
    #   - Taking the square root halves the bit-shift, and the reciprocal then inverts it.
    #     So the original (right) shift of -6 becomes -(-6/2) = 3
    #   - vrsq additionally shifts left by 7
    # vinvd: --  1/d0  1.0  --  --  1/d1  1.0  --

    #define vlcol        $v16
    ldv vlcol.e0, MGFX_LIGHT_COLOR,s0;                                  vrsql vinvd_f.e1, vlsqdist_f.e0
    ldv vlcol.e4, MGFX_LIGHT_COLOR,s0;                                  vrsqh vinvd_i.e1, vlsqdist_i.e4
    #define vattcoeff_i  $v14
    #define vattcoeff_f  $v08
    ldv vattcoeff_i.e0, MGFX_LIGHT_ATT_INT,s0;                          vrsql vinvd_f.e5, vlsqdist_f.e4
    ldv vattcoeff_i.e4, MGFX_LIGHT_ATT_INT,s0;                          vrsqh vinvd_i.e5, vzero.e0
                             

    #define veposnorm    $v22
    ldv vattcoeff_f.e0, MGFX_LIGHT_ATT_FRAC,s0;                         vmudm v___,      vpos_eye, vtinvdist_f.h0
    ldv vattcoeff_f.e4, MGFX_LIGHT_ATT_FRAC,s0;                         vmadh veposnorm, vpos_eye, vtinvdist_i.h0
    #undef vtinvdist_i
    #undef vtinvdist_f

    # Get actual distance by multiplying the inverse with the squared distance: d^-1 * d^2 = d^(2-1) = d
    # Because vinvd.e2 is initialized to 1, the squared distance will be in vdist.e2
    # d is shifted left by 4, d^2 is still shifted right by 6
    # vdist: --  d0  d0^2  --  --  d1  d1^2  --
    addi light_count, -1;                                               vmudl v___,    vinvd_f, vlsqdist_f.h0
    slt t0, light_count, zero;                                          vmadm v___,    vinvd_i, vlsqdist_f.h0
    sub t0, zero, t0;                                                   vmadn vdist_f, vinvd_f, vlsqdist_i.h0
    ctc2 t0, COP2_CTRL_VCC;                                             vmadh vdist_i, vinvd_i, vlsqdist_i.h0
    #undef vlsqdist_i
    #undef vlsqdist_f
    #undef vtsqdist_i
    #undef vtsqdist_f
    andi fog_enabled, flags, MGFX_FLAG_FOG;                             vmrg vlcol, vzero, vlcol

    #define vdot         $v04
    sltu fog_enabled, zero, fog_enabled;                                vmulf vdot, veposnorm, vnorm



    # Multiply with attenuation coefficients
    # The coefficients are pre-shifted in such a way that all values end up being shifted right by 1, 
    # so the final result ends up non-shifted after the reciprocal below.
    # - d is shifted left by 4, so k1 is pre-shifted right by 4 on CPU
    # - d^2 is shifted right by 6, so k2 is pre-shifted left by 6 on CPU
    # vdist: --  k1*d0  k2*d0^2  --  --  k1*d1  k2*d1^2  --
    sb tr1, MG_VTX_TR_CODE + 0(tmp_out);                                vmudl v___,    vdist_f, vattcoeff_f
    sb tr2, MG_VTX_TR_CODE + MG_VTX_SIZE(tmp_out);                      vmadm v___,    vdist_i, vattcoeff_f
    sb clip1, MG_VTX_CLIP_CODE + 0(tmp_out);                            vmadn vdist_f, vdist_f, vattcoeff_i
    sb clip2, MG_VTX_CLIP_CODE + MG_VTX_SIZE(tmp_out);                  vmadh vdist_i, vdist_i, vattcoeff_i

    #define vtmp_f      $v05
    #define v0p5_i      $v06

                                                                        vadd vtmp_f, vdot, vdot.h1
                                                                        vadd v0p5_i, vzero, K128
    
    # Normalize light vector by multiplying the reciprocal distance.
    # Light vector is shifted left by 5 and inverse distance is shifted left by 10.
    # This means the result is shifted left by 15, which makes the result in vlightdir a signed fraction.
    # This happens to match perfectly so we can continue the following calculations without any adjustment.

    # Compute final attenuation factor
    # Sum is shifted right by 1
    # k0 + k1*d + k2*d^2
    sll t0, fog_enabled, 4;                                             vaddc vattcoeff_f, vdist_f.h1
    or fog_enabled, t0;                                                 vadd  vattcoeff_i, vdist_i.h1
    
    sll fog_enabled, 3;                                                 vadd vdot, vtmp_f, vdot.h2
    #undef vtmp_f

    #define vppoint      $v05
    lsv vfogtmp.e3, 0,scratch_ptr;                                      vmudm v___,    vdiff, vinvd_f.h1
    lsv vfogtmp.e7, 2,scratch_ptr;                                      vmadh vppoint, vdiff, vinvd_i.h1
    #undef vinvd_i
    #undef vdiff

    andi env_enabled, flags, MGFX_FLAG_ENV_MAP;                         vaddc vattcoeff_f, vdist_f.h2
    sltu env_enabled, zero, env_enabled;                                vadd  vattcoeff_i, vdist_i.h2

    lh is_point, %lo(MGFX_LIGHT_POSITION) + 0x6(s0);                    vlt vdot, vzero
    #undef vdist_i
    #undef vdist_f
    
    # Final factor is not shifted
    # 1 / (k0 + k1*d + k2*d^2)

    #define vatt_i      $v03
    #define vatt_f      $v07
    #define venvtmp     $v18
    sub env_enabled, zero, env_enabled;                                 vxor venvtmp, venvtmp, venvtmp
    li t0, %lo(TEXTURING);                                              # <stall>
    #define vtexscale       $v21
    llv vtexscale.e0,  0x0,t0;                                          vrcph v___.e0,   vattcoeff_i.e0
    llv vtexscale.e4,  0x0,t0;                                          vrcpl vatt_f.e0, vattcoeff_f.e0
    li tmp_out, %lo(MG_CLIP_CACHE);                                     vrcph vatt_i.e0, vattcoeff_i.e4
    sltu is_point, zero, is_point;                                      vrcpl vatt_f.e4, vattcoeff_f.e4
    sub is_point, zero, is_point;                                       vrcph vatt_i.e4, vzero.e0

    ctc2 is_point, COP2_CTRL_VCC;                                       vsub vdot, vzero, vdot
    #undef vattcoeff_i
    #undef vattcoeff_f



    add t3, vtx_in0_ptr, vtx_size2;                                     vmrg vlpos, vppoint, vlpos

    #undef vppoint
    #define vcpoint     $v05
    
    # Multiply by attenuation (if the light is directional, it's just 1)
    lsv venvtmp.e2, 4,scratch_ptr;                                      vmudm v___,    vlcol, vatt_f.h0
    lsv venvtmp.e6, 4,scratch_ptr;                                      vmadh vcpoint, vlcol, vatt_i.h0
    #undef vatt_i
    #undef vatt_f

    add t4, vtx_in1_ptr, vtx_size2;                                                                                                         vmulf v___, vnorm, vdot.h0
    #define vpos_in      $v19
    LOAD_POS_NORM4: ldv vpos_in.e0, 0,t3;                               vmacf v___, vnorm, vdot.h0 # Add twice to account for multiplication by 2
    LOAD_POS_NORM5: ldv vpos_in.e4, 0,t4;                               vmadh v___, veposnorm, K1
    #undef veposnorm
    li t3, %lo(MG_NORMAL_MASK);                                         vmadh v___, venvtmp, K1
    #undef venvtmp
    #undef vdot

    #define refl_i       $v18
    #define refl_f       $v04
    #define vtexoffset      $v22
    lsv vpos_in.e3, 0xA,t3;                                             vsar refl_f, COP2_ACC_MD
    lsv vpos_in.e7, 0xA,t3;                                             vsar refl_i, COP2_ACC_HI
    
    llv vtexoffset.e0, 0x4,t0;                                          vmrg vlcol, vcpoint, vlcol
    

    #undef vcpoint
                                                        
    #define vndl     $v07

    # Dot product of light vector with vertex normal
    # Both are a signed fraction, so we can just use vmulf
    # Diffuse light = light_color * dot(light_vec, normal)
    llv vtexoffset.e4, 0x4,t0;                                          vmulf vndl, vnorm, vlpos

    #define vtsqdist_i   $v27
    #define vtsqdist_f   $v10
    ldv vlpos.e0, MGFX_LIGHT_POSITION+MGFX_LIGHT_SIZE,s0;               vmudl v___,      refl_f, refl_f
    ldv vlpos.e4, MGFX_LIGHT_POSITION+MGFX_LIGHT_SIZE,s0;               vmadm v___,      refl_i, refl_f
    li t0, %lo(MATRICES);                                               vmadn vtsqdist_f, refl_f, refl_i
    #define vmvp2_f     $v08
    ldv vmvp2_f.e0, 0x30,t0;                                            vmadh vtsqdist_i, refl_i, refl_i

    #define vltmp       $v11
    ldv vmvp2_f.e4, 0x30,t0;                                            vadd  vltmp, vndl, vndl.h1

    #define vtmp_i      $v12
    #define vtmp_f      $v13
    #define vmvp3_i     $v09
    ldv vmvp3_i.e0, 0x18,t0;                                            # <stall>
    ldv vmvp3_i.e4, 0x18,t0;                                            vaddc vtmp_f, vtsqdist_f, vtsqdist_f.h1
        
    #define vmmv1_f     $v14
    ldv vmmv1_f.e0, 0x68,t0;                                            vadd  vtmp_i, vtsqdist_i, vtsqdist_i.h1

    ldv vmmv1_f.e4, 0x68,t0;                                            vadd  vndl, vltmp, vndl.h2
    #undef vltmp

    #define vmvp0_i     $v03
    ldv vmvp0_i.e0, 0x00,t0;                                            # <stall>
    ldv vmvp0_i.e4, 0x00,t0;                                            vaddc vtsqdist_f, vtmp_f, vtsqdist_f.h2
    #undef vtmp_f
    #define vmvp1_i     $v05
    ldv vmvp1_i.e0, 0x08,t0;                                            vadd  vtsqdist_i, vtmp_i, vtsqdist_i.h2
    #undef vtmp_i

    ldv vmvp1_i.e4, 0x08,t0;                                            vge   vndl, vzero

    #define vmmv0_i     $v11
    ldv vmmv0_i.e0, 0x40,t0;                                            # <stall>
    ldv vmmv0_i.e4, 0x40,t0;                                            # <stall>
    #define vmmv1_i     $v13
    ldv vmmv1_i.e0, 0x48,t0;                                            vrsqh v___.e0,        vtsqdist_i.e0
    #define vtinvdist_f  $v15
    ldv vmmv1_i.e4, 0x48,t0;                                            vrsql vtinvdist_f.e0, vtsqdist_f.e0
    #define vtinvdist_i  $v27
    #define vmmv0_f     $v12
    ldv vmmv0_f.e0, 0x60,t0;                                            vrsqh vtinvdist_i.e0, vtsqdist_i.e4
    #undef vtsqdist_i
    ldv vmmv0_f.e4, 0x60,t0;                                            vrsql vtinvdist_f.e4, vtsqdist_f.e4
    #undef vtsqdist_f
    #define vmvp3_f     $v10
    ldv vmvp3_f.e0, 0x38,t0;                                            vrsqh vtinvdist_i.e4, vzero.e0

    ldv vmvp3_f.e4, 0x38,t0;                                            vmulf vlcol, vndl.h0
    #undef vndl
    #define vmvp2_i     $v07

    ldv vmvp2_i.e0, 0x10,t0;                                            vmudh v___,    v0p5_i, K1
    #undef v0p5_i
    #define vmvp1_f     $v06
    ldv vmvp2_i.e4, 0x10,t0;                                            vmadl v___,    refl_f, vtinvdist_f.h0
    ldv vmvp1_f.e0, 0x28,t0;                                            vmadm v___,    refl_i, vtinvdist_f.h0
    #undef vtinvdist_f
    ldv vmvp1_f.e4, 0x28,t0;                                            vmadn v___,    refl_f, vtinvdist_i.h0
    #undef refl_f

    #define vtexenv     $v15
    #define vmvp0_f     $v04
    ldv vmvp0_f.e0, 0x20,t0;                                            vmadh vtexenv, refl_i, vtinvdist_i.h0
    #undef vtinvdist_i
    #undef refl_i

    ldv vmvp0_f.e4, 0x20,t0;                                            vadd vrgba, vlcol
    bgtz light_count, light_loop;
    addi s0, MGFX_LIGHT_SIZE;

    #undef vlpos
    #undef vlcol
    #undef vinvd_f
    
    #define vnormfactor     $v27

    move tmp_out, vtx_out_ptr;                                          # <stall>
    ldv vnormfactor.e0, 0x8,t5;                                         # <stall>
    ctc2 env_enabled, COP2_CTRL_VCC;                                    PATCH_COLOR0: vmulf vrgba, vrgba_in
    add vtx_in0_ptr, vtx_size2;                                         # <stall>
    ldv vnormfactor.e4, 0x8,t5;                                         vmrg vtex, vtexenv, vtex
    ctc2 fog_enabled, COP2_CTRL_VCC;                                    
    add vtx_in1_ptr, vtx_in0_ptr, vtx_size;                             vmrg vrgba, vfogtmp, vrgba
    LOAD_POS_NORM6: ldv vnorm.e0, 0,vtx_in0_ptr;                        vmudh v___, vtexoffset, K1
    LOAD_POS_NORM7: ldv vnorm.e4, 0,vtx_in1_ptr;                        vmadh vtex, vtex, vtexscale


    #undef vtexscale
    #undef vtexoffset

    #undef vnorm
    #undef vrgba_in
    #undef vpos_in
    
    blt vtx_in0_ptr, vtx_in_end, vertex_loop
    addi vtx_out_ptr, MG_VTX_SIZE*2
    
#if MGFX_TRACE_VTX_LOOP
    emux_trace_stop
#endif

    suv vrgba.e0, MG_VTX_RGBA + 0,          tmp_out
    suv vrgba.e4, MG_VTX_RGBA + MG_VTX_SIZE,tmp_out
    slv vtex.e0, MG_VTX_ST + 0,          tmp_out
    j RSPQ_Loop
    slv vtex.e4, MG_VTX_ST + MG_VTX_SIZE,tmp_out

    #undef vtex
    #undef vrgba

    #undef v___
MgEndShader
