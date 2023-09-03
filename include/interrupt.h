/**
 * @file interrupt.h
 * @brief Interrupt Controller
 * @ingroup interrupt
 */
#ifndef __LIBDRAGON_INTERRUPT_H
#define __LIBDRAGON_INTERRUPT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup interrupt
 * @{
 */

/**
 * @brief State of interrupts on the system
 */
typedef enum
{
    /** @brief Interrupt controller has not been initialized */
    INTERRUPTS_UNINITIALIZED,
    /** @brief Interrupts are currently disabled */
    INTERRUPTS_DISABLED,
    /** @brief Interrupts are currently enabled */
    INTERRUPTS_ENABLED
} interrupt_state_t;

/** @} */

void register_AI_handler( void (*callback)() );
void register_VI_handler( void (*callback)() );
void register_PI_handler( void (*callback)() );
void register_DP_handler( void (*callback)() );
void register_SI_handler( void (*callback)() );
void register_SP_handler( void (*callback)() );
void register_TI_handler( void (*callback)() );
void register_CART_handler( void (*callback)() );
void register_RESET_handler( void (*callback)() );

void unregister_AI_handler( void (*callback)() );
void unregister_VI_handler( void (*callback)() );
void unregister_PI_handler( void (*callback)() );
void unregister_DP_handler( void (*callback)() );
void unregister_SI_handler( void (*callback)() );
void unregister_SP_handler( void (*callback)() );
void unregister_TI_handler( void (*callback)() );
void unregister_CART_handler( void (*callback)() );
void unregister_RESET_handler( void (*callback)() );

void set_AI_interrupt( int active );
void set_VI_interrupt( int active, unsigned long line );
void set_PI_interrupt( int active );
void set_DP_interrupt( int active );
void set_SI_interrupt( int active );
void set_SP_interrupt( int active );
void set_TI_interrupt( int active );
void set_CART_interrupt( int active );
void set_RESET_interrupt( int active );

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

uint32_t exception_reset_time( void );

static inline __attribute__((deprecated("calling init_interrupts no longer required")))
void init_interrupts() {}

static inline __attribute__((deprecated("use register_RESET_handler instead")))
void register_reset_handler( void (*callback)() )
{
    register_RESET_handler(callback);
}

void enable_interrupts();
void disable_interrupts();

interrupt_state_t get_interrupts_state(); 

#ifdef __cplusplus
}
#endif

#endif
