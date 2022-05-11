#include <rsp_queue.inc>

#define SLOT_SIZE 0x20
#define NUM_SLOTS 0x20

    .set noreorder
    .set at

    .data

RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand VecCmd_Load,         8
    RSPQ_DefineCommand VecCmd_Store,        8
    RSPQ_DefineCommand VecCmd_Transform,    8
RSPQ_EndOverlayHeader

RSPQ_BeginSavedState
    .align 4
VEC_SLOTS: .ds.b NUM_SLOTS * SLOT_SIZE
RSPQ_EndSavedState

    .text

VecCmd_Load:
    j Vec_DMA
    li t2, DMA_IN
    
VecCmd_Store:
    li t2, DMA_OUT

Vec_DMA:
    andi s4, a1, 0xFFF0
    addiu s4, %lo(VEC_SLOTS)
    srl t0, a1, 16
    j DMAExec
    move s0, a0

VecCmd_Transform:
    #define trans_mtx t0
    #define trans_vec t1
    #define trans_out t2
    #define m0i $v01
    #define m0f $v02
    #define m1i $v03
    #define m1f $v04
    #define m2i $v05
    #define m2f $v06
    #define m3i $v07
    #define m3f $v08
    #define v01i $v09
    #define v01f $v10
    #define o01i $v13
    #define o01f $v14
    #define vtemp $v17

    srl trans_mtx, a1, 16
    andi trans_mtx, 0xFF0
    andi trans_vec, a1, 0xFF0
    andi trans_out, a0, 0xFF0

    addiu trans_mtx, %lo(VEC_SLOTS)
    addiu trans_vec, %lo(VEC_SLOTS)
    addiu trans_out, %lo(VEC_SLOTS)

    # Load matrix columns, repeating each column twice in a register
    ldv m0i,0x0,  0x00,trans_mtx
    ldv m0i,0x8,  0x00,trans_mtx
    ldv m0f,0x0,  0x10,trans_mtx
    ldv m0f,0x8,  0x10,trans_mtx
    ldv m1i,0x0,  0x08,trans_mtx
    ldv m1i,0x8,  0x08,trans_mtx
    ldv m1f,0x0,  0x18,trans_mtx
    ldv m1f,0x8,  0x18,trans_mtx
    ldv m2i,0x0,  0x20,trans_mtx
    ldv m2i,0x8,  0x20,trans_mtx
    ldv m2f,0x0,  0x30,trans_mtx
    ldv m2f,0x8,  0x30,trans_mtx
    ldv m3i,0x0,  0x28,trans_mtx
    ldv m3i,0x8,  0x28,trans_mtx
    ldv m3f,0x0,  0x38,trans_mtx
    ldv m3f,0x8,  0x38,trans_mtx

    # Load vector (a slot always contains two vectors)
    lqv v01i,0x0,  0x00,trans_vec
    lqv v01f,0x0,  0x10,trans_vec

    # Perform transformation by computing the dot products of the matrix rows with the vector.
    # We take advantage of the accumulator by letting it perform the addition automatically
    # with each multiplication.

    vmudl vtemp, m0f, v01f,e(0h) #   m(x,0) * v(0)
    vmadm vtemp, m0i, v01f,e(0h)
    vmadn vtemp, m0f, v01i,e(0h)
    vmadh vtemp, m0i, v01i,e(0h)

    vmadl vtemp, m1f, v01f,e(1h) # + m(x,1) * v(1)
    vmadm vtemp, m1i, v01f,e(1h)
    vmadn vtemp, m1f, v01i,e(1h)
    vmadh vtemp, m1i, v01i,e(1h)

    vmadl vtemp, m2f, v01f,e(2h) # + m(x,2) * v(2)
    vmadm vtemp, m2i, v01f,e(2h)
    vmadn vtemp, m2f, v01i,e(2h)
    vmadh vtemp, m2i, v01i,e(2h)

    vmadl vtemp, m3f, v01f,e(3h) # + m(x,3) * v(3)
    vmadm vtemp, m3i, v01f,e(3h)
    vmadn o01f,  m3f, v01i,e(3h)
    vmadh o01i,  m3i, v01i,e(3h)

    # Write result
    sqv o01i,0x0,  0x00,trans_out
    jr ra
    sqv o01f,0x0,  0x10,trans_out
    
    #undef trans_mtx
    #undef trans_vec
    #undef trans_out
    #undef m0i
    #undef m0f
    #undef m1i
    #undef m1f
    #undef m2i
    #undef m2f
    #undef m3i
    #undef m3f
    #undef v01i
    #undef v01f
    #undef o01i
    #undef o01f
    #undef vtemp

