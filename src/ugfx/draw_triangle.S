# -------------------------------------------------------------------
#  draw_triangle: Given three vertices, pack an RDP Edge Coefficients
#                 instruction (for a triangle primitive) accordingly.
#
# Steps:
#  * Trivial rejection (Frustum culling using pre-calculated clip codes)
#  * Frustum Clipping
#  * For each triangle created by clipping:
#    * Back face culling
#    * Sort vertices by y-coordinate (ascending).
#    * Compute edge walker slopes and cross-product.
#    * Utilize partial cross-product to determine if lft or not.
#    * Compute shade, texture, and z-buffer coefficients with full cross product
# -------------------------------------------------------------------
draw_triangle:
  #define tmp $19
  #define vtx1 $2
  #define vtx2 $3
  #define vtx3 $4

    # TODO: Maybe just straight up pass offset into vertex buffer as parameters?

    andi vtx3, COMMAND_WORD, 0x7E0
    srl tmp, vtx3, 2
    add vtx3, vtx3, tmp

    srl vtx1, COMMAND_WORD, 12
    andi vtx1, vtx1, 0x7E0
    srl tmp, vtx1, 2
    add vtx1, vtx1, tmp

    srl vtx2, COMMAND_WORD, 6
    andi vtx2, vtx2, 0x7E0
    srl tmp, vtx2, 2
    add vtx2, vtx2, tmp

    addiu vtx3, vtx3, vtx_cache
    addiu vtx2, vtx2, vtx_cache
    addiu vtx1, vtx1, vtx_cache

    #define clip_all $5

    #define clip1 $6
    #define clip2 $7
    #define clip3 $8

    lh clip1, 0x24(vtx1)
    lh clip2, 0x24(vtx2)
    lh clip3, 0x24(vtx3)

    # Trivial rejection
    and tmp, clip1, clip2
    and tmp, tmp, clip3
    bne tmp, $0, main_loop_advance_input

    lh clip1, 0x26(vtx1)
    lh clip2, 0x26(vtx2)
    lh clip3, 0x26(vtx3)

    # Clipping
    or clip_all, clip1, clip2
    or clip_all, clip_all, clip3

    # If clip codes are all 0, then no clipping must be done. Simply draw the triangle as-is.
    beq clip_all, $0, draw_clipped_triangle
    addiu ra, $0, main_loop_advance_input

    # Otherwise, start clipping

    # Sutherland-Hodgman algorithm (https://en.wikipedia.org/wiki/Sutherland%E2%80%93Hodgman_algorithm)
    # At most one vertex is added per clipping plane.
    # Since we have 6 clipping planes we need space for 9 vertices.
    # Per vertex, the transformed data and the homogeneous coordinates are saved,
    # which adds up to 40 bytes per vertex. So each list is 40 x 9 = 360 bytes long (0x168).
    # We use two list for a double-buffered algorithm, which results in 0x2D0 bytes total.

    #define output_list $6
    #define output_len $7

    #define input_list $8
    #define input_len $9

    #define planeidx $10
    #define plane $11
    #define edge_idx $12
    #define prev_idx $13
    #define vc $14
    #define vp $15
    #define cc $16
    #define cp $17

    #define vscale $v26
    #define voffset $v25

    # Load viewport
    li tmp, %lo(vport)
    ldv vscale, 0x0, 0x0, tmp
    ldv voffset, 0x0, 0x8, tmp

    addiu input_list, $0, tmp_buffer
    addiu output_list, $0, tmp_buffer + CLIP_LIST_SIZE

    # Save first three vertices to input list

    # TODO: make this shorter
    ldv $v01, 0x0, 0x00, vtx1
    ldv $v02, 0x0, 0x08, vtx1
    ldv $v03, 0x0, 0x10, vtx1
    ldv $v04, 0x0, 0x18, vtx1
    ldv $v05, 0x0, 0x20, vtx1

    sdv $v01, 0x0, 0x00, output_list
    sdv $v02, 0x0, 0x08, output_list
    sdv $v03, 0x0, 0x10, output_list
    sdv $v04, 0x0, 0x18, output_list
    sdv $v05, 0x0, 0x20, output_list

    addiu tmp, output_list, VTX_SIZE

    ldv $v01, 0x0, 0x00, vtx2
    ldv $v02, 0x0, 0x08, vtx2
    ldv $v03, 0x0, 0x10, vtx2
    ldv $v04, 0x0, 0x18, vtx2
    ldv $v05, 0x0, 0x20, vtx2

    sdv $v01, 0x0, 0x00, tmp
    sdv $v02, 0x0, 0x08, tmp
    sdv $v03, 0x0, 0x10, tmp
    sdv $v04, 0x0, 0x18, tmp
    sdv $v05, 0x0, 0x20, tmp

    addiu tmp, tmp, VTX_SIZE

    ldv $v01, 0x0, 0x00, vtx3
    ldv $v02, 0x0, 0x08, vtx3
    ldv $v03, 0x0, 0x10, vtx3
    ldv $v04, 0x0, 0x18, vtx3
    ldv $v05, 0x0, 0x20, vtx3

    sdv $v01, 0x0, 0x00, tmp
    sdv $v02, 0x0, 0x08, tmp
    sdv $v03, 0x0, 0x10, tmp
    sdv $v04, 0x0, 0x18, tmp
    sdv $v05, 0x0, 0x20, tmp

    addiu tmp, output_list, VTX_SIZE

    addiu output_len, $0, 3 * VTX_SIZE

    # Iterate clipping planes
    addiu planeidx, $0, 0

  clip_planes_loop:
    slti tmp, planeidx, CPLANE_COUNT
    beq tmp, $0, clip_planes_loop_end
    nop

    sll tmp, planeidx, 1
    lh plane, %lo(clip_plane_masks)(tmp)
    and tmp, plane, clip_all
    beq tmp, $0, clip_planes_loop_continue
    nop

    # Swap input and output lists
    addiu tmp, input_list, 0
    addiu input_list, output_list, 0
    addiu output_list, tmp, 0
    addiu input_len, output_len, 0

    # Clear output list
    addiu output_len, $0, 0

    # Iterate edges
    addiu edge_idx, $0, 0
    addi prev_idx, input_len, -VTX_SIZE
    
  clip_edge_loop:
    slt tmp, edge_idx, input_len
    beq tmp, $0, clip_edge_loop_end

    add vc, edge_idx, input_list
    add vp, prev_idx, input_list

    lh cc, 0x26(vc)
    lh cp, 0x26(vp)
    and cc, cc, plane
    and cp, cp, plane
    xor tmp, cc, cp
    beq tmp, $0, clip_no_intersection
    nop

    # Compute intersection of edge with clipping plane

    #define pcoeff $v01
    #define pi $v02
    #define pf $v03
    #define doti $v04
    #define dotf $v05
    #define diffi $v06
    #define difff $v07
    #define ai $v08
    #define af $v09
    #define vtmp $v10
    #define ipi $v28
    #define ipf $v27

    sll tmp, planeidx, 3

    # +----+----+----+----+----+----+----+----+
    # | xp | yp | zp | wp | xp | yp | zp | wp |
    # +----+----+----+----+----+----+----+----+
    addiu tmp, tmp, %lo(clip_plane_coefficients)
    ldv pcoeff, 0x0, 0x0, tmp
    ldv pcoeff, 0x8, 0x0, tmp

    # Load clip ratio
    li tmp, %lo(clip_ratio)
    ldv vtmp, 0x0, 0x0, tmp
    ldv vtmp, 0x8, 0x0, tmp

    # +----+----+----+----+----+----+----+----+
    # | x0 | y0 | z0 | w0 | x1 | y1 | z1 | w1 |
    # +----+----+----+----+----+----+----+----+
    ldv pi, 0x0,  0x00, vp
    ldv pf, 0x0,  0x08, vp
    ldv pi, 0x8,  0x00, vc
    ldv pf, 0x8,  0x08, vc

    # Multiply plane coefficients with clip ratio
    vmudh pcoeff, pcoeff, vtmp, VE_V

    # +----+----+----+----+----+----+----+----+
    # | x1 | y1 | z1 | w1 |    |    |    |    |
    # +----+----+----+----+----+----+----+----+
    ldv ipi, 0x0, 0x00, vc
    ldv ipf, 0x0, 0x08, vc

    # dot[0]: d0 = dot(p0, plane)
    # dot[4]: d1 = dot(p1, plane)
    vmudn dotf, pf, pcoeff, VE_V
    vmadh doti, pi, pcoeff, VE_V
    vmadn dotf, vzero, vzero, VE_0

    vaddc dotf, dotf, dotf, VE_1q
    vadd  doti, doti, doti, VE_1q
    vaddc dotf, dotf, dotf, VE_2h
    vadd  doti, doti, doti, VE_2h

    # d0 - d1
    vsubc difff, dotf, dotf, VE_4
    vsub  diffi, doti, doti, VE_4

    # 1 / (d0 - d1)
    vrcph ai, VE_0, diffi, VE_0
    vrcpl af, VE_0, difff, VE_0
    vrcph ai, VE_0, vzero, VE_0

    vmudn af, af, vshl, VE_1
    vmadh ai, ai, vshl, VE_1
    vmadn af, vzero, vzero, VE_0

    # a = d0 / (d0 - d1)
    vmudl vtmp, af, dotf, VE_0
    vmadm vtmp, ai, dotf, VE_0
    vmadn af,   af, doti, VE_0
    vmadh ai,   ai, doti, VE_0

    # p1 - p0
    vsubc ipf, ipf, pf, VE_V
    vsub  ipi, ipi, pi, VE_V

    # (p1 - p0) * a
    vmudl vtmp, ipf, af, VE_0
    vmadm vtmp, ipi, af, VE_0
    vmadn ipf,  ipf, ai, VE_0
    vmadh ipi,  ipi, ai, VE_0

    # p0 + (p1 - p0) * a (intersection)
    vaddc ipf, ipf, pf, VE_V
    vadd  ipi, ipi, pi, VE_V

    add vtx2scr_dest, output_len, output_list
    sdv ipi, 0x0, 0x00, vtx2scr_dest

    jal vtx_to_scr
    sdv ipf, 0x0, 0x08, vtx2scr_dest

    #define attp $v11
    #define attc $v12

    # Interpolate attributes
    luv attp, 0x0, 0x18, vp
    luv attc, 0x0, 0x18, vc
    llv attp, 0x0, 0x18, vp
    llv attc, 0x0, 0x18, vc

    vsub attc, attc, attp, VE_V

    vmudm vtmp, attc, af, VE_0
    vmadh attc, attc, ai, VE_0

    vadd attp, attp, attc, VE_V

    suv attp, 0x0, 0x18, vtx2scr_dest
    slv attp, 0x0, 0x18, vtx2scr_dest

    addiu output_len, output_len, VTX_SIZE

  clip_no_intersection:
    bne cc, $0, clip_no_current

    ldv $v01, 0x0, 0x00, vc
    ldv $v02, 0x0, 0x08, vc
    ldv $v03, 0x0, 0x10, vc
    ldv $v04, 0x0, 0x18, vc
    ldv $v05, 0x0, 0x20, vc
    add tmp, output_len, output_list
    addiu output_len, output_len, VTX_SIZE
    sdv $v01, 0x0, 0x00, tmp
    sdv $v02, 0x0, 0x08, tmp
    sdv $v03, 0x0, 0x10, tmp
    sdv $v04, 0x0, 0x18, tmp
    sdv $v05, 0x0, 0x20, tmp

  clip_no_current:
    addiu prev_idx, edge_idx, 0
    j clip_edge_loop
    addiu edge_idx, edge_idx, VTX_SIZE

  clip_edge_loop_end:
  clip_planes_loop_continue:
    j clip_planes_loop
    addiu planeidx, planeidx, 1

  clip_planes_loop_end:

    #define tri_idx $5

    # Draw each triangle
    addiu tri_idx, $0, 0

  clip_draw_loop:
    addi tmp, output_len, -(VTX_SIZE * 2)
    slt tmp, tri_idx, tmp
    beq tmp, $0, main_loop_advance_input
    addiu vtx1, output_list, 0

    add tmp, tri_idx, output_list
    addiu vtx2, tmp, VTX_SIZE * 1

    jal draw_clipped_triangle
    addiu vtx3, tmp, VTX_SIZE * 2

    j clip_draw_loop
    addiu tri_idx, tri_idx, VTX_SIZE
  #undef tmp
  #undef tmp2
  #undef vtx1
  #undef vtx2
  #undef vtx3


draw_clipped_triangle:
  # Inputs (immutable)
  #define ivtx1 $2
  #define ivtx2 $3
  #define ivtx3 $4
  # Clobbers
  #define return $8
  #define x1y1 $v01
  #define x3y2 $v02
  #define x2y1 $v03
  #define x1y3 $v04
  #define vdelta1 $v05
  #define vdelta2 $v06
  #define cross $v07

  addiu return, ra, 0

  #define cull $9
  #define is_back $10
  #define is_front $11
  #define flags $12
  
  lh cull, %lo(cull_mode)
  beq cull, $0, cull_triangle_pass

  # Compute cross product of edges
  llv x1y1, 0x0, 0x10, ivtx1
  lsv x3y2, 0x0, 0x10, ivtx3
  lsv x3y2, 0x2, 0x12, ivtx2
  lsv x2y1, 0x0, 0x10, ivtx2
  lsv x2y1, 0x2, 0x12, ivtx1
  lsv x1y3, 0x0, 0x10, ivtx1
  lsv x1y3, 0x2, 0x12, ivtx3

  # x3 - x1, y2 - y1
  vsub vdelta1, x3y2, x1y1, VE_V
  # x2 - x1, y1 - y3
  vsub vdelta2, x2y1, x1y3, VE_V

  #   (x3 - x1) * (y2 - y1)
  vmudh cross, vdelta1, vdelta1, VE_1
  # + (x2 - x1) * (y1 - y3)
  vmadh cross, vdelta2, vdelta2, VE_1
  
  # If cross product is negative then this is a front face. Otherwise, a back face
  mfc2 flags, cross, 0x0
  slt is_back, flags, $0
  xori is_front, is_back, 1
  sll is_front, is_front, 1
  or flags, is_front, is_back
  and flags, flags, cull

  bne flags, $0, end

  #undef cull
  #undef is_back
  #undef is_front
  #undef flags

cull_triangle_pass:
  #define gmode $9
  #define do_zbuf $10
  #define do_tex $11
  #define do_shade $12
  #define tmp $13

  lh gmode, %lo(g_mode)

  # Check which geometry modes are enabled and add the corresponding sizes to output.
  andi do_zbuf,  gmode, 0x1
  andi do_tex,   gmode, 0x2
  andi do_shade, gmode, 0x4

  sll output_open_size, do_zbuf, 4            # 16 bytes (0x10)
  sll tmp, do_tex, 5
  add output_open_size, output_open_size, tmp # 64 bytes (0x40)
  sll tmp, do_shade, 4
  add output_open_size, output_open_size, tmp # 64 bytes (0x40)

  jal output_open
  addiu output_open_size, output_open_size, 0x20 # 32 bytes

  #undef tmp

  # Maximum size: 176 bytes (0xB0)

  #define vtx1 $13
  #define vtx2 $14
  #define vtx3 $15
  #define amin $v01
  #define amid $v02
  #define amax $v03
  #define vmin $v04
  #define vmid $v05
  #define vmax $v06
  #define vtm1 $v07
  #define vtm2 $v08
  #define atm1 $v09
  #define atm2 $v10

  lsv vmin, 0x0, 0x12, ivtx1
  lsv vmid, 0x0, 0x12, ivtx2
  lsv vmax, 0x0, 0x12, ivtx3

  # Put vertex addresses in a separate register so they can be sorted too
  mtc2 ivtx1, amin, 0x0
  mtc2 ivtx2, amid, 0x0
  mtc2 ivtx3, amax, 0x0

  # Perform sorting algorithm
  vge  vtm1, vmin, vmid, VE_V
  vmrg atm1, amin, amid, VE_V

  vlt  vmin, vmin, vmid, VE_V
  vmrg amin, amin, amid, VE_V

  vge  vtm2, vmin, vmax, VE_V
  vmrg atm2, amin, amax, VE_V

  vlt  vmin, vmin, vmax, VE_V
  vmrg amin, amin, amax, VE_V

  vge  vmax, vtm1, vtm2, VE_V
  vmrg amax, atm1, atm2, VE_V

  vlt  vmid, vtm1, vtm2, VE_V
  vmrg amid, atm1, atm2, VE_V

  # Get sorted addresses.
  # Vertices are now sorted, vtx1 < vtx2 < vtx3.
  mfc2 vtx1, amin, 0x0
  mfc2 vtx2, amid, 0x0
  mfc2 vtx3, amax, 0x0

  #define slopei $v01
  #define slopef $v02
  #define crosszi $v03
  #define crosszf $v04

  #define vfraci $v05
  #define vfracf $v06

  #define vllm $v07
  #define deltai $v08
  #define deltaf $v09

  #define vhmh $v10
  #define vtmp1 $v11
  #define mdelta $v12

  # Compute deltas for edge slopes and the cross product.
  #
  # BTW, if /dy is 0, the reciprocal calculated is the most positive
  # signed number possible. And lim x->0 1/x = + infinity.
  # So we do not need to check/handle division by zero...

  llv vhmh, 0x0, 0x10, vtx3
  llv vhmh, 0x4, 0x10, vtx2
  llv vhmh, 0x8, 0x10, vtx3
  llv vllm, 0x0, 0x10, vtx1
  llv vllm, 0x4, 0x10, vtx1
  llv vllm, 0x8, 0x10, vtx2

  # delta[0,1] = x3-x1, y3-y1 (hdy)
  # delta[2,3] = x2-x1, y2-y1 (mdy)
  # delta[4,5] = x3-x2, y3-y2 (ldy)

  vsub deltai, vhmh, vllm, VE_V
  vsub mdelta, vzero, deltai, VE_2

  # cz = hx * my - hy * mx
  vmudh vtmp1, deltai, deltai, VE_3 #   (x3 - x1) * (y2 - y1)
  vmadh vtmp1, mdelta, deltai, VE_1 # + (x1 - x2) * (y3 - y1)
  vsar crosszi, vtmp1, vtmp1, VE_0
  vsar crosszf, vtmp1, vtmp1, VE_1

  #define csign $16

  # when cz is negative, then lft = 1 (right major triangle)
  mfc2 csign, crosszi, 0x0

  vmudm deltai, deltai, vshr, VE_2
  vmadn deltaf, vzero, vzero, VE_0

  #define invi $v10
  #define invf $v11

  vrcph invi, VE_1, deltai, VE_1 # 1 / hdy
  vrcpl invf, VE_1, deltaf, VE_1
  vrcph invi, VE_1, deltai, VE_3 # 1 / mdy
  vrcpl invf, VE_3, deltaf, VE_3
  vrcph invi, VE_3, deltai, VE_5 # 1 / ldy
  vrcpl invf, VE_5, deltaf, VE_5
  vrcph invi, VE_5, vzero,  VE_0

  vmudn invf, invf, vshl, VE_1
  vmadh invi, invi, vshl, VE_1
  vmadn invf, vzero, vzero, VE_0

  vmudl slopef, deltaf, invf, VE_1q
  vmadm slopei, deltai, invf, VE_1q
  vmadn slopef, deltaf, invi, VE_1q
  vmadh slopei, deltai, invi, VE_1q

  #define ylmh $v12
  #define ymask $v13
  #define ysign $v14
  #define command $17
  #define lft $18
  #define tex $19

  lsv ylmh, 0x2, 0x12, vtx3
  lsv ylmh, 0x4, 0x12, vtx2
  lsv ylmh, 0x6, 0x12, vtx1

  # convert y-coords from s13.2 to s11.2
  # we need to extend the sign bit down 2 bits

  #define tmp $17

  addiu tmp, $0, 0xE000
  mtc2 tmp, ymask, 0x0

  #undef tmp

  vmudm ysign, ylmh,  vshr, VE_2  # Extend sign bit down (Multiply by 1/4)
  vand  ysign, ysign, ymask, VE_0 # Mask out everything except extended sign
  vnor  ymask, ymask, vzero, VE_0 # Invert bit mask
  vand  ylmh,  ylmh,  ymask, VE_0 # Get digits (Mask out high bits)
  vor   ylmh,  ylmh,  ysign, VE_V # Combine sign and digits again
  
  lbu tex, %lo(texture_settings)
  andi command, gmode, 0x7
  ori command, command, 0x8
  sll command, command, 8
  slt lft, csign, $0
  sll lft, lft, 7
  or command, command, lft
  or command, command, tex

  mtc2 command, ylmh, 0x0

  sdv ylmh, 0x0, 0x0, OUTPUT_POINTER

  #undef tex
  #undef lft
  #undef command
  #undef csign

  #define xcoordi $v12
  #define xcoordf $v13
  #define xoffi $v14
  #define xofff $v15
  #define vllmi $v16
  #define vllmf $v17

  # Shift everything right to convert from s13.2
  vmudm vllmi, vllm, vshr, VE_2
  vmadn vllmf, vzero, vzero, VE_0

  vsubc vfracf, vzero, vllmf, VE_V
  vsub  vfraci, vzero, vzero, VE_V

  #define vtmp2 $v18

  # Multiply the inverse slope with the y-offsets to get the x-offset
  # And subtract it from the x-coordinates
  vmudn vtmp2,   vllmf,  vshl, VE_0
  vmadh vtmp2,   vllmi,  vshl, VE_0
  vmadl vtmp2,   slopef, vfracf, VE_1q
  vmadm vtmp2,   slopei, vfracf, VE_1q
  vmadn xcoordf, slopef, vfraci, VE_1q
  vmadh xcoordi, slopei, vfraci, VE_1q

  #define tmp $20
  lh tmp, 0x10(vtx2)
  sll tmp, tmp, 14
  sw tmp, 0x8(OUTPUT_POINTER)

  # Save everything to output (edge coefficients)
  ssv slopei,  0x8, 0x0C, OUTPUT_POINTER # DxLDy.i
  ssv slopef,  0x8, 0x0E, OUTPUT_POINTER # DxLDy.f
  ssv xcoordi, 0x0, 0x10, OUTPUT_POINTER # XH.i
  ssv xcoordf, 0x0, 0x12, OUTPUT_POINTER # XH.f
  ssv slopei,  0x0, 0x14, OUTPUT_POINTER # DxHDy.i
  ssv slopef,  0x0, 0x16, OUTPUT_POINTER # DxHDy.f
  ssv xcoordi, 0x4, 0x18, OUTPUT_POINTER # XM.i
  ssv xcoordf, 0x4, 0x1A, OUTPUT_POINTER # XM.f
  ssv slopei,  0x4, 0x1C, OUTPUT_POINTER # DxMDy.i
  ssv slopef,  0x4, 0x1E, OUTPUT_POINTER # DxMDy.f

  andi tmp, gmode, 0x7
  beq tmp, $0, no_zbuf

  #undef tmp

  #define cdeltasi $v07
  #define cdeltasf $v08

  #define x1y1x2y1 $v09
  #define x3y3x1y2 $v10
  #define mczi $v28
  #define mczf $v27
  #define iczi $v26
  #define iczf $v25

  # Negate the z-component of the partial cross product computed earlier
  vor mczf, vzero, crosszf, VE_0
  vor mczi, vzero, crosszi, VE_0

  # Compute the reciprocal of the z-component (which acts as a scale for the whole cross product)
  # We need extra precision here, so call this function that implements the Newton-Raphson method
  # TODO: Inline this function
  jal newton_raphson
  addiu newton_iterations, $0, 1

  vmudn iczf, iczf, vshl, VE_2
  vmadh iczi, iczi, vshl, VE_2
  vmadn iczf, vzero, vzero, VE_0
  
  # TODO: Avoid these load instructions
  lsv x3y3x1y2, 0x0, 0x10, vtx3
  lsv x3y3x1y2, 0x2, 0x12, vtx1
  lsv x3y3x1y2, 0x4, 0x10, vtx2
  lsv x3y3x1y2, 0x6, 0x12, vtx1

  lsv x1y1x2y1, 0x0, 0x10, vtx1
  lsv x1y1x2y1, 0x2, 0x12, vtx3
  lsv x1y1x2y1, 0x4, 0x10, vtx1
  lsv x1y1x2y1, 0x6, 0x12, vtx2

  vsub cdeltasi, x3y3x1y2, x1y1x2y1, VE_V

  #define att1i $v09
  #define att1f $v10
  #define att2i $v11
  #define att2f $v12
  #define att3i $v13
  #define att3f $v14

  # R, G, B, A

  #define tmp $20

  vor att1f, vzero, vzero, VE_0
  andi tmp, gmode, 0x8
  vor att2f, vzero, vzero, VE_0
  beq tmp, $0, flat_shading
  vor att3f, vzero, vzero, VE_0

  #undef tmp

  luv att1i, 0x0, 0x18, vtx1
  luv att2i, 0x0, 0x18, vtx2
  j shading_done
  luv att3i, 0x0, 0x18, vtx3

flat_shading:
  # TODO: Load correct alpha values in flat shading
  luv att1i, 0x0, 0x18, ivtx1
  luv att2i, 0x0, 0x18, ivtx1
  luv att3i, 0x0, 0x18, ivtx1

shading_done:

  vmudm att1i, att1i, vshr, VE_7
  vmudm att2i, att2i, vshr, VE_7
  vmudm att3i, att3i, vshr, VE_7

  #define out_ptr $20

  # S, T, W
  beq do_tex, $0, no_tex_prep
  addiu out_ptr, OUTPUT_POINTER, 0x20

  # Determine largest value of 1/W
  #define invw1 $16
  #define invw2 $17
  #define invw3 $18
  #define tmp $19

  lw invw1, 0x20(vtx1)
  lw invw2, 0x20(vtx2)
  lw invw3, 0x20(vtx3)

  # Calculates MAX(vtx1, vtx2)
  sub tmp, invw1, invw2
  sra invw2, tmp, 31
  and tmp, tmp, invw2
  sub invw1, invw1, tmp

  # Calculates MAX(MAX(vtx1, vtx2), vtx3)
  sub tmp, invw1, invw3
  sra invw3, tmp, 31
  and tmp, tmp, invw3
  sub invw1, invw1, tmp

  # Store result temporarily
  sw invw1, 0x0(out_ptr)
  
  #define stw12i $v15
  #define stw12f $v16
  #define stw3i $v17
  #define stw3f $v18
  #define iwi $v19
  #define iwf $v20
  #define iwsi $v21
  #define iwsf $v22
  #define vtmp3 $v27

  llv iwsi, 0x0, 0x0, out_ptr

  # Normalize inverted depth (W)
  addiu tmp, $0, 0x7FFF
  mtc2 tmp, vtmp3, 0x0
  vor stw12i, vzero, vtmp3, VE_0
  vor stw3i,  vzero, vtmp3, VE_0

  llv stw12i, 0x0, 0x18, vtx1
  llv stw12i, 0x8, 0x18, vtx2
  llv stw3i, 0x0,  0x18, vtx3

  llv iwi, 0x0, 0x20, vtx1
  llv iwi, 0x8, 0x20, vtx2
  llv iwi, 0xC, 0x20, vtx3

  vor iwf, vzero, iwi, VE_1q

  # Divide all 1/W values by the maximum to scale them to range [0# 1]
  vrcph iwsi, VE_0, iwsi, VE_0
  vrcpl iwsf, VE_0, iwsi, VE_1
  vrcph iwsi, VE_0, vzero, VE_0
  vmudl iwf, iwf, iwsf, VE_0
  vmadm iwi, iwi, iwsf, VE_0
  vmadn iwf, iwf, iwsi, VE_0
  vmadh iwi, iwi, iwsi, VE_0

  vmudm stw12f, stw12i, iwf, VE_0h
  vmadh stw12i, stw12i, iwi, VE_0h
  vmadn stw12f, vzero, vzero, VE_0

  vmudm stw3f, stw3i, iwf, VE_6
  vmadh stw3i, stw3i, iwi, VE_6
  vmadn stw3f, vzero, vzero, VE_0

  sdv stw12i, 0x8, 0x0, out_ptr
  sdv stw12f, 0x8, 0x8, out_ptr
  ldv att2i, 0x0, 0x0, out_ptr
  ldv att2f, 0x0, 0x8, out_ptr

  addiu tmp, $0, 0x7
  ctc2 tmp, COP2_CTRL_VCC

  vmrg att1i, stw12i, att1i, VE_V
  vmrg att1f, stw12f, att1f, VE_V
  vmrg att3i, stw3i,  att3i, VE_V
  vmrg att3f, stw3f,  att3f, VE_V

  #undef tmp

no_tex_prep:

  lsv att1i, 0x6, 0x14, vtx1
  lsv att2i, 0x6, 0x14, vtx2
  lsv att3i, 0x6, 0x14, vtx3
  lsv att1f, 0x6, 0x16, vtx1
  lsv att2f, 0x6, 0x16, vtx2
  lsv att3f, 0x6, 0x16, vtx3

  #define dadei $v15
  #define dadef $v16
  #define dadxi $v17
  #define dadxf $v18
  #define dadyi $v19
  #define dadyf $v20

  #define atd1i $v21
  #define atd1f $v22
  #define atd2i $v23
  #define atd2f $v24

  vsubc atd1f, att2f, att1f, VE_V
  vsub  atd1i, att2i, att1i, VE_V

  vsubc atd2f, att1f, att3f, VE_V
  vsub  atd2i, att1i, att3i, VE_V

  # dadx = (a2 - a1) * (y3 - y1) +
  #        (a1 - a3) * (y2 - y1)
  vmudn vtmp3, atd1f, cdeltasi, VE_1
  vmadh vtmp3, atd1i, cdeltasi, VE_1
  vmadn vtmp3, atd2f, cdeltasi, VE_3
  vmadh vtmp3, atd2i, cdeltasi, VE_3
  vsar dadxi, vtmp3, vtmp3, VE_0
  vsar dadxf, vtmp3, vtmp3, VE_1

  # dadx /= cross product
  vmudl vtmp3, dadxf, iczf, VE_0
  vmadm vtmp3, dadxi, iczf, VE_0
  vmadn dadxf, dadxf, iczi, VE_0
  vmadh dadxi, dadxi, iczi, VE_0

  # dady = (a2 - a1) * (x3 - x1) +
  #        (a1 - a3) * (x1 - x2)
  vmudn vtmp3, atd1f, cdeltasi, VE_0
  vmadh vtmp3, atd1i, cdeltasi, VE_0
  vmadn vtmp3, atd2f, cdeltasi, VE_2
  vmadh vtmp3, atd2i, cdeltasi, VE_2
  vsar dadyi, vtmp3, vtmp3, VE_0
  vsar dadyf, vtmp3, vtmp3, VE_1

  # dady /= cross product
  vmudl vtmp3, dadyf, iczf,   VE_0
  vmadm vtmp3, dadyi, iczf,   VE_0
  vmadn dadyf, dadyf, iczi,   VE_0
  vmadh dadyi, dadyi, iczi,   VE_0
  
  # dade = dady + dadx * dxhdy
  vmadl vtmp3, dadxf, slopef, VE_0
  vmadm vtmp3, dadxi, slopef, VE_0
  vmadn dadef, dadxf, slopei, VE_0
  vmadh dadei, dadxi, slopei, VE_0

  # a1 -= dade * y1.f
  vmudn vtmp3, att1f, vshl,   VE_0 
  vmadh vtmp3, att1i, vshl,   VE_0
  vmadl vtmp3, dadef, vfracf, VE_1
  vmadm vtmp3, dadei, vfracf, VE_1
  vmadn att1f, dadef, vfraci, VE_1
  vmadh att1i, dadei, vfraci, VE_1

  beq do_shade, $0, no_shade
  nop
  
  sdv att1i, 0x8, 0x00, out_ptr
  sdv dadxi, 0x8, 0x08, out_ptr
  sdv att1f, 0x8, 0x10, out_ptr
  sdv dadxf, 0x8, 0x18, out_ptr
  sdv dadei, 0x8, 0x20, out_ptr
  sdv dadyi, 0x8, 0x28, out_ptr
  sdv dadef, 0x8, 0x30, out_ptr
  sdv dadyf, 0x8, 0x38, out_ptr

  addiu out_ptr, out_ptr, 0x40

no_shade:
  beq do_tex, $0, no_tex
  nop

  sdv att1i, 0x0, 0x00, out_ptr
  sdv dadxi, 0x0, 0x08, out_ptr
  sdv att1f, 0x0, 0x10, out_ptr
  sdv dadxf, 0x0, 0x18, out_ptr
  sdv dadei, 0x0, 0x20, out_ptr
  sdv dadyi, 0x0, 0x28, out_ptr
  sdv dadef, 0x0, 0x30, out_ptr
  sdv dadyf, 0x0, 0x38, out_ptr

  addiu out_ptr, out_ptr, 0x40

no_tex:
  beq do_zbuf, $0, no_zbuf
  nop

  ssv att1i, 0x6, 0x0, out_ptr
  ssv att1f, 0x6, 0x2, out_ptr
  ssv dadxi, 0x6, 0x4, out_ptr
  ssv dadxf, 0x6, 0x6, out_ptr
  ssv dadei, 0x6, 0x8, out_ptr
  ssv dadef, 0x6, 0xA, out_ptr
  ssv dadyi, 0x6, 0xC, out_ptr
  ssv dadyf, 0x6, 0xE, out_ptr

no_zbuf:
  jal output_close
  nop

end:
  jr return
  nop

  #undef out_ptr
