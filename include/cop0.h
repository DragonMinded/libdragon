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

/* COP0 Status bits definition. Please refer to MIPS R4300 manual. */
#define C0_STATUS_IE        0x00000001
#define C0_STATUS_EXL       0x00000002
#define C0_STATUS_ERL       0x00000004

#define C0_STATUS_IM0       0x00000100
#define C0_STATUS_IM1       0x00000200
#define C0_STATUS_IM2       0x00000400
#define C0_STATUS_IM3       0x00000800
#define C0_STATUS_IM4       0x00001000
#define C0_STATUS_IM5       0x00002000
#define C0_STATUS_IM6       0x00004000
#define C0_STATUS_IM7       0x00008000

/** @} */

#endif