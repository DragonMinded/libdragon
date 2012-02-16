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
 * @brief Structure representing a register block
 *
 * DO NOT modify the order unless editing inthandler.S
 */
typedef volatile struct
{
    /** @brief General purpose registers 1-32 */
	volatile uint64_t gpr[32];
    /** @brief SR */
	volatile uint32_t sr;
    /** @brief EPC */
	volatile uint32_t epc;
    /** @brief HI */
	volatile uint64_t hi;
    /** @brief LO */
	volatile uint64_t lo;
    /** @brief FC31 */
	volatile uint64_t fc31;
    /** @brief Floating point registers 1-32 */
	volatile uint64_t fpr[32];
} reg_block_t;

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
    /** @brief String information of exception */
	const char* info;
    /** @brief Registers at point of exception */
	volatile const reg_block_t* regs;
} exception_t;

/** @} */

#ifdef __cplusplus
extern "C" {
#endif

void register_exception_handler( void (*cb)(exception_t *) );

#ifdef __cplusplus
}
#endif

#endif
