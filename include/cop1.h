/**
 * @file cop1.h
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
#define C1_FLAG_INEXACT_OP          0x00000004         ///< Flag recording inexact operation
#define C1_FLAG_UNDERFLOW           0x00000008         ///< Flag recording underflow
#define C1_FLAG_OVERFLOW            0x00000010         ///< Flag recording overflow
#define C1_FLAG_DIV_BY_0            0x00000020         ///< Flag recording division by zero
#define C1_FLAG_INVALID_OP          0x00000040         ///< Flag recording invalid operation

#define C1_ENABLE_INEXACT_OP        0x00000080         ///< Enable inexact operation exception
#define C1_ENABLE_UNDERFLOW         0x00000100         ///< Enable underflow exception
#define C1_ENABLE_OVERFLOW          0x00000200         ///< Enable overflow exception
#define C1_ENABLE_DIV_BY_0          0x00000400         ///< Enable division by zero exception
#define C1_ENABLE_INVALID_OP        0x00000800         ///< Enable invalid operation exception
#define C1_ENABLE_MASK              0x00000F80         ///< Mask for all enable bits

#define C1_CAUSE_INEXACT_OP         0x00001000         ///< Triggered inexact operation exception
#define C1_CAUSE_UNDERFLOW          0x00002000         ///< Triggered underflow exception
#define C1_CAUSE_OVERFLOW           0x00004000         ///< Triggered overflow exception
#define C1_CAUSE_DIV_BY_0           0x00008000         ///< Triggered division by zero exception
#define C1_CAUSE_INVALID_OP         0x00010000         ///< Triggered invalid operation exception
#define C1_CAUSE_NOT_IMPLEMENTED    0x00020000         ///< Triggered not implemented exception
#define C1_CAUSE_MASK               0x0003F000         ///< Mask for all cause bits

#define C1_FCR31_FS                 (1<<24)            ///< Flush denormals to zero/min

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
