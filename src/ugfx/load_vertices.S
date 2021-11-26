load_vertices:
  #define v_off $2
  #define vertices_end $3
  #define mtx_cur $4
  #define in_pos $5
  #define gmode $6
  #define light_en $7
  #define light_num $8
  #define rgba $9
  #define mvaddr $10
  #define tmp $11

  #define mtxc0i $v01
  #define mtxc0f $v02
  #define mtxc1i $v03
  #define mtxc1f $v04
  #define mtxc2i $v05
  #define mtxc2f $v06
  #define mtxc3i $v07
  #define mtxc3f $v08
  #define texscale $v09
  #define mvc0i $v10
  #define mvc0f $v11
  #define mvc1i $v12
  #define mvc1f $v13
  #define mvc2i $v14
  #define mvc2f $v15
  #define vtmp $v29

    # vertex offset shifted left by 4 bits (vertex struct is 16 bytes)
    andi v_off, COMMAND_WORD, 0x3F0
    srl dma_len, COMMAND_WORD, 8
    andi dma_len, dma_len, 0x3F0
    jal load_from_address_slot
    addiu dma_mem_addr, $0, tmp_buffer

    addu vertices_end, dma_mem_addr, dma_len
    addiu in_pos, dma_mem_addr, 0
    sll vtx2scr_dest, v_off, 1
    srl tmp, v_off, 1
    add vtx2scr_dest, vtx2scr_dest, tmp
    addiu vtx2scr_dest, vtx2scr_dest, vtx_cache

    lh gmode, %lo(g_mode)
    andi light_en, gmode, 0x10
    lh light_num, %lo(num_lights)

    # Load matrix columns
    li mtx_cur, %lo(mtx_current)
    ldv mtxc0i, 0, 0x00, mtx_cur
    ldv mtxc1i, 0, 0x08, mtx_cur
    ldv mtxc2i, 0, 0x10, mtx_cur
    ldv mtxc3i, 0, 0x18, mtx_cur
    ldv mtxc0f, 0, 0x20, mtx_cur
    ldv mtxc1f, 0, 0x28, mtx_cur
    ldv mtxc2f, 0, 0x30, mtx_cur
    ldv mtxc3f, 0, 0x38, mtx_cur

    # Load viewport
    li tmp, %lo(vport)
    ldv vtx2scr_vscale, 0x0, 0x0, tmp
    ldv vtx2scr_voffset, 0x0, 0x8, tmp

    # Load texture scale
    li tmp, %lo(texture_scale)
    llv texscale, 0x0, 0x0, tmp

    beq light_en, $0, transform_loop
    lh mvaddr, %lo(mtx_mv_stack_ptr)

    ldv mvc0i, 0x0, 0x00, mvaddr
    ldv mvc1i, 0x0, 0x08, mvaddr
    ldv mvc2i, 0x0, 0x10, mvaddr
    ldv mvc0f, 0x0, 0x20, mvaddr
    ldv mvc1f, 0x0, 0x28, mvaddr
    ldv mvc2f, 0x0, 0x30, mvaddr

    # Transform loaded vertices by current matrix
  transform_loop:
    #define vpos $v16
    #define tcoords $v17

    sub tmp, vertices_end, in_pos
    blez tmp, main_loop_advance_input

    ldv vpos, 0x0, 0x0, in_pos

    llv tcoords, 0x0, 0x8, in_pos
    
    lw rgba, 0xC(in_pos)
    beq light_en, $0, no_lighting

    #define color $v18
    #define light_pos $10

    addiu light_pos, $0, light_buffer
    luv color, 0x0, 0x0, light_pos
    beq light_num, $0, light_loop_end

    #define normal $v19

    lpv normal, 0x0, 0x8, in_pos

    vmudn vtmp, mvc0f, normal, VE_4
    vmadh vtmp, mvc0i, normal, VE_4
    vmadn vtmp, mvc1f, normal, VE_5
    vmadh vtmp, mvc1i, normal, VE_5
    vmadn vtmp, mvc2f, normal, VE_6
    vmadh vtmp, mvc2i, normal, VE_6

    #define normi $v19
    #define normf $v20
    #define sqni $v21
    #define sqnf $v22
    #define tmpi $v23
    #define tmpf $v24

    vsar normi, normi, normi, VE_0
    vsar normf, normf, normf, VE_1

    vmudl vtmp, normf, normf
    vmadm vtmp, normi, normf
    vmadn sqnf, normf, normi
    vmadh sqni, normi, normi

    vaddc tmpf, sqnf, sqnf, VE_1
    vadd  tmpi, sqni, sqni, VE_1
    vaddc sqnf, tmpf, sqnf, VE_2
    vadd  sqni, tmpi, sqni, VE_2

    vrsqh vtmp, VE_0, sqni, VE_0
    vrsql sqnf, VE_0, sqnf, VE_0
    vrsqh sqni, VE_0, vzero, VE_0

    vmudl vtmp, normf, sqnf, VE_0
    vmadm vtmp, normi, sqnf, VE_0
    vmadn tmpf, normf, sqni, VE_0
    vmadh tmpi, normi, sqni, VE_0

    #define nx $v19

    addiu tmp, $0, 0x0100
    mtc2 tmp, vtmp, 0x0

    vmudn nx, tmpf, vtmp, VE_0
    vmadh nx, tmpi, vtmp, VE_0

    #define ny $v20
    #define nz $v21

    vadd ny, vzero, nx, VE_1
    vadd nz, vzero, nx, VE_2

  light_loop:
    #define lcol $v22

    #define ln $v23

    lpv ln,   0x0, LIGHT_SIZE, light_pos
    luv lcol, 0x0, LIGHT_SIZE, light_pos

    # dot(n, l)
    vmulf vtmp, nx, ln, VE_4
    vmacf vtmp, ny, ln, VE_5
    vmacf vtmp, nz, ln, VE_6

    #define dot $v23

    vge dot, vtmp, vzero

    addiu tmp, $0, 0x7FFF
    mtc2 tmp, vtmp, 0x0

    # color += lcol * dot(n, l)
    vmulf color, color, vtmp, VE_0
    vmacf color, lcol,  dot,  VE_0

    addiu tmp, light_num, light_buffer
    sub tmp, light_pos, tmp
    bltz tmp, light_loop
    addiu light_pos, light_pos, LIGHT_SIZE

  light_loop_end:

    #define alpha $v19

    luv alpha, 0x0, 0x8, in_pos
    vmov color, VE_3, alpha, VE_7
    spv color, 0x0, 0x0, in_pos
    lw rgba, 0x0(in_pos)

  no_lighting:
    # Multiply texture coordinates with scale
    vmudm tcoords, tcoords, texscale
    
    # Save ST to output
    slv tcoords, 0x0, 0x18, vtx2scr_dest

    # Save RGBA to output
    sw rgba, 0x1C(vtx2scr_dest)

    # Multiply columns with vertex
    vmudn vtmp, mtxc0f, vpos, VE_0
    vmadh vtmp, mtxc0i, vpos, VE_0
    vmadn vtmp, mtxc1f, vpos, VE_1
    vmadh vtmp, mtxc1i, vpos, VE_1
    vmadn vtmp, mtxc2f, vpos, VE_2
    vmadh vtmp, mtxc2i, vpos, VE_2
    vmadn vtx2scr_hvtxf, mtxc3f, vshl, VE_5
    vmadh vtx2scr_hvtxi, mtxc3i, vshl, VE_5

    # Shift result right by 5 to convert from s10.5
    # TODO: do this elsewhere to preserve some precision?
    vmudl vtmp, vtx2scr_hvtxf, vshr, VE_5
    vmadm vtx2scr_hvtxi, vtx2scr_hvtxi, vshr, VE_5
    vmadn vtx2scr_hvtxf, vzero, vzero, VE_0
    
    # Save homogeneous coordinates to output
    sdv vtx2scr_hvtxi, 0x0, 0x00, vtx2scr_dest
    sdv vtx2scr_hvtxf, 0x0, 0x08, vtx2scr_dest

    # Transform to screen space and compute attributes
    jal vtx_to_scr
    addiu in_pos, in_pos, VTX_IN_SIZE

    j transform_loop
    addiu vtx2scr_dest, vtx2scr_dest, VTX_SIZE

  #undef tmp
  #undef v_off
  #undef vertices_end
  #undef mtx_cur
  #undef in_pos
  #undef gmode
  #undef light_en
  #undef light_num
  #undef rgba
  #undef mvaddr
  #undef mtxc0i
  #undef mtxc0f
  #undef mtxc1i
  #undef mtxc1f
  #undef mtxc2i
  #undef mtxc2f
  #undef mtxc3i
  #undef mtxc3f
  #undef texscale
  #undef mvc0i
  #undef mvc0f
  #undef mvc1i
  #undef mvc1f
  #undef mvc2i
  #undef mvc2f
  #undef vtmp
