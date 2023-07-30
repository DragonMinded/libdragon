#ifndef __GL_RSP_ASM
#define __GL_RSP_ASM

#include <stdint.h>

typedef enum {
    VLOAD_BYTE      = 0b00000,
    VLOAD_HALF      = 0b00001,
    VLOAD_LONG      = 0b00010,
    VLOAD_DOUBLE    = 0b00011,
    VLOAD_QUAD      = 0b00100
} vload_size_t;

#define LBU  0b100100
#define LW   0b100011
#define LWC2 0b110010
#define ADDI 0b001000

inline uint32_t rsp_asm_lwc2(vload_size_t size, uint8_t dst_vreg, uint8_t element, uint16_t offset, uint8_t base_reg)
{
    return (LWC2 << 26) | (base_reg << 21) | (dst_vreg << 16) | (size << 11) | (element << 7) | offset;
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

#endif
