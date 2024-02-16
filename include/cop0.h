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

#include <stdint.h>

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
#define C0_CAUSE() ({ \
	uint32_t x; \
	asm volatile("mfc0 %0,$13" : "=r" (x) : ); \
	x; \
})

/**
 * @brief Write the COP0 register $13 (Cause register)
 * 
 * Use this to update it for a custom exception handler.
 * */
#define C0_WRITE_CAUSE(x) ({ \
    asm volatile("mtc0 %0,$13"::"r"(x)); \
})

/** @cond */
/* Alternative version with different naming */
#define C0_CR()        C0_CAUSE()
#define C0_WRITE_CR(x) C0_WRITE_CAUSE(x)
/** @endcond */

/**
 * @brief Returns the COP0 register $8 (BadVAddr)
 *
 * The coprocessor 0 (system control coprocessor - COP0) register $8 is a read
 * only register holding the last virtual address to be translated which became
 * invalid, or a virtual address for which an addressing error occurred.
 */
#define C0_BADVADDR() ({ \
	uint64_t x; \
	asm volatile("dmfc0 %0,$8" : "=r" (x) : ); \
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
#define C0_EPC() ({ \
	uint32_t x; \
	asm volatile("mfc0 %0,$14" : "=r" (x) : ); \
	x; \
})

/**
 * @brief Read the COP0 INDEX register
 * 
 * This register is used during TLB programming. It holds the index of the TLB
 * entry being accessed (0-31).
 */
#define C0_INDEX() ({ \
    uint32_t x; \
    asm volatile("mfc0 %0,$0":"=r"(x)); \
    x; \
})

/**
 * @brief Write the COP0 INDEX register
 * 
 * This register is used during TLB programming. It holds the index of the TLB
 * entry being accessed (0-31).
 */
#define C0_WRITE_INDEX(x)    asm volatile("mtc0 %0,$0; nop; nop"::"r"(x))


/**
 * @brief Read the COP0 ENTRYHI register
 * 
 * This register is used during TLB programming. It holds the configuration
 * of the virtual memory entry for the TLB slot being accessed.
 */
#define C0_ENTRYHI() ({ \
    uint32_t x; \
    asm volatile("mfc0 %0,$10":"=r"(x)); \
    x; \
})

/**
 * @brief Write the COP0 ENTRYHI register
 * 
 * This register is used during TLB programming.
 */
#define C0_WRITE_ENTRYHI(x)  asm volatile("mtc0 %0,$10; nop; nop"::"r"(x))

/**
 * @brief Read the COP0 ENTRYLO0 register
 * 
 * This register is used during TLB programming. It holds the configuration
 * of the physical memory entry (even bank) for the TLB slot being accessed.
 */
#define C0_ENTRYLO0() ({ \
    uint32_t x; \
    asm volatile("mfc0 %0,$2":"=r"(x)); \
    x; \
})

/**
 * @brief Write the COP0 ENTRYLO0 register
 * 
 * This register is used during TLB programming. It holds the configuration
 * of the physical memory entry (even bank) for the TLB slot being accessed.
 */
#define C0_WRITE_ENTRYLO0(x) asm volatile("mtc0 %0,$2; nop; nop"::"r"(x))

/**
 * @brief Read the COP0 ENTRYLO1 register
 * 
 * This register is used during TLB programming. It holds the configuration
 * of the physical memory entry (odd bank) for the TLB slot being accessed.
 */
#define C0_ENTRYLO1() ({ \
    uint32_t x; \
    asm volatile("mfc0 %0,$3":"=r"(x)); \
    x; \
})

/**
 * @brief Write the COP0 ENTRYLO1 register
 * 
 * This register is used during TLB programming. It holds the configuration
 * of the physical memory entry (even bank) for the TLB slot being accessed.
 */
#define C0_WRITE_ENTRYLO1(x) asm volatile("mtc0 %0,$3; nop; nop"::"r"(x))


/**
 * @brief Read the COP0 PAGEMASK register
 * 
 * This register is used during TLB programming. It holds the bitmask that
 * configures the page size of the TLB slot being accessed.
 */
#define C0_PAGEMASK() ({ \
    uint32_t x; \
    asm volatile("mfc0 %0,$5":"=r"(x)); \
    x; \
})

/**
 * @brief Write the COP0 PAGEMASK register
 * 
 * This register is used during TLB programming. It holds the bitmask that
 * configures the page size of the TLB slot being accessed.
 */
#define C0_WRITE_PAGEMASK(x) asm volatile("mtc0 %0,$5; nop; nop"::"r"(x))


/**
 * @brief Read the COP0 WIRED register
 * 
 * This register is used during TLB programming. It allows to partition TLB
 * slots between fixed slots and random slots. The fixed slot pool is the
 * range [0..WIRED[ and the random pool is the range [WIRED..32[
 */
#define C0_WIRED() ({ \
    uint32_t x; \
    asm volatile("mfc0 %0,$6":"=r"(x)); \
    x; \
})

/**
 * @brief Read the COP0 WATCHLO register
 * 
 * This register is used during watchpoint programming. It allows to trigger
 * an exception when a memory access occurs on a specific memory location.
 */
#define C0_WATCHLO() ({ \
    uint32_t x; \
    asm volatile("mfc0 %0,$18":"=r"(x)); \
    x; \
})

/**
 * @brief Write the COP0 WATCHLO register
 * 
 * This register is used during watchpoint programming. It allows to trigger
 * an exception when a memory access occurs on a specific memory location.
 */
#define C0_WRITE_WATCHLO(x) asm volatile("mtc0 %0,$18"::"r"(x))

/**
 * @brief Write the COP0 WIRED register
 * 
 * This register is used during TLB programming. It allows to partition TLB
 * slots between fixed slots and random slots. The fixed slot pool is the
 * range [0..WIRED[ and the random pool is the range [WIRED..32[
 */
#define C0_WRITE_WIRED(x) asm volatile("mtc0 %0,$6; nop; nop"::"r"(x))

/** @cond */
/* Deprecated version of macros with wrong naming that include "READ" */
#define C0_READ_CR()         C0_CAUSE()
#define C0_READ_EPC()        C0_EPC()
#define C0_READ_BADVADDR()   C0_BADVADDR()
/** @endcond */

/* COP0 Status bits definition. Please refer to MIPS R4300 manual. */
#define C0_STATUS_IE        0x00000001      ///< Status: interrupt enable
#define C0_STATUS_EXL       0x00000002      ///< Status: within exception 
#define C0_STATUS_ERL       0x00000004      ///< Status: within error

/* COP0 Cause bits definition. Please refer to MIPS R4300 manual. */
#define C0_CAUSE_BD         0x80000000      ///< Cause: exception triggered in delay slot
#define C0_CAUSE_CE         0x30000000      ///< Cause: coprocessor exception
#define C0_CAUSE_EXC_CODE   0x0000007C      ///< Cause: exception code

/* COP0 interrupt bits definition. These are compatible bothwith mask and pending bits. */
#define C0_INTERRUPT_0      0x00000100      ///< Status/Cause: SW interrupt 0
#define C0_INTERRUPT_1      0x00000200      ///< Status/Cause: SW interrupt 1
#define C0_INTERRUPT_2      0x00000400      ///< Status/Cause: HW interrupt 2 (RCP)
#define C0_INTERRUPT_3      0x00000800      ///< Status/Cause: HW interrupt 3 (CART)
#define C0_INTERRUPT_4      0x00001000      ///< Status/Cause: HW interrupt 4 (PRENMI)
#define C0_INTERRUPT_5      0x00002000      ///< Status/Cause: HW interrupt 5
#define C0_INTERRUPT_6      0x00004000      ///< Status/Cause: HW interrupt 6
#define C0_INTERRUPT_7      0x00008000      ///< Status/Cause: HW interrupt 7 (Timer)

#define C0_INTERRUPT_RCP    C0_INTERRUPT_2  ///< Status/Cause: HW interrupt 2 (RCP)
#define C0_INTERRUPT_CART   C0_INTERRUPT_3  ///< Status/Cause: HW interrupt 3 (CART)
#define C0_INTERRUPT_PRENMI C0_INTERRUPT_4  ///< Status/Cause: HW interrupt 4 (PRENMI)
#define C0_INTERRUPT_TIMER  C0_INTERRUPT_7  ///< Status/Cause: HW interrupt 7 (Timer)

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

/* Flag bits valid for COP0 ENTRYLO0/ENTRYLO1 registers */
#define C0_ENTRYLO_GLOBAL      (1<<0)       ///< ENTRYLO: mapping is global (all ASIDs)
#define C0_ENTRYLO_VALID       (1<<1)       ///< ENTRYLO: mapping is active (not disabled)
#define C0_ENTRYLO_DIRTY       (1<<2)       ///< ENTRYLO: mapping is writable

/* Flag bits valid for COP0 INDEX register */
#define C0_INDEX_PROBE_FAILED  (1<<31)      ///< INDEX: set when a TLBP probe failed to find a match


/**
 * @brief COP0 TLBWI opcode.
 * 
 * This opcode is used during TLB programming. It writes the TLB slot referenced
 * by INDEX with the contents of PAGEMASK, ENTRYHI, ENTRYLO0, ENTRYLO1.
 */
#define C0_TLBWI()           asm volatile("tlbwi; nop; nop; nop; nop")

/**
 * @brief COP0 TLBWR opcode.
 * 
 * This opcode is used during TLB programming. It writes a random TLB slot with
 * the contents of PAGEMASK, ENTRYHI, ENTRYLO0, ENTRYLO1. THe slot is selected
 * in the random pool (slots in the range from WIRED to 31).
 */
#define C0_TLBWR()           asm volatile("tlbwr; nop; nop; nop; nop")

/**
 * @brief COP0 TLBR opcode.
 * 
 * This opcode is used during TLB programming. It reads the contents of the TLB
 * slot referenced by INDEX into the registers PAGEMASK, ENTRYHI, ENTRYLO0 and
 * ENTRYLO1.
 */
#define C0_TLBR()            asm volatile("tlbr; nop; nop; nop; nop")

/**
 * @brief COP0 TLBP opcode.
 * 
 * This opcode is used during TLB programming. It probes the current TLB slots
 * using ENTRYHI (virtual address) to find a matching slot. If it finds, it
 * loads its index into INDEX. Otherwise, it sets the C0_INDEX_PROBE_FAILED bit
 * in INDEX.
 */
#define C0_TLBP()            asm volatile("tlbp; nop; nop; nop; nop")

/** @} */

#endif