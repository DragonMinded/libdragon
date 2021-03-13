/**
 * @file cop0.h
 * @brief N64 COP0 Interface
 * @ingroup n64sys
 */

/**
 * @addtogroup n64sys
 * @{
 */

#ifndef __LIBDRAGON_COP0_H
#define __LIBDRAGON_COP0_H

/** @brief Read the COP0 Count register (see also TICKS_READ). */
#define C0_COUNT() ({ \
    uint32_t x; \
    asm volatile("mfc0 %0,$9":"=r"(x)); \
    x; \
})

/** @brief Write the COP0 Count register. */
#define C0_WRITE_COUNT(x) ({ \
    asm volatile("mtc0 %0,$9"::"r"(x)); \
})


/** @brief Read the COP0 Compare register. */
#define C0_COMPARE() ({ \
    uint32_t x; \
    asm volatile("mfc0 %0,$11":"=r"(x)); \
    x; \
})

/** @brief Write the COP0 Compare register. */
#define C0_WRITE_COMPARE(x) ({ \
    asm volatile("mtc0 %0,$11"::"r"(x)); \
})


/** @brief Read the COP0 Status register. */
#define C0_STATUS() ({ \
    uint32_t x; \
    asm volatile("mfc0 %0,$12":"=r"(x)); \
    x; \
})

/** @brief Write the COP0 Status register. */
#define C0_WRITE_STATUS(x) ({ \
    asm volatile("mtc0 %0,$12"::"r"(x)); \
})

/**
 * @brief Returns the COP0 register $13 (Cause Register)
 *
 * The coprocessor 0 (system control coprocessor - COP0) register $13 is a read
 * write register keeping pending interrupts, exception code, coprocessor unit
 * number referenced for a coprocessor unusable exception.
 *
 * @see #C0_GET_CAUSE_EXC_CODE, #C0_GET_CAUSE_CE and #C0_CAUSE_BD
 */
#define C0_READ_CR() ({ \
	uint32_t x; \
	asm volatile("mfc0 %0,$13" : "=r" (x) : ); \
	x; \
})

/**
 * @brief Write the COP0 register $13 (Cause register)
 * 
 * Use this to update it for a custom exception handler.
 * */
#define C0_WRITE_CR(x) ({ \
    asm volatile("mtc0 %0,$13"::"r"(x)); \
})

/**
 * @brief Returns the COP0 register $8 (BadVAddr)
 *
 * The coprocessor 0 (system control coprocessor - COP0) register $8 is a read
 * only register holding the last virtual address to be translated which became
 * invalid, or a virtual address for which an addressing error occurred.
 */
#define C0_READ_BADVADDR() ({ \
	uint32_t x; \
	asm volatile("mfc0 %0,$8" : "=r" (x) : ); \
	x; \
})

/**
 * @brief Read the COP0 register $14 (EPC)
 *
 * The coprocessor 0 (system control coprocessor - COP0) register $14 is the
 * return from exception program counter. For asynchronous exceptions it points
 * to the place to continue execution whereas for synchronous (caused by code)
 * exceptions, point to the instruction causing the fault condition, which
 * needs correction in the exception handler. This macro is for reading its
 * value.
 */
#define C0_READ_EPC() ({ \
	uint32_t x; \
	asm volatile("mfc0 %0,$14" : "=r" (x) : ); \
	x; \
})

/* COP0 Status bits definition. Please refer to MIPS R4300 manual. */
#define C0_STATUS_IE        0x00000001
#define C0_STATUS_EXL       0x00000002
#define C0_STATUS_ERL       0x00000004

/* COP0 Cause bits definition. Please refer to MIPS R4300 manual. */
#define C0_CAUSE_BD         0x80000000
#define C0_CAUSE_CE         0x30000000
#define C0_CAUSE_EXC_CODE   0x0000007C

/* COP0 interrupt bits definition. These are compatible bothwith mask and pending bits. */
#define C0_INTERRUPT_0      0x00000100
#define C0_INTERRUPT_1      0x00000200
#define C0_INTERRUPT_RCP    0x00000400 // RCP
#define C0_INTERRUPT_3      0x00000800
#define C0_INTERRUPT_4      0x00001000
#define C0_INTERRUPT_5      0x00002000
#define C0_INTERRUPT_6      0x00004000
#define C0_INTERRUPT_TIMER  0x00008000 // Timer

/**
 * @brief Get the CE value from the COP0 status register
 *
 * Gets the Coprocessor unit number referenced by a coprocessor unusable
 * exception from the given COP0 Status register value.
 */
#define C0_GET_CAUSE_CE(cr) (((cr) & C0_CAUSE_CE) >> 28)

/**
 * @brief Get the exception code value from the COP0 status register value
 */
#define C0_GET_CAUSE_EXC_CODE(sr) (((sr) & C0_CAUSE_EXC_CODE) >> 2)

/** @} */

#endif