/**
 * @file COP1.h
 * @brief N64 COP1 Interface
 * @ingroup n64sys
 * 
 * The coprocessor 1 (COP1) is implemented as a floating point unit (FPU)
 */

/**
 * @addtogroup n64sys
 * @{
 */

#ifndef __LIBDRAGON_COP1_H
#define __LIBDRAGON_COP1_H

/* COP1 Control/Status bits definition. Please refer to MIPS R4300 manual. */
#define C1_FLAG_INEXACT_OP          0x00000004
#define C1_FLAG_UNDERFLOW           0x00000008
#define C1_FLAG_OVERFLOW            0x00000010
#define C1_FLAG_DIV_BY_0            0x00000020
#define C1_FLAG_INVALID_OP          0x00000040

#define C1_ENABLE_INEXACT_OP        0x00000080
#define C1_ENABLE_UNDERFLOW         0x00000100
#define C1_ENABLE_OVERFLOW          0x00000200
#define C1_ENABLE_DIV_BY_0          0x00000400
#define C1_ENABLE_INVALID_OP        0x00000800

#define C1_CAUSE_INEXACT_OP         0x00001000
#define C1_CAUSE_UNDERFLOW          0x00002000
#define C1_CAUSE_OVERFLOW           0x00004000
#define C1_CAUSE_DIV_BY_0           0x00008000
#define C1_CAUSE_INVALID_OP         0x00010000
#define C1_CAUSE_NOT_IMPLEMENTED    0x00020000

/** @brief Read the COP1 FCR31 register (floating-point control register 31)
 *
 * FCR31 is also known as the Control/Status register. It keeps control and
 * status data for the FPU.
 */
#define C1_FCR31() ({ \
    uint32_t x; \
    asm volatile("cfc1 %0,$f31":"=r"(x)); \
    x; \
})

/** @brief Write to the COP1 FCR31 register. */
#define C1_WRITE_FCR31(x) ({ \
    asm volatile("ctc1 %0,$f31"::"r"(x)); \
})

/** @} */

#endif