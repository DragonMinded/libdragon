#include <regs.S>

/**
 * @file ucode.h
 * @brief Macros for RSP assembler
 */
#ifndef __UCODE_H
#define __UCODE_H

/** @brief Syntactic sugar for lwc2 instuctions */
.macro loadVector vt, combined, base
    lwc2 \vt, \combined (\base)
.endm

/** @brief Load Byte into Vector Register */
#define LBV_OPCODE 0b00000
/** @brief Load Double into Vector Register */
#define LDV_OPCODE 0b00011
/** @brief Load Packed Fourth into Vector Register */
#define LFV_OPCODE 0b01001
/** @brief Load Packed Half into Vector Register */
#define LHV_OPCODE 0b01000
/** @brief Load Long into Vector Register */
#define LLV_OPCODE 0b00010
/** @brief Load Packed Bytes into Vector Register */
#define LPV_OPCODE 0b00110
/** @brief Load Quad into Vector Register */
#define LQV_OPCODE 0b00110
/** @brief Load Quad (Rest) into Vector Register */
#define LRV_OPCODE 0b00101
/** @brief Load Short into Vector Register */
#define LSV_OPCODE 0b00001
/** @brief Load Transpose into Vector Register */
#define LTV_OPCODE 0b01011
/** @brief Load Unsigned Packed into Vector Register */
#define LUV_OPCODE 0b00111

/** @brief Generate offset for lwc2 from opcode, element and offset */
#define COMBINED_OFFSET(opcode, element, offset) (opcode << 11 | element << 7 | offset)

/** @brief Generate LBV offset for lwc2 from element and offset */
#define LBV(element, offset) COMBINED_OFFSET(LBV_OPCODE, element, offset)
/** @brief Generate LDV offset for lwc2 from element and offset */
#define LDV(element, offset) COMBINED_OFFSET(LDV_OPCODE, element, offset)
/** @brief Generate LFV offset for lwc2 from element and offset */
#define LFV(element, offset) COMBINED_OFFSET(LFV_OPCODE, element, offset)
/** @brief Generate LHV offset for lwc2 from element and offset */
#define LHV(element, offset) COMBINED_OFFSET(LHV_OPCODE, element, offset)
/** @brief Generate LLV offset for lwc2 from element and offset */
#define LLV(element, offset) COMBINED_OFFSET(LLV_OPCODE, element, offset)
/** @brief Generate LPV offset for lwc2 from element and offset */
#define LPV(element, offset) COMBINED_OFFSET(LPV_OPCODE, element, offset)
/** @brief Generate LQV offset for lwc2 from element and offset */
#define LQV(element, offset) COMBINED_OFFSET(LQV_OPCODE, element, offset)
/** @brief Generate LRV offset for lwc2 from element and offset */
#define LRV(element, offset) COMBINED_OFFSET(LRV_OPCODE, element, offset)
/** @brief Generate LSV offset for lwc2 from element and offset */
#define LSV(element, offset) COMBINED_OFFSET(LSV_OPCODE, element, offset)
/** @brief Generate LTV offset for lwc2 from element and offset */
#define LTV(element, offset) COMBINED_OFFSET(LTV_OPCODE, element, offset)
/** @brief Generate LUV offset for lwc2 from element and offset */
#define LUV(element, offset) COMBINED_OFFSET(LUV_OPCODE, element, offset)

#endif
