/**
 * This is an example of how to create a library that sits on top of rspq.
 * 
 * The "vec" library is a very simple vector transformation tool that
 * takes advantage of the RSP's vector opcodes. This is done by creating
 * an rspq overlay (see "rsp_vec.S") which implements the vector math on the RSP
 * side, and defining some helper functions that serve as the CPU-side
 * interface to that overlay.
 * 
 * Note that this is purely meant to show off the features of the rspq system
 * and is *not* meant to be an optimal vector math implementation. In reality,
 * such a system would likely be implemented very differently.
 * 
 * The overlay offers three commands: load, store, and transform.
 * The load/store commands will basically just perform a DMA that transfers
 * vector data between RDRAM and DMEM.
 * 
 * Vectors are organized into "slots". A single slot consists of 8 vector components
 * (which can be interpreted as two 4-component vectors), each of which has an 
 * integer and a fractional part. The #vec_slot_t struct shows the memory layout of such a slot.
 * 
 * The overlay can hold up to 16 slots in DMEM. The load and store commands work
 * on the basis of slots, so you specify at which slot to start the transfer, and
 * how many slots should be transferred.
 * 
 * The heart of the system is the "transform" command. It takes the slot index of
 * a "matrix" and that of a vector and will transform that vector with the matrix,
 * storing the output to another slot. This works by interpreting two slots as the
 * columns of a 4x4 matrix, which will act on two 4 component vectors. Multiplying
 * two matrices together can also be done by transforming the columns of the right
 * hand side matrix with the left hand matrix in two steps: columns 0 and 1 first,
 * then columnds 2 and 3.
 * 
 * This header also offers some convenience functions that will convert float arrays
 * to and from the slot format.
 */

#ifndef VEC_H
#define VEC_H

#include <stdint.h>
#include <libdragon.h>

enum {
    VEC_CMD_LOAD  = 0x0,
    VEC_CMD_STORE = 0x1,
    VEC_CMD_TRANS = 0x2,
};

typedef struct {
    int16_t i[8];
    uint16_t f[8];
} __attribute__((aligned(16))) vec_slot_t;

typedef struct {
    vec_slot_t c[2];
} __attribute__((aligned(16))) vec_mtx_t;

void vec_init();
void vec_close();

static inline void vec_load(uint32_t slot, vec_slot_t *src, uint32_t num)
{
    extern uint32_t vec_id;
    rspq_write(vec_id, VEC_CMD_LOAD,
        PhysicalAddr(src) & 0xFFFFFF,
        (((num*sizeof(vec_slot_t)-1)&0xFFF)<<16) | ((slot*sizeof(vec_slot_t))&0xFF0));
}

static inline void vec_store(vec_slot_t *dest, uint32_t slot, uint32_t num)
{
    extern uint32_t vec_id;
    rspq_write(vec_id, VEC_CMD_STORE,
        PhysicalAddr(dest) & 0xFFFFFF,
        (((num*sizeof(vec_slot_t)-1)&0xFFF)<<16) | ((slot*sizeof(vec_slot_t))&0xFF0));
}

static inline void vec_transform(uint32_t dest, uint32_t mtx, uint32_t vec)
{
    extern uint32_t vec_id;
    rspq_write(vec_id, VEC_CMD_TRANS,
        (dest*sizeof(vec_slot_t))&0xFF0,
        (((mtx*sizeof(vec_slot_t))&0xFF0)<<16) | ((vec*sizeof(vec_slot_t))&0xFF0));
}

void floats_to_vectors(vec_slot_t *dest, const float *source, uint32_t source_length);
void vectors_to_floats(float *dest, const vec_slot_t *source, uint32_t dest_length);

#endif
