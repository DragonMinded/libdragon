/**
 * @file joybus.c
 * @brief Joybus Subsystem
 * @ingroup lowlevel
 */

#include <string.h>
#include "libdragon.h"
#include "regsinternal.h"

/**
 * @defgroup joybus Joybus Subsystem
 * @ingroup lowlevel
 * @brief Joybus peripheral interface.
 *
 * The Joybus subsystem is in charge of communication with all controllers,
 * accessories, and peripherals plugged into the N64 controller ports as well
 * as some peripherals on the cartridge. The Joybus subsystem is responsible
 * for communicating with the serial interface (SI) registers to send commands
 * to controllers (including Controller Paks, Rumble Paks, and Transfer Paks),
 * the VRU, EEPROM save memory, and the cartridge-based real-time clock.
 * 
 * This module implements just the low-level protocol. You should use it
 * only to implement an unsupported peripherals. Otherwise, refer to the
 * higher-level modules such as:
 *
 * For controllers:
 * @ref controller "Controller Subsystem".
 *
 * For EEPROM, RTC and other peripherals:
 * @ref peripherals "Peripherals Subsystem".
 *
 * Internally, the JoyBus subsystem communicates with the PIF controller via
 * the SI DMA, via the JoyBus protocol which is a standard master/slave
 * binary protocol. Each message of the protocol is a block of 64 bytes, and
 * can contain multiple commands. Currently, there are no macros or functions
 * to help composing a JoyBus message, so higher-level libraries currently
 * hard code the binary messages. 
 * 
 * All communications is made asynchronously because SI DMA is quite slow:
 * its completion is bound to the PIF actually processing the data, rather than
 * just being the memory transfer. A queue of pending JoyBus messages is kept
 * in a ring buffer, and is then executed under interrupt when the previous SI DMA
 * completes. The internal entry point is #joybus_exec_async, that schedules
 * a message to be sent to PIF, and calls a callback with the reply whenever
 * it is available. A blocking API (#joybus_exec) is made available for
 * simpler usage.
 *
 * @{
 */

/**
 * @name SI status register bit definitions
 * @{
 */

/** @brief SI DMA busy */
#define SI_STATUS_DMA_BUSY  ( 1 << 0 )
/** @brief SI IO busy */
#define SI_STATUS_IO_BUSY   ( 1 << 1 )
/** @} */

/**
 * @brief Structure used to interact with SI registers.
 */
static volatile struct SI_regs_s * const SI_regs = (struct SI_regs_s *)0xa4800000;

/**
 * @brief Pointer to the memory-mapped location of the PIF RAM.
 */
static void * const PIF_RAM = (void *)0x1fc007c0;

/**
 * @brief Wait until the SI is finished with a DMA request
 */
static void __SI_DMA_wait( void )
{
    while (SI_regs->status & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY)) ;
}

/**
 * @brief Write a 64-byte block of data to the PIF and read the 64-byte result.
 * 
 * This function is not a stable feature of the libdragon API and should be
 * considered experimental!
 * 
 * The usage of this function will likely change as a result of the ongoing
 * effort to integrate the multitasking kernel with asynchronous operations.
 *
 * @param[in]  input
 *             Source buffer for the input block to send to the PIF
 *
 * @param[out] output
 *             Destination buffer to place the output block from the PIF
 */
void joybus_exec( const void * input, void * output )
{
    volatile uint64_t input_aligned[JOYBUS_BLOCK_DWORDS] __attribute__((aligned(16)));
    volatile uint64_t output_aligned[JOYBUS_BLOCK_DWORDS] __attribute__((aligned(16)));

    data_cache_hit_writeback_invalidate(input_aligned, JOYBUS_BLOCK_SIZE);
    memcpy(UncachedAddr(input_aligned), input, JOYBUS_BLOCK_SIZE);

    /* Be sure another thread doesn't get into a resource fight */
    disable_interrupts();

    __SI_DMA_wait();

    SI_regs->DRAM_addr = input_aligned; // only cares about 23:0
    MEMORY_BARRIER();
    SI_regs->PIF_addr_write = PIF_RAM;
    MEMORY_BARRIER();

    __SI_DMA_wait();

    data_cache_hit_writeback_invalidate(output_aligned, JOYBUS_BLOCK_SIZE);

    SI_regs->DRAM_addr = output_aligned;
    MEMORY_BARRIER();
    SI_regs->PIF_addr_read = PIF_RAM;
    MEMORY_BARRIER();

    __SI_DMA_wait();

    /* Now that we've copied, its safe to let other threads go */
    enable_interrupts();

    memcpy(output, UncachedAddr(output_aligned), JOYBUS_BLOCK_SIZE);
}

/** @} */ /* joybus */
