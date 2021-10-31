/**
 * @file exception.h
 * @brief Exception Handler
 * @ingroup exceptions
 */
#ifndef __LIBDRAGON_EXCEPTION_H
#define __LIBDRAGON_EXCEPTION_H

#include <stdint.h>

/**
 * @addtogroup exceptions
 * @{
 */

/**
 * @brief Exception types
 */
enum
{
    /** @brief Unknown exception */
    EXCEPTION_TYPE_UNKNOWN = 0,
    /** @brief Reset exception */
    EXCEPTION_TYPE_RESET,
    /** @brief Critical exception */
    EXCEPTION_TYPE_CRITICAL
};

/**
 * @brief Exception codes
 */
typedef enum {
    EXCEPTION_CODE_INTERRUPT = 0,
    EXCEPTION_CODE_TLB_MODIFICATION = 1,
    EXCEPTION_CODE_TLB_LOAD_I_MISS = 2,
    EXCEPTION_CODE_TLB_STORE_MISS = 3,
    EXCEPTION_CODE_LOAD_I_ADDRESS_ERROR = 4,
    EXCEPTION_CODE_STORE_ADDRESS_ERROR = 5,
    EXCEPTION_CODE_I_BUS_ERROR = 6,
    EXCEPTION_CODE_D_BUS_ERROR = 7,
    EXCEPTION_CODE_SYS_CALL = 8,
    EXCEPTION_CODE_BREAKPOINT = 9,
    EXCEPTION_CODE_RESERVED_INSTRUCTION = 10,
    EXCEPTION_CODE_COPROCESSOR_UNUSABLE = 11,
    EXCEPTION_CODE_ARITHMETIC_OVERFLOW = 12,
    EXCEPTION_CODE_TRAP = 13,
    EXCEPTION_CODE_FLOATING_POINT = 15,
    EXCEPTION_CODE_WATCH = 23,
} exception_code_t;

/**
 * @brief Structure representing a register block
 *
 * DO NOT modify the order unless editing inthandler.S
 */
typedef struct __attribute__((packed))
{
    /** @brief General purpose registers 1-32 */
    uint64_t gpr[32];
    /** @brief HI */
    uint64_t hi;
    /** @brief LO */
    uint64_t lo;
    /** @brief SR */
    uint32_t sr;
    /** @brief CR (NOTE: can't modify this from an exception handler) */
    uint32_t cr;
    /**
     * @brief represents EPC - COP0 register $14
     *
     * The coprocessor 0 (system control coprocessor - COP0) register $14 is the
     * return from exception program counter. For asynchronous exceptions it points
     * to the place to continue execution whereas for synchronous (caused by code)
     * exceptions, point to the instruction causing the fault condition, which
     * needs correction in the exception handler. This member is for reading/writing
     * its value.
     * */
    uint32_t epc;
    /** @brief FC31 */
    uint32_t fc31;
    /** @brief Floating point registers 1-32 */
    uint64_t fpr[32];
} reg_block_t;

/* Make sure the structure has the right size. Please keep this in sync with inthandler.S */
_Static_assert(sizeof(reg_block_t) == 544, "invalid reg_block_t size -- this must match inthandler.S");

/**
 * @brief Structure representing an exception
 */
typedef struct
{
    /** 
     * @brief Exception type
     * @see #EXCEPTION_TYPE_RESET, #EXCEPTION_TYPE_CRITICAL
     */
    int32_t type;
    /**
     * @brief Underlying exception code
     */
    exception_code_t code;
    /** @brief String information of exception */
    const char* info;
    /** @brief Registers at point of exception */
    volatile reg_block_t* regs;
} exception_t;


/** 
 * @brief Guaranteed length of the reset time.
 * 
 * This is the guaranteed length of the reset time, that is the time
 * that goes between the user pressing the reset button, and the CPU actually
 * resetting. See #exception_reset_time for more details.
 * 
 * @note The general knowledge about this is that the reset time should be
 *       500 ms. Testing on different consoles show that, while most seem to
 *       reset after 500 ms, a few EU models reset after 200ms. So we define
 *       the timer shorter for greater compatibility.
 */
#define RESET_TIME_LENGTH      TICKS_FROM_MS(200)

/** @} */

#ifdef __cplusplus
extern "C" {
#endif

void register_exception_handler( void (*cb)(exception_t *) );
void exception_default_handler( exception_t* ex );

void register_reset_handler( void (*cb)(void) );
uint32_t exception_reset_time( void );

#ifdef __cplusplus
}
#endif

#endif
