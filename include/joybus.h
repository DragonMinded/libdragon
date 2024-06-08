/**
 * @file joybus.h
 * @brief Joybus Subsystem
 * @ingroup lowlevel
 */
#ifndef __LIBDRAGON_JOYBUS_H
#define __LIBDRAGON_JOYBUS_H

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup joybus Joybus Subsystem
 * @ingroup peripherals
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
 * @brief Size of a Joybus input/output block in bytes.
 */
#define JOYBUS_BLOCK_SIZE 64
/**
 * @brief Size of a Joybus input/output block in double-words.
 */
#define JOYBUS_BLOCK_DWORDS ( JOYBUS_BLOCK_SIZE / sizeof(uint64_t) )


#ifdef __cplusplus
extern "C" {
#endif

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
void joybus_exec( const void* input, void* output );

#ifdef __cplusplus
}
#endif

/** @} */ /* joybus */

#endif
