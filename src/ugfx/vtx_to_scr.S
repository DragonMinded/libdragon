vtx_to_scr:
  # Inputs
  #define vtx2scr_hvtxi $v28
  #define vtx2scr_hvtxf $v27
  #define vtx2scr_vscale $v26
  #define vtx2scr_voffset $v25
  #define vtx2scr_dest $20
  # Clobbers
  #define wrecpi $v24
  #define wrecpf $v23
  #define intmi $v22
  #define intmf $v21
  #define resulti $v20
  #define resultf $v19
  #define vtmp $v18
  #define pfactor $v17
  #define cratio $v16
  #define tmp $19

    # Load perspective normalization factor
    li tmp, %lo(persp_norm_factor)
    lsv pfactor, 0x0, 0, tmp

    # Load clip ratio
    li tmp, %lo(clip_ratio)
    ldv cratio, 0x0, 0, tmp

    #define clip1 $19
    #define clip2 $18

    # Get clip codes
    vch vtmp, vtx2scr_hvtxi, vtx2scr_hvtxi, VE_3
    vcl vtmp, vtx2scr_hvtxf, vtx2scr_hvtxf, VE_3
    cfc2 clip1, COP2_CTRL_VCC

    # Get extended clip codes
    vmudn intmf, vtx2scr_hvtxf, cratio, VE_V
    vmadh intmi, vtx2scr_hvtxi, cratio, VE_V
    vmadn intmf, vzero, vzero, VE_0
    vch vtmp, intmi, intmi, VE_3
    vcl vtmp, intmf, intmf, VE_3
    cfc2 clip2, COP2_CTRL_VCC

    # Perform perspective normalization
    vmudl intmf, vtx2scr_hvtxf, pfactor, VE_0
    vmadm intmi, vtx2scr_hvtxi, pfactor, VE_0
    vmadn intmf, vzero, vzero, VE_0

    # Get reciprocal of w
    vrcph wrecpi, VE_0, intmi, VE_3
    vrcpl wrecpf, VE_0, intmf, VE_3
    vrcph wrecpi, VE_0, vzero, VE_0

    # Shift left by 1 because of vrcp
    vmudn wrecpf, wrecpf, vshl, VE_1
    vmadh wrecpi, wrecpi, vshl, VE_1
    vmadn wrecpf, vzero, vzero, VE_0

    # Multiply result by 1/w to get normalized device coordinates
    vmudl vtmp,  intmf, wrecpf, VE_0
    vmadm vtmp,  intmi, wrecpf, VE_0
    vmadn intmf, intmf, wrecpi, VE_0
    vmadh intmi, intmi, wrecpi, VE_0

    # Adjust to viewport (scale and offset)
    # Automatically converts the result to s13.2 since that is the viewport format
    vmudn resultf, intmf, vtx2scr_vscale, VE_V
    vmadh resulti, intmi, vtx2scr_vscale, VE_V
    vmadn resultf, vzero, vzero, VE_0
    vadd resulti, resulti, vtx2scr_voffset, VE_V

    # Save screen space X and Y to output
    slv resulti, 0x0, 0x10, vtx2scr_dest

    # Save Z to output
    vmudn resultf, resultf, vshl, VE_5
    vmadh resulti, resulti, vshl, VE_5
    vmadn resultf, vzero, vzero, VE_0
    ssv resulti, 0x4, 0x14, vtx2scr_dest
    ssv resultf, 0x4, 0x16, vtx2scr_dest

    # Save 1/W to output
    ssv wrecpi, 0x0, 0x20, vtx2scr_dest
    ssv wrecpf, 0x0, 0x22, vtx2scr_dest

    # Save clip codes to output
    andi clip1, clip1, 0x0707
    andi clip2, clip2, 0x0707
    sh clip1, 0x24(vtx2scr_dest)
    jr ra
    sh clip2, 0x26(vtx2scr_dest)

  #undef wrecpi
  #undef wrecpf
  #undef intmi
  #undef intmf
  #undef resulti
  #undef resultf
  #undef vtmp
  #undef pfactor
  #undef clip_ratio
  #undef clip1
  #undef clip2
  #undef tmp
