/**
 * @file joybus.h
 * @brief Joybus Subsystem
 * @ingroup joybus
 */
#ifndef __LIBDRAGON_JOYBUS_H
#define __LIBDRAGON_JOYBUS_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @anchor JOYBUS_PAYLOAD_SIZES
 * @name Joybus payload sizes
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
/** 
 * @brief Size of a Joybus N64 accessory read/write payload in bytes. 
 */
#define JOYBUS_ACCESSORY_DATA_SIZE  32
/** @} */ /* JOYBUS_PAYLOAD_SIZES */

/**
 * @brief Count of Joybus ports.
 * 
 * The N64 has four joypad ports, plus one additional port exposed
 * on the cartridge connector for EEPROM and real-time clock support.
 */
#define JOYBUS_PORT_COUNT 5

/**
 * @brief Joypad Identifier type
 *
 * For known values, see @ref JOYBUS_IDENTIFIER and
 * @ref JOYBUS_IDENTIFIER_GCN_BITFIELD.
 * 
 * @note For GameCube peripherals, the Joybus identifier should be
 * interpreted as a bitfield rather than a single value.
 */
typedef uint16_t joybus_identifier_t;

/**
 * @anchor JOYBUS_IDENTIFIER
 * @name Joybus identifier values
 * @{
 */
/**
 * @brief Joybus identifier for an unknown or malfunctioning device.
 */
#define JOYBUS_IDENTIFIER_UNKNOWN               0x0000
/**
 * @brief Joybus identifier for a port with no device connected.
 */
#define JOYBUS_IDENTIFIER_NONE                  0xFFFF
/**
 * @brief Joybus identifier for the Nintendo 64 voice recognition peripheral (NUS-020).
 * 
 * Also known as VRU in North America and VRS in Japan.
 */
#define JOYBUS_IDENTIFIER_N64_VOICE_RECOGNITION 0x0001
/**
 * @brief Joybus identifier for the Nintendo 64 Randnet keyboard peripheral (RND-001).
 */
#define JOYBUS_IDENTIFIER_N64_RANDNET_KEYBOARD  0x0002
/**
 * @brief Joybus identifier for the unreleased 64GB Link Cable.
 */
#define JOYBUS_IDENTIFIER_64GB_LINK_CABLE       0x0003
/**
 * @brief Joybus identifier for a Game Boy Advance Link Cable (DOL-011).
 */
#define JOYBUS_IDENTIFIER_GBA_LINK_CABLE        0x0004
/**
 * @brief Joybus identifier for cartridge-based real-time clock.
 */
#define JOYBUS_IDENTIFIER_CART_RTC              0x0010
/**
 * @brief Joybus identifier for cartridge-based 4Kbit EEPROM save type.
 */
#define JOYBUS_IDENTIFIER_CART_EEPROM_4KBIT     0x0080
/**
 * @brief Joybus identifier for cartridge-based 16Kbit EEPROM save type.
 */
#define JOYBUS_IDENTIFIER_CART_EEPROM_16KBIT    0x00C0
/**
 * @brief Joybus identifier for a standard Nintendo 64 controller (NUS-005).
 */
#define JOYBUS_IDENTIFIER_N64_CONTROLLER        0x0500
/**
 * @brief Joybus identifier for the Nintendo 64 mouse peripheral (NUS-017).
 */
#define JOYBUS_IDENTIFIER_N64_MOUSE             0x0200
/** @} */

/**
 * @anchor JOYBUS_IDENTIFIER_GCN_BITFIELD
 * @name Joybus identifier bitfield for GameCube peripherals
 * 
 * Note that for GameCube peripherals, the Joybus identifier is
 * interpreted as a bitfield rather than a single value.
 * 
 * In particular, Wavebird controllers will return a different identifiers
 * depending on wireless state.
 * 
 * To identify a device that acts like a standard GameCube controller,
 * check the #JOYBUS_IDENTIFIER_MASK_PLATFORM and the
 * #JOYBUS_IDENTIFIER_MASK_GCN_CONTROLLER values.
 * 
 * @{
 */
/**
 * @brief Joybus identifier platform bitfield mask.
 * 
 * Bits 11-12 of the Joybus identifier signify the intended platform:
 * 
 * * Bit 11 is set for GameCube devices.
 * * Bit 12 is zero for all known devices.
*/
#define JOYBUS_IDENTIFIER_MASK_PLATFORM         0x1800
/**
 * @brief GameCube Joybus identifier platform value.
 * 
 * Bit 11 of the Joybus identifier is one for GameCube devices.
 * Bit 12 of the Joybus identifier is zero for all known devices.
 */
#define JOYBUS_IDENTIFIER_PLATFORM_GCN          0x0800
/**
 * @brief Joybus identifier GameCube standard controller flag.
 * 
 * For GameCube platform devices, this bit is set if the device acts like a standard controller.
 */
#define JOYBUS_IDENTIFIER_MASK_GCN_CONTROLLER   0x0100
/**
 * @brief Joybus identifier GameCube rumble support flag.
 * 
 * For GameCube controllers, this bit is set if the controller DOES NOT support rumble functionality.
 */
#define JOYBUS_IDENTIFIER_MASK_GCN_NORUMBLE     0x2000
/**
 * @brief Joybus identifier GameCube wireless flag.
 * 
 * For GameCube controllers, this bit is set if the controller is a wireless controller.
 */
#define JOYBUS_IDENTIFIER_MASK_GCN_WIRELESS     0x8000
/** @} */ /* JOYBUS_IDENTIFIER_GCN_BITFIELD */

/**
 * @anchor JOYBUS_IDENTIFY_STATUS
 * @name Joybus identify status values
 * @{
 */
/**
 * @brief Joybus identify status byte mask for N64 accessory presence values.
 */
#define JOYBUS_IDENTIFY_STATUS_ACCESSORY_MASK             0x03
/**
 * @brief Joybus identify status for an N64 controller that does not support accessories.
 * 
 * Some third-party controllers incorrectly use this status to mean absence of an accessory.
 * Therefore, this value is treated as a synonym for JOYBUS_IDENTIFY_STATUS_ACCESSORY_ABSENT.
 */
#define JOYBUS_IDENTIFY_STATUS_ACCESSORY_UNSUPPORTED      0x00
/**
 * @brief Joybus identify status for an N64 controller with an accessory present.
 */
#define JOYBUS_IDENTIFY_STATUS_ACCESSORY_PRESENT          0x01
/**
 * @brief Joybus identify status for an N64 controller with no accessory present.
 */
#define JOYBUS_IDENTIFY_STATUS_ACCESSORY_ABSENT           0x02
/**
 * @brief Joybus identify status for an N64 controller with an accessory present
 * that has changed since it was last identified.
 */
#define JOYBUS_IDENTIFY_STATUS_ACCESSORY_CHANGED          0x03
/**
 * @brief Joybus identify status bit for a VRU/VRS that is initialized and ready.
 */
#define JOYBUS_IDENTIFY_STATUS_VOICE_RECOGNITON_READY     0x01
/**
 * @brief Joybus identify status bit that signifies the previous accessory command
 * had a checksum error.
 */
#define JOYBUS_IDENTIFY_STATUS_COMMAND_CHECKSUM_ERROR     0x04
/**
 * @brief Joybus identify status bit for GameCube controllers that indicates whether
 * the rumble motor is currently active.
 */
#define JOYBUS_IDENTIFY_STATUS_GCN_RUMBLE_ACTIVE          0x08
/**
 * @brief Joybus identify status bit for EEPROM devices that indicates a write is in-progress.
 */
#define JOYBUS_IDENTIFY_STATUS_EEPROM_BUSY                0x80
/** @} */


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

/**
 * @brief Execute a Joybus command synchronously on the given port.
 * 
 * For convenience, there is a #joybus_exec_cmd_struct macro that uses the
 * send and recv fields of a command struct to call this function with the
 * proper send_len and recv_len arguments.
 * 
 * This function only sends a single command to a single port. For sending
 * a command to multiple ports simultaneously, use #joybus_exec instead.
 * 
 * For reading controllers, use the @ref joypad "Joypad Subsystem" instead.
 * 
 * This function is not a stable feature of the libdragon API and should be
 * considered experimental!
 * 
 * The usage of this function will likely change as a result of the ongoing
 * effort to integrate the multitasking kernel with asynchronous operations.
 * 
 * @note This function is slow: it blocks until the command completes.
 *       Calling this function multiple times per frame may cause
 *       audio and video stuttering.
 * 
 * @param      port
 *             The Joybus port (0-4) to send the command to.
 * @param      send_len
 *             Number of bytes in the send_data request payload
 *             (including the command ID).
 * @param      recv_len
 *             Number of bytes in the recv_data response payload.
 * @param[in]  send_data
 *             Buffer of send_len bytes to send to the Joybus device
 *             (including the command ID byte).
 * @param[out] recv_data
 *             Buffer of recv_len bytes for the reply from the Joybus device.
 */
inline void joybus_exec_cmd(
    int port,
    size_t send_len,
    size_t recv_len,
    const void *send_data,
    void *recv_data
)
{
    // Validate the desired Joybus port offset
    assert((port >= 0) && (port < JOYBUS_PORT_COUNT));
    // Ensure the send_len and recv_len fit in the Joybus operation block
    assert((port + send_len + recv_len) < (JOYBUS_BLOCK_SIZE - 4));
    // Allocate the Joybus operation block input and output buffers
    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    uint8_t output[JOYBUS_BLOCK_SIZE] = {0};
    // Skip commands on ports before the desired port offset
    size_t i = port;
    // Set the command metadata
    input[i++] = send_len;
    input[i++] = recv_len;
    // Copy the send_data into the input buffer
    memcpy(&input[i], send_data, send_len);
    i += send_len + recv_len;
    // Close out the Joybus operation block
    input[i] = 0xFE;
    input[sizeof(input) - 1] = 0x01;
    // Execute the Joybus operation synchronously
    joybus_exec(input, output);
    // Copy recv_data from the output buffer
    memcpy(recv_data, &output[i - recv_len], recv_len);
}

/**
 * @brief Execute a Joybus command struct synchronously.
 * 
 * This macro is a convenience wrapper around #joybus_exec_cmd
 * that uses the send and recv fields of the struct to set the
 * proper arguments.
 * 
 * This is not a stable feature of the libdragon API and should
 * be considered experimental!
 * 
 * The usage of this macro will likely change as a result of the
 * ongoing effort to integrate the multitasking kernel with
 * asynchronous operations.
 * 
 * @note This operation is slow: it blocks until the command completes.
 *       Calling this multiple times per frame may cause audio and video
 *       stuttering.
 * 
 * @param         port The Joybus port to execute the command on.
 * @param[in,out] cmd The command struct to execute with.
 */
#define joybus_exec_cmd_struct(port, cmd) \
    joybus_exec_cmd(                      \
        port,                             \
        sizeof(cmd.send),                 \
        sizeof(cmd.recv),                 \
        (void *)&cmd.send,                \
        (void *)&cmd.recv                 \
    )

#ifdef __cplusplus
}
#endif

/** @} */ /* joybus */

#endif
