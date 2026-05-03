############################################################
# Magma clipping ucode sub-overlay
#
# Authors: Giovanni Bajo <giovannibajo@gmail.com>
#          Dennis Heinze <dennis.heinze@mailbox.org>
#
############################################################
#include <rsp_magma.inc>

_MgDefineUniformRegion

_MgBeginShader
    # We also need to bootstrap the shader here, so that it gets reloaded when required after clipping is done.
    _MgBootstrapShader

# IMPORTANT: We want to define the actual clipping routine instead of the bootstrapper that is contained in "MgEndShader",
#            which is why that routine is omitted here deliberately.

Mg_Clipping:
    #define vtx1            a0
    #define vtx2            a2
    #define vtx3            a1
    #define clip_flags      t5
    #define out_count       v1
    #define plane_flag      t6
    #define in_count        t7
    #define in_end          t8
    #define in_list         s0
    #define out_list        s1
    #define plane           s2
    #define intersection    s3
    #define cur_ptr         s4
    #define prev_ptr        s5
    #define cur_vtx         s6
    #define prev_vtx        s7
    #define p0              k0
    #define p1              k1

    #define vplane          $v01
    #define vint_f          $v02
    #define vint_i          $v03
    #define vdot_i          $v04
    #define vdot_f          $v05
    #define vdiff_i         $v06
    #define vdiff_f         $v07
    #define va_i            $v08
    #define va_f            $v09
    #define vpos_i          $v10
    #define vpos_f          $v11
    #define vattr0          $v12
    #define vattr1          $v13
    #define voff0           $v14
    #define voff1           $v15
    #define vcache0         $v16
    #define vcache1         $v17
    #define vclip_factors   $v18
    #define vviewscale      $v19
    #define vviewoff        $v22
    #define v___            $v29

    srl clip_flags, t3, 8
    li in_list, %lo(MG_CLIP_LIST1)

    # Copy the three original vertices to the out_list
    # (after the initial swap they will be in the in_list)
    li out_list, %lo(MG_CLIP_LIST0)
    li out_count, 3*2
    addi t0, rspq_dmem_buf_ptr, %lo(RSPQ_DMEM_BUFFER) - 6
    ldv v___, 0,t0
    sdv v___, 0,out_list

    li plane, %lo(MG_CLIP_PLANES)
    li plane_flag, 1

    # Load cache offsets    
    li t0, %lo(MG_CLIP_CACHE_OFFSETS)
    vxor voff1, voff1
    lqv voff0,  0,t0
    lsv voff1, 16,t0

    # Temporarily use the output buffer as a map of which cache slots are used
    # Init to zero
    li t0, %lo(MG_CLIP_CACHE_USED)
    sqv vzero,  0,t0
    sqv vzero, 16,t0

    li t0, %lo(MG_VIEWPORT)
    ldv vviewscale, 0,t0
    ldv vviewoff,   8,t0

    li t0, %lo(MG_CLIP_FACTORS)
    ldv vclip_factors.e0, 0x0,t0
    ldv vclip_factors.e4, 0x0,t0

    # Iterate over the 6 clipping planes
mg_clip_plane_loop:
    and t0, clip_flags, plane_flag
    beqz t0, mg_clip_plane_loop_end
    move t1, in_list

    # Swap in and out lists

    # If the out list is empty from the last iteration, 
    # the triangle has no visible points and we are done
    beqz out_count, RSPQ_Loop
    move in_list, out_list
    move out_list, t1
    move in_count, out_count
    move out_count, zero

    # Repeat plane coefficients twice
    ldv vplane.e0,  0,plane
    ldv vplane.e4,  0,plane

    vmudh vplane, vclip_factors

    # Iterate over the egdes of the polygon in the input list
    # The current edge is between cur_vtx and prev_vtx
    move cur_ptr, in_list
    add in_end, in_list, in_count
    # Init the "previous" vertex to the last in the list for the wrap-around
    addi prev_ptr, in_end, -2

mg_clip_edge_loop:
    #define cur_flag  t3
    #define prev_flag t4

    # Check which side of the plane the two vertices are on
    lhu cur_vtx, 0(cur_ptr)
    lhu prev_vtx, 0(prev_ptr)
    lbu cur_flag, MG_VTX_CLIP_CODE(cur_vtx)
    lbu prev_flag, MG_VTX_CLIP_CODE(prev_vtx)
    and cur_flag, plane_flag
    and prev_flag, plane_flag

    # If they are on opposite sides, there is an intersection
    xor t0, cur_flag, prev_flag
    beqz t0, mg_clip_no_intersection
    move p0, cur_vtx

    # Swap the two points if necessary to make intersection calculation consistent
    # This will make sure p0 is always inside and p1 is always outside
    bnez prev_flag, 1f
    move p1, prev_vtx
    xor p0, p0, p1
    xor p1, p0, p1
    xor p0, p0, p1

    #undef prev_flag

1:
    # Calculate intersection of the line segment and the plane

    li t0, %lo(MG_CLIP_CACHE_USED)
    lqv vcache0,    0,t0
    lqv vcache1,   16,t0

    # vpos: x0  y0  z0  w0  x1  y1  z1  w1
    ldv vpos_i.e0,  MG_VTX_CS_POSi,p0
    ldv vpos_f.e0,  MG_VTX_CS_POSf,p0
    ldv vpos_i.e4,  MG_VTX_CS_POSi,p1
    ldv vpos_f.e4,  MG_VTX_CS_POSf,p1

    # vint: x1  y1  z1  w1
    ldv vint_i.e0,  MG_VTX_CS_POSi,p1
    ldv vint_f.e0,  MG_VTX_CS_POSf,p1

    # vattr0: r0  g0  b0  a0  s0  t0
    luv vattr0.e0,  MG_VTX_RGBA   ,p0
    llv vattr0.e4,  MG_VTX_ST     ,p0

    # vattr1: r1  g1  b1  a1  s1  t1
    luv vattr1.e0,  MG_VTX_RGBA   ,p1
    llv vattr1.e4,  MG_VTX_ST     ,p1

    # Find first free slot in clip cache

    # Add the values from the "used slots map" to the cache offsets
    # After this, each lane will contain the offset of its corresponding cache slot,
    # but only if the slot is not used. If it is used, it will contain some large value.
    vaddc vcache0, voff0
    vaddc vcache1, voff1

    # Look for the smallest value, which will end up in vcache.e0
    # Because used slots are marked as large values, they will never be found.
    vlt vcache0, vcache0.q1
    vlt vcache0, vcache0.h2
    vlt vcache0, vcache0.e4
    vlt vcache0, vcache1.e0

    mfc2 t0, vcache0.e0

    # Mark slot as used by storing some large value (careful of overflows!)
    li t1, 0xFF
    sh t1, %lo(MG_CLIP_CACHE_USED)-2(t0)

    # t0 is the index multiplied by 2
    # intersection = t0 * 20 = t0 * 16 + t0 * 4
    sll intersection, t0, 4
    sll t1, t0, 2
    add intersection, t1

    # CAUTION: intersection might point to the same address as either p0 or p1,
    # because one of them is the previous point, which could have been marked unused
    # in the previous iteration. As long as we don't access p0 or p1 after writing to
    # intersection, this is fine.
    addi intersection, %lo(MG_CLIP_CACHE) - MG_VTX_SIZE

    # Store the cache offset in unused memory (used later when finding the cache slot to mark as unused)
    sb t0, MG_VTX_TR_CODE(intersection)

    # Compute dot products of both positions with the clip plane
    # vdot.e0: d0 = dot(p0, plane)
    # vdot.e4: d1 = dot(p1, plane)
    vmudn vdot_f, vpos_f, vplane
    vmadh vdot_i, vpos_i, vplane
    vaddc vdot_f, vdot_f.q1
    vadd  vdot_i, vdot_i.q1
    vaddc vdot_f, vdot_f.h2
    vadd  vdot_i, vdot_i.h2

    # d0 - d1
    vsubc vdiff_f, vdot_f, vdot_f.e4
    vsub  vdiff_i, vdot_i, vdot_i.e4

    # 1 / (d0 - d1)
    vrcph v___.e0,  vdiff_i.e0
    vrcpl va_f.e0, vdiff_f.e0
    vrcph va_i.e0, vzero.e0

    # a = d0 / (d0 - d1)
    vmudl v___, va_f, vdot_f.e0
    vmadm v___, va_i, vdot_f.e0
    vmadn va_f, va_f, vdot_i.e0

    # Prepare 0x7FFF in va_i.e0
    vsubc va_i, vshift8, K1

    # a = min(a, 1)
    vge  v___, va_f, vzero
    vmrg va_f, va_f, va_i.e0

    # Account for right shift introduced by vrcp
    vmudn va_f, va_f, K2

    # p1 - p0
    vsubc vint_f, vpos_f
    vsub  vint_i, vpos_i
    # attr1 - attr0
    vsubc vattr1, vattr0

    # Result of linear interpolation:
    # p0 + a * (p1 - p0)
    vmudl v___,   vint_f, va_f.e0
    vmadm v___,   vint_i, va_f.e0
    vmadn vint_f, vpos_f, K1
    vmadh vint_i, vpos_i, K1

    # a * (attr1 - attr0)
    vmudm vattr1, vattr1, va_f.e0

    # attr0 + a * (attr1 - attr0)
    vaddc vattr0, vattr1

    # Store results
    sdv vint_i.e0,  MG_VTX_CS_POSi,intersection
    sdv vint_f.e0,  MG_VTX_CS_POSf,intersection
    suv vattr0.e0,  MG_VTX_RGBA   ,intersection
    slv vattr0.e4,  MG_VTX_ST     ,intersection

    # Re-calculate clip codes
    vmudn vint_f, vclip_factors
    vmadh vint_i, vclip_factors
    vch v___, vint_i, vint_i.h3
    vcl v___, vint_f, vint_f.h3
    cfc2 t0, COP2_CTRL_VCC
    andi t0, 0x707
    srl t1, t0, 5
    or t0, t1
    # Set the MSB of clip codes to 1 to mark this vertex as used
    ori t0, 0x80
    sb t0, MG_VTX_CLIP_CODE(intersection)

    # Add intersection to the output list
    add t0, out_list, out_count
    sh intersection, 0(t0)
    addi out_count, 2

mg_clip_no_intersection:
    # If cur_vtx is inside, add it to the output list
    bnez cur_flag, 1f
    add t0, out_list, out_count
    sh cur_vtx, 0(t0)
    b mg_clip_edge_loop_end
    addi out_count, 2

    #undef cur_flag

1:
    # Check if the vertex is stored in the clip cache
    lb t0, MG_VTX_CLIP_CODE(cur_vtx)
    bgez t0, mg_clip_edge_loop_end
    lb t1, MG_VTX_TR_CODE(cur_vtx)

    # Set the MSB of the clip codes to 0 to mark this vertex as unused
    andi t0, 0x7F
    sb t0, MG_VTX_CLIP_CODE(cur_vtx)
    # Also mark it as unused within the cache so it can be found again when searching for unused vertices
    sh zero, %lo(MG_CLIP_CACHE_USED)-2(t1)
    
mg_clip_edge_loop_end:
    # Advance to the next edge
    addi cur_ptr, 2
    blt cur_ptr, in_end, mg_clip_edge_loop
    addi prev_ptr, cur_ptr, -2

mg_clip_plane_loop_end:
    # Advance to the next clipping plane
    sll plane_flag, 1
    blt plane_flag, (1<<MG_CLIP_PLANE_COUNT), mg_clip_plane_loop
    addi plane, MG_CLIP_PLANE_SIZE

    #define cache_vtx   s3
    #define cache_end   s5
    #define clip_codes  t1

    # Calculate screen space values for new vertices (in the clip cache)
    # TODO: maybe iterate over out_list instead
    li cache_vtx, %lo(MG_CLIP_CACHE)
    li cache_end, %lo(MG_CLIP_CACHE_END) - MG_VTX_SIZE
    li clip_codes, 0x00FF
mg_clip_finalize_loop:
    lb t0, MG_VTX_CLIP_CODE(cache_vtx)

    # Only calculate screen space values if the vertex is actually used (MSB of clip codes is set)
    beqz t0, mg_clip_finalize_loop_end
    ldv vint_i,  MG_VTX_CS_POSi,cache_vtx
    ldv vint_f,  MG_VTX_CS_POSf,cache_vtx

    vmudl v___,   vint_f, vviewscale.h3
    vmadm vint_i, vint_i, vviewscale.h3
    vmadn vint_f, vzero, vzero

    ssv vint_i.e3, MG_VTX_Wi,cache_vtx
    ssv vint_f.e3, MG_VTX_Wf,cache_vtx

    vrcph v___.e3,   vint_i.e3
    vrcpl vint_f.e3, vint_f.e3
    vrcph vint_i.e3, vzero.e0

    ssv vint_i.e3, MG_VTX_INVWi,cache_vtx
    ssv vint_f.e3, MG_VTX_INVWf,cache_vtx

    vmudl v___,   vint_f, vint_f.h3
    vmadm v___,   vint_i, vint_f.h3
    vmadn vint_f, vint_f, vint_i.h3
    vmadh vint_i, vint_i, vint_i.h3

    vmudh v___, vviewoff, K1
    vmadn vint_f, vint_f, vviewscale
    vmadh vint_i, vint_i, vviewscale

    sdv vint_i, MG_VTX_XYZ,cache_vtx
    sh clip_codes, MG_VTX_CLIP_CODE(cache_vtx)

mg_clip_finalize_loop_end:
    blt cache_vtx, cache_end, mg_clip_finalize_loop
    addi cache_vtx, MG_VTX_SIZE

    #undef cache_vtx
    #undef cache_end
    #undef clip_codes

    #undef clip_flags
    #undef plane_flag
    #undef in_count
    #undef in_end
    #undef in_list
    #undef plane
    #undef intersection
    #undef cur_ptr
    #undef prev_ptr
    #undef cur_vtx
    #undef prev_vtx
    #undef p0
    #undef p1
    #undef vplane
    #undef vint_f
    #undef vint_i
    #undef vdot_i
    #undef vdot_f
    #undef vdiff_i
    #undef vdiff_f
    #undef va_i
    #undef va_f
    #undef vpos_i
    #undef vpos_f
    #undef vattr0
    #undef vattr1
    #undef voff0
    #undef voff1
    #undef vcache0
    #undef vcache1
    #undef vclip_factors
    #undef vviewscale
    #undef vviewoff
    #undef v___

    #define out_ptr s2
    #define out_end k0

    lbu t7, %lo(MG_OUTPUT_OFFSET)
    lbu v0, %lo(MG_CULL_MODE)

    move out_ptr, out_list
    add out_end, out_list, out_count
    addi out_end, -4
mg_draw_clipped_triangles_loop:
    bge out_ptr, out_end, RSPQ_Loop
    lhu a3, %lo(MG_TRI_CMD)
    lhu vtx1, 0(out_list)
    lhu vtx2, 2(out_ptr)
    lhu vtx3, 4(out_ptr)
    li sp, %lo(tri_cull)
    jal RDPQ_Triangle_Send_Async
    addi s3, t7, %lo(MG_OUTPUT_BUFFER)

    xor t7, TRIANGLE_SIZE
    sb t7, %lo(MG_OUTPUT_OFFSET)

tri_cull:
    b mg_draw_clipped_triangles_loop
    addi out_ptr, 2
    #undef out_list
    #undef out_count
    #undef vtx1
    #undef vtx2
    #undef vtx3
