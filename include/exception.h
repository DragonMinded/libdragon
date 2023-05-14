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
    EXCEPTION_TYPE_CRITICAL,
    /** @brief Syscall exception*/
    EXCEPTION_TYPE_SYSCALL,
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
    reg_block_t* regs;
} exception_t;

/** @} */

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * @brief Generic exception handler
 * 
 * This is the type of a handler that can be registered using #register_exception_handler.
 * It is associated to all unhandled exceptions that are not otherwise handled by libdragon.
 * 
 * @param exc Exception information
 */
typedef void (*exception_handler_t)(exception_t *exc);

/** 
 * @brief Syscall handler
 * 
 * This is the type of a handler of a syscall exception.
 * 
 * @param exc Exception information
 * @param code Syscall code
 */
typedef void (*syscall_handler_t)(exception_t *exc, uint32_t code);

exception_handler_t register_exception_handler( exception_handler_t cb );
void exception_default_handler( exception_t* ex );

void register_syscall_handler( syscall_handler_t cb, uint32_t first_code, uint32_t last_code );

#ifdef __cplusplus
}
#endif

#endif
