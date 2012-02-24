/**
 * @file interrupt.h
 * @brief Interrupt Controller
 * @ingroup interrupt
 */
#ifndef __LIBDRAGON_INTERRUPT_H
#define __LIBDRAGON_INTERRUPT_H

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
void register_TI_handler( void (*callback)() );

void unregister_AI_handler( void (*callback)() );
void unregister_VI_handler( void (*callback)() );
void unregister_PI_handler( void (*callback)() );
void unregister_DP_handler( void (*callback)() );
void unregister_TI_handler( void (*callback)() );

void set_AI_interrupt( int active );
void set_VI_interrupt( int active, unsigned long line );
void set_PI_interrupt( int active );
void set_DP_interrupt( int active );

void init_interrupts();

void enable_interrupts();
void disable_interrupts();

interrupt_state_t get_interrupts_state(); 

#ifdef __cplusplus
}
#endif

#endif
