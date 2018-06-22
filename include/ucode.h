#include "ucode.S"

/**
 * @file ucode.h
 * @brief Macros for RSP assembler
 */
#ifndef __UCODE_H
#define __UCODE_H

#define s0 $0
#define s1 $1
#define s2 $2
#define s3 $3
#define s4 $4
#define s5 $5
#define s6 $6
#define s7 $7
#define s8 $8
#define s9 $9
#define s10 $10
#define s11 $11
#define s12 $12
#define s13 $13
#define s14 $14
#define s15 $15
#define s16 $16
#define s17 $17
#define s18 $18
#define s19 $19
#define s20 $20
#define s21 $21
#define s22 $22
#define s23 $23
#define s24 $24
#define s25 $25
#define s26 $26
#define s27 $27
#define s28 $28
#define s29 $29
#define s30 $30
#define s31 $31

/** @brief Load/Store Byte into/from Vector Register */
#define BV_OPCODE 0b00000
/** @brief Load/Store Double into/from Vector Register */
#define DV_OPCODE 0b00011
/** @brief Load/Store Packed Fourth into/from Vector Register */
#define FV_OPCODE 0b01001
/** @brief Load/Store Packed Half into/from Vector Register */
#define HV_OPCODE 0b01000
/** @brief Load/Store Long into/from Vector Register */
#define LV_OPCODE 0b00010
/** @brief Load/Store Packed Bytes into/from Vector Register */
#define PV_OPCODE 0b00110
/** @brief Load/Store Quad into/from Vector Register */
#define QV_OPCODE 0b00100
/** @brief Load/Store Quad (Rest) into/from Vector Register */
#define RV_OPCODE 0b00101
/** @brief Load/Store Short into/from Vector Register */
#define SV_OPCODE 0b00001
/** @brief Load/Store Transpose into/from Vector Register */
#define TV_OPCODE 0b01011
/** @brief Load/Store Unsigned Packed into/from Vector Register */
#define UV_OPCODE 0b00111
/** @brief Store Wrapped vector from Vector Register */
#define WV_OPCODE 0b00111

/** @brief Vector Absolute Value of Short Elements */
#define ABS_OPCODE 0b010011
/** @brief Vector Add of Short Elements */
#define ADD_OPCODE 0b010000
/** @brief Vector Add Short Elements With Carry */
#define ADDC_OPCODE 0b010100
/** @brief Vector AND of Short Elements */
#define AND_OPCODE 0b010100
/** @brief Vector Select Clip Test High */
#define CH_OPCODE 0b100101
/** @brief Vector Select Clip Test Low */
#define CL_OPCODE 0b100100
/** @brief Vector Select Crimp Test Low */
#define CR_OPCODE 0b100110
/** @brief Vector Select Equal */
#define EQ_OPCODE 0b100001
/** @brief Vector Select Greater Than or Equal */
#define GE_OPCODE 0b100011
/** @brief Vector Select Less Than */
#define LT_OPCODE 0b100000
/** @brief Vector Multiply-Accumulate of Signed Fractions */
#define MACF_OPCODE 0b001000
/** @brief Vector Accumulator Oddification */
#define MACQ_OPCODE 0b001011
/** @brief Vector Multiply-Accumulate of Unsigned Fractions */
#define MACU_OPCODE 0b001001
/** @brief Vector Multiply-Accumulate of High Partial Products */
#define MADH_OPCODE 0b001111
/** @brief Vector Multiply-Accumulate of Low Partial Products */
#define MADL_OPCODE 0b001100
/** @brief Vector Multiply-Accumulate of Mid Partial Products */
#define MADM_OPCODE 0b001101
/** @brief Vector Multiply-Accumulate of Mid Partial Products */
#define MADN_OPCODE 0b001110
/** @brief Vector Element Scalar Move */
#define MOV_OPCODE 0b110011
/** @brief Vector Select Merge */
#define MRG_OPCODE 0b100111
/** @brief Vector Multiply of High Partial Products */
#define MUDH_OPCODE 0b000111
/** @brief Vector Multiply of Low Partial Products */
#define MUDL_OPCODE 0b000100
/** @brief Vector Multiply of Mid Partial Products */
#define MUDM_OPCODE 0b000101
/** @brief Vector Multiply of Mid Partial Products */
#define MUDN_OPCODE 0b000110
/** @brief Vector Multiply of Signed Fractions */
#define MULF_OPCODE 0b000000
/** @brief Vector Multiply MPEG Quantization */
#define MULQ_OPCODE 0b000011
/** @brief Vector Multiply of Unsigned Fractions */
#define MULU_OPCODE 0b000001
/** @brief Vector NAND of Short Elements */
#define NAND_OPCODE 0b101001
/** @brief Vector Select Not Equal */
#define LT_OPCODE 0b100010
/** @brief Vector Null Instruction */
#define NOP_OPCODE 0b110111
/** @brief Vector NOR of Short Elements */
#define NOR_OPCODE 0b101011
/** @brief Vector NXOR of Short Elements */
#define NXOR_OPCODE 0b101101
/** @brief Vector NXOR of Short Elements */
#define OR_OPCODE 0b101010
/** @brief Vector Element Scalar Reciprocal (Single Precision) */
#define RCP_OPCODE 0b110000
/** @brief Vector Element Scalar Reciprocal (Double Prec. High) */
#define RCPH_OPCODE 0b110010
/** @brief Vector Element Scalar Reciprocal (Double Prec. Low) */
#define RCPL_OPCODE 0b110001
/** @brief Vector Accumulator DCT Rounding (Negative) */
#define RNDN_OPCODE 0b001010
/** @brief Vector Accumulator DCT Rounding (Positive) */
#define RNDP_OPCODE 0b000010
/** @brief Vector Element Scalar SQRT Reciprocal */
#define RSQ_OPCODE 0b110100
/** @brief Vector Element Scalar SQRT Reciprocal (Double Prec. High) */
#define RSQH_OPCODE 0b110110
/** @brief Vector Element Scalar SQRT Reciprocal (Double Prec. Low) */
#define RSQL_OPCODE 0b110101
/** @brief Vector Accumulator Read (and Write) */
#define SAR_OPCODE 0b011101
/** @brief Vector Subtraction of Short Elements */
#define SUB_OPCODE 0b010001
/** @brief Vector Subtraction of Short Elements With Carry */
#define SUBC_OPCODE 0b010101
/** @brief Vector XOR of Short Elements */
#define XOR_OPCODE 0b101100

/** @brief Generate offset for lwc2/swc2 from opcode, element and offset */
#define COMBINED_OFFSET(opcode, element, offset) (opcode << 11 | element << 7 | offset)

/** @brief Generate func for cop2 from opcode, element, vt, vs and vd */
#define COPROCESSOR_OP(opcode, element, vt, vs, vd) (element << 21 | vt << 16 | vs << 11 | vd << 6 | opcode)

/** @brief Generate LBV/SBV offset for lwc2/svc2 from element and offset */
#define BV(element, offset) COMBINED_OFFSET(BV_OPCODE, element, offset)
/** @brief Generate LDV/SDV offset for lwc2/svc2 from element and offset */
#define DV(element, offset) COMBINED_OFFSET(DV_OPCODE, element, offset)
/** @brief Generate LFV/SFV offset for lwc2/svc2 from element and offset */
#define FV(element, offset) COMBINED_OFFSET(FV_OPCODE, element, offset)
/** @brief Generate LHV/SHV offset for lwc2/svc2 from element and offset */
#define HV(element, offset) COMBINED_OFFSET(HV_OPCODE, element, offset)
/** @brief Generate LLV/SLV offset for lwc2/svc2 from element and offset */
#define LV(element, offset) COMBINED_OFFSET(LV_OPCODE, element, offset)
/** @brief Generate LPV/SPV offset for lwc2/svc2 from element and offset */
#define PV(element, offset) COMBINED_OFFSET(PV_OPCODE, element, offset)
/** @brief Generate LQV/SQV offset for lwc2/svc2 from element and offset */
#define QV(element, offset) COMBINED_OFFSET(QV_OPCODE, element, offset)
/** @brief Generate LRV/SRV offset for lwc2/svc2 from element and offset */
#define RV(element, offset) COMBINED_OFFSET(RV_OPCODE, element, offset)
/** @brief Generate LSV/SSV offset for lwc2/svc2 from element and offset */
#define SV(element, offset) COMBINED_OFFSET(SV_OPCODE, element, offset)
/** @brief Generate LTV/STV offset for lwc2/svc2 from element and offset */
#define TV(element, offset) COMBINED_OFFSET(TV_OPCODE, element, offset)
/** @brief Generate LUV/SUV offset for lwc2/svc2 from element and offset */
#define UV(element, offset) COMBINED_OFFSET(UV_OPCODE, element, offset)
/** @brief Generate SWV offset for swc2 from element and offset */
#define WV(element, offset) COMBINED_OFFSET(UV_OPCODE, element, offset)

/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define ABS(element, vt, vs, vd) COPROCESSOR_OP(ABS_OPCODE, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define ADD(element, vt, vs, vd) COPROCESSOR_OP(ADD_OPCODE, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define ADDC(element, vt, vs, vd) COPROCESSOR_OP(ADDC_OPCODE, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define AND(element, vt, vs, vd) COPROCESSOR_OP(AND, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define CH(element, vt, vs, vd) COPROCESSOR_OP(CH, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define CL(element, vt, vs, vd) COPROCESSOR_OP(CL, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define CR(element, vt, vs, vd) COPROCESSOR_OP(CR, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define EQ(element, vt, vs, vd) COPROCESSOR_OP(EQ, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define GE(element, vt, vs, vd) COPROCESSOR_OP(GE, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define LT(element, vt, vs, vd) COPROCESSOR_OP(LT, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define MACF(element, vt, vs, vd) COPROCESSOR_OP(MACF, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define MACQ(element, vt, vs, vd) COPROCESSOR_OP(MACQ, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define MACU(element, vt, vs, vd) COPROCESSOR_OP(MACU, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define MADH(element, vt, vs, vd) COPROCESSOR_OP(MADH, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define MADL(element, vt, vs, vd) COPROCESSOR_OP(MADL, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define MADM(element, vt, vs, vd) COPROCESSOR_OP(MADM, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define MADN(element, vt, vs, vd) COPROCESSOR_OP(MADN, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define MOV(element, vt, vs, vd) COPROCESSOR_OP(MOV, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define MRG(element, vt, vs, vd) COPROCESSOR_OP(MRG, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define MUDH(element, vt, vs, vd) COPROCESSOR_OP(MUDH, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define MUDL(element, vt, vs, vd) COPROCESSOR_OP(MUDL, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define MUDM(element, vt, vs, vd) COPROCESSOR_OP(MUDM, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define MUDN(element, vt, vs, vd) COPROCESSOR_OP(MUDN, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define MULF(element, vt, vs, vd) COPROCESSOR_OP(MULF, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define MULQ(element, vt, vs, vd) COPROCESSOR_OP(MULQ, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define MULU(element, vt, vs, vd) COPROCESSOR_OP(MULU, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define NAND(element, vt, vs, vd) COPROCESSOR_OP(NAND, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define LT(element, vt, vs, vd) COPROCESSOR_OP(LT, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define NOP(element, vt, vs, vd) COPROCESSOR_OP(NOP, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define NOR(element, vt, vs, vd) COPROCESSOR_OP(NOR, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define NXOR(element, vt, vs, vd) COPROCESSOR_OP(NXOR, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define OR(element, vt, vs, vd) COPROCESSOR_OP(OR, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define RCP(element, vt, vs, vd) COPROCESSOR_OP(RCP, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define RCPH(element, vt, vs, vd) COPROCESSOR_OP(RCPH, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define RCPL(element, vt, vs, vd) COPROCESSOR_OP(RCPL, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define RNDN(element, vt, vs, vd) COPROCESSOR_OP(RNDN, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define RNDP(element, vt, vs, vd) COPROCESSOR_OP(RNDP, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define RSQ(element, vt, vs, vd) COPROCESSOR_OP(RSQ, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define RSQH(element, vt, vs, vd) COPROCESSOR_OP(RSQH, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define RSQL(element, vt, vs, vd) COPROCESSOR_OP(RSQL, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define SAR(element, vt, vs, vd) COPROCESSOR_OP(SAR, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define SUB(element, vt, vs, vd) COPROCESSOR_OP(SUB, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define SUBC(element, vt, vs, vd) COPROCESSOR_OP(SUBC, element, vt, vs, vd)
/** @brief Generate byte sequence for cop2 from opcode, element, vt, vs and vd*/
#define XOR(element, vt, vs, vd) COPROCESSOR_OP(XOR, element, vt, vs, vd)

#endif
