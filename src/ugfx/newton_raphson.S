newton_raphson:
  # Inputs
  #define newton_iterations $20
  #define newton_denomi $v28
  #define newton_denomf $v27
  # Outputs
  #define newton_recpi $v26
  #define newton_recpf $v25
  # Clobbers
  #define tmp1i $v24
  #define tmp1f $v23
  #define tmp2i $v22
  #define tmp2f $v21

    # First, get approximate reciprocal via lookup table
    vrcph newton_recpi, VE_0, newton_denomi, VE_0
    vrcpl newton_recpf, VE_0, newton_denomf, VE_0
    vrcph newton_recpi, VE_0, vzero, VE_0

    vmudn newton_recpf, newton_recpf, vshl,  VE_1
    vmadh newton_recpi, newton_recpi, vshl,  VE_1
    vmadn newton_recpf, vzero,        vzero, VE_0

  newton_raphson_loop:
    vmudl tmp1f, newton_recpf, newton_denomf, VE_0
    vmadm tmp1i, newton_recpi, newton_denomf, VE_0
    vmadn tmp1f, newton_recpf, newton_denomi, VE_0
    vmadh tmp1i, newton_recpi, newton_denomi, VE_0
  
    vsubc tmp1f, vzero, tmp1f, VE_0
    vsub  tmp1i, vshl,  tmp1i, VE_0

    vmudl tmp2f,        newton_recpf, tmp1f, VE_1
    vmadm tmp2i,        newton_recpi, tmp1f, VE_1
    vmadn newton_recpf, newton_recpf, tmp1i, VE_1
    vmadh newton_recpi, newton_recpi, tmp1i, VE_1

    addiu newton_iterations, newton_iterations, -1
    bgtz newton_iterations, newton_raphson_loop
    nop

    jr ra
    nop
    
  #undef tmp1i
  #undef tmp1f
  #undef tmp2i
  #undef tmp2f
