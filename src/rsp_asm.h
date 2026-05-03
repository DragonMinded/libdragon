#ifndef __RSP_ASM
#define __RSP_ASM

#include <stdint.h>

typedef enum {
    VLOAD_BYTE      = 0b00000,
    VLOAD_HALF      = 0b00001,
    VLOAD_LONG      = 0b00010,
    VLOAD_DOUBLE    = 0b00011,
    VLOAD_QUAD      = 0b00100,
    VLOAD_REST      = 0b00101,
    VLOAD_PACK      = 0b00110,
    VLOAD_UPACK     = 0b00111,
    VLOAD_HPACK     = 0b01000,
    VLOAD_FPACK     = 0b01001,
    VLOAD_TRANSPOSE = 0b01011,
} vload_size_t;

#define LB   0b100000
#define LH   0b100001
#define LW   0b100011
#define LBU  0b100100
#define LHU  0b100101
#define LWU  0b100111
#define LWC2 0b110010
#define ADDI 0b001000
#define COP2 0b010010

#define VOR  0b101010

inline uint32_t rsp_asm_lwc2(vload_size_t size, uint8_t dst_vreg, uint8_t element, uint16_t offset, uint8_t base_reg)
{
    return (LWC2 << 26) | (base_reg << 21) | (dst_vreg << 16) | (size << 11) | (element << 7) | offset;
}

inline uint32_t rsp_asm_llv(uint8_t dst_vreg, uint8_t element, uint16_t offset, uint8_t base_reg)
{
    return rsp_asm_lwc2(VLOAD_LONG, dst_vreg, element, offset, base_reg);
}

inline uint32_t rsp_asm_lbu(uint8_t dst_reg, uint16_t offset, uint8_t base_reg)
{
    return (LBU << 26) | (base_reg << 21) | (dst_reg << 16) | offset;
}

inline uint32_t rsp_asm_lw(uint8_t dst_reg, uint16_t offset, uint8_t base_reg)
{
    return (LW << 26) | (base_reg << 21) | (dst_reg << 16) | offset;
}

inline uint32_t rsp_asm_addi(uint8_t rt_reg, uint8_t rs_reg, uint16_t immediate)
{
    return (ADDI << 26) | (rs_reg << 21) | (rt_reg << 16) | immediate;
}

inline uint32_t rsp_asm_vector_op(uint8_t op, uint8_t vd_reg, uint8_t vs_reg, uint8_t vt_reg, uint8_t element)
{
    return (COP2 << 26) | (1<<25) | (element << 21) | (vt_reg << 16) | (vs_reg << 11) | (vd_reg << 6) | op;
}

inline uint32_t rsp_asm_vor(uint8_t vd_reg, uint8_t vs_reg, uint8_t vt_reg, uint8_t element)
{
    return rsp_asm_vector_op(VOR, vd_reg, vs_reg, vt_reg, element);
}

inline uint32_t rsp_asm_vcopy(uint8_t vd_reg, uint8_t vs_reg)
{
    return rsp_asm_vor(vd_reg, 0, vs_reg, 0);
}

#endif
