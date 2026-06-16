/**
 * @file joyframe.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Joybus frame construction macros
 * @ingroup joybus
 * 
 * This API allows for the construction of PIF frames in memory (also called
 * joybus frames), that can be run via #joybus_exec / #joybus_exec_async.
 * 
 * To create a frame, you can use #joyframe_write API macros. This is a simple
 * example:
 * 
 * @code{.c}
 *     joyframe_t pkt;
 *     joyframe_write_begin(pkt);
 *     joyframe_write_skip(pkt, 2);                       // Skip port 1-2
 *     uint8_t *send = joyframe_write(pkt, 2, 4, 0xAB);   // Port 3: command 0xAB
 *     *send = 0x11;                                      // First byte of send data
 *     joyframe_write_end(pkt);                           
 *   
 *     // Execute the frame (synchronously). Output will be written into
 *     // the same frame buffer.
 *     joybus_exec(pkt, pkt);
 * @endcode
 * 
 * Reading back the results of a frame can be done via #joyframe_read API macros.
 * These macros allow to parse the frame and extract the results of each command.
 * The macros are designed to parse arbitrary frames without previous knowledge
 * of how they were constructed, and also handling all error conditions.
 * 
 * @code{.c}
 *    joyframe_read_begin(pkt);
 *    joyframe_read_skip(pkt, 2);                     // Parse but ignore (skip) port 1-2
 * 
 *    joyframe_error_t err; 
 *    uint8_t cmd; uint8_t *data;
 *
 *    err = joyframe_read(pkt, &cmd, &data, NULL);    // Parse port 3
 *    if (err < 0) {
 *      // Error handling: no device, timeout, etc.
 *    }
 *    
 *    // Check that the command is what we expected to read back.
 *    // If you just process the result of #joybus_exec that you constructed,
 *    // you are already sure of the contents of the frame, but in general you
 *    // could instead have a frame parsing code that handle different structures.
 *    assert(cmd == 0xAB);
 * 
 *    // Process the data received. The length of the data is known from
 *    // the command that was sent.
 *    debugf("Received %02X %02X %02X %02X\n", data[0], data[1], data[2], data[3]);
 * @endcode
 * 
 * It is important to handle error conditions as they can always happen, given
 * that joybus devices are hot-pluggable. For instance, a device might have been
 * disconnected by the player, so the command might return an error.
 * 
 * Helpers for known commands
 * --------------------------
 * For known joybus commands (as publicly documented), there are helper macros
 * that allow to write and read them more easily, as they take care of
 * packing and unpacking the input and output data. 
 * 
 * For instance, the following code shows an example to perform a handshake to
 * identify a device on the first port. This is normally not required as
 * the joybus module, after #joybus_init, automatically performs background
 * handshakes to identify connected devices, but it is shown here as an example.
 * 
 * @code{.c}
 *    joyframe_t pkt;
 *    joyframe_write_begin(pkt);
 *    joyframe_write_N64_IDENTIFY(pkt);
 *    joyframe_write_end(pkt);
 * 
 *    // Execute the handshake (synchronously)
 *    joybus_exec(pkt, pkt);
 * 
 *    // Parse the result
 *    uint16_t device_id; uint8_t status;
 *    joyframe_read_begin(pkt);
 *    if (!joyframe_read_N64_IDENTIFY(pkt, &device_id, &status)) {
 *       // Error handling: no device, timeout, etc.
 *    }
 *    debugf("Device ID: %04X, status %02X\n", device_id, status);
 * @endcode
 * 
 * Libraries to handle custom joybus devices can expose functions with similar
 * APIs to handle inserting their own commands into a joybus frame.
 */
#ifndef LIBDRAGON_JOYFRAME_H
#define LIBDRAGON_JOYFRAME_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "debug.h"

/** 
 * @brief Joybus frame escape codes
 * \{
 */
#define JOYFRAME_ESC_SKIP         0x00        ///< Skip this port
#define JOYFRAME_ESC_RESET        0xFD        ///< Reset this port
#define JOYFRAME_ESC_END          0xFE        ///< End of frame
#define JOYFRAME_ESC_NOP          0xFF        ///< No operation, ignored
/** \} */

/** 
 * @brief Joybus TX/RX flag bits
 * \{
 */
#define JOYFRAME_TX_SKIP          0x80        ///< Skip this port
#define JOYFRAME_TX_RESET         0x40        ///< Reset this port
#define JOYFRAME_RX_NO_DEVICE     0x80        ///< No device connected
#define JOYFRAME_RX_TIMEOUT       0x40        ///< Error during transmission/reception
/** \} */

/** @brief A joybus frame */
typedef uint8_t joyframe_t[64] __attribute__((aligned(8)));

/**
 * @brief Start writing a Joybus frame
 * 
 * A joybus frame is a 64-byte buffer containing a series of commands to be sent
 * to each port in sequence. It allows only for one command per port. If you
 * don't have a command to send to the current port, you can write a "skip" command
 * (#joyframe_write_skip) to skip it. The frame can be terminated early by writing
 * an "End of frame" command (#joyframe_write_end), 
 * 
 * For instance, to write only a command to port 3 (1-based), you would do:
 * 
 * @code{.c}
 *      joyframe_t pkt;
 * 
 *      joyframe_write_begin(pkt);
 *      joyframe_write_skip(pkt, 2);    // Skip port 1-2
 *      joyframe_write_command(...);    // Command for Port 3
 *      joyframe_write_end(pkt);        // End of frame (auto skip ports 4-5)
 * @endcode
 * 
 * @hideinitializer
 */
#define joyframe_write_begin(pkt) ({ \
    ((uint64_t *)pkt)[0] = 0; \
    ((uint64_t *)pkt)[1] = 0; \
    ((uint64_t *)pkt)[2] = 0; \
    ((uint64_t *)pkt)[3] = 0; \
    ((uint64_t *)pkt)[4] = 0; \
    ((uint64_t *)pkt)[5] = 0; \
    ((uint64_t *)pkt)[6] = 0; \
    ((uint64_t *)pkt)[7] = 0; \
})

/**
 * @brief Write a Joybus frame
 *
 * This macro writes a Joybus frame to the specified buffer. The command is
 * the one that will be sent to the current port.
 * 
 * @param pkt       Joybus frame buffer
 * @param send_len  Length of the send data in bytes (including command byte)
 * @param recv_len  Length of the receive data in bytes
 * @param cmd       Command byte
 */
#define joyframe_write(pkt, send_len, recv_len, cmd) ({ \
    int idx = pkt[63]; \
    pkt[idx++] = send_len; \
    pkt[idx++] = recv_len; \
    pkt[idx++] = cmd; \
    uint8_t *send_ptr = &pkt[idx]; \
    pkt[63] = idx + send_len + recv_len - 1; \
    assertf(pkt[63] < 64, "joybus frame overflow"); \
    (send_len | recv_len) ? send_ptr : NULL; \
})

/**
 * @brief Write a "Reset" command to the Joybus frame
 * 
 * This macro writes a "Reset" command to the current port in the Joybus frame.
 * 
 * The PIF will emit a special signal (keep the line low for 1 ms), which is
 * meant to reset the connected device. Not all devices support this command,
 * and the actual behavior on joybus devices is not well documented.
 * 
 * @hideinitializer
 */
#define joyframe_write_reset(pkt) ({ \
    int idx = pkt[63]; \
    pkt[idx++] = JOYFRAME_ESC_RESET; \
    pkt[63] = idx; \
})

/**
 * @brief Write a "Skip" command to the Joybus frame
 * 
 * This macro should be used whenever you don't have a command to send
 * to the current port. It tells the PIF to skip this port and move
 * to the next one.
 * 
 * @hideinitializer
 */
#define joyframe_write_skip(pkt, n) ({ \
    int __n = (n); \
    int idx = pkt[63]; \
    for (int i = 0; i < __n; i++) { \
        pkt[idx++] = JOYFRAME_ESC_SKIP; \
    } \
    pkt[63] = idx; \
})

/**
 * @brief Write an "End of frame" command to the Joybus frame
 * 
 * This macro marks the frame as ended. The PIF will skip any remaining
 * ports after this command.
 * 
 * Make sure to always call this macro after writing your commands.
 * 
 * @hideinitializer
 */
#define joyframe_write_end(pkt) ({ \
    int idx = pkt[63]; \
    pkt[idx++] = JOYFRAME_ESC_END; \
    assertf(idx < 64, "joybus frame overflow"); \
    pkt[63] = 0x01; \
})


/**
 * @brief Joybus frame status codes for #joyframe_read
 * 
 * These codes are returned by #joyframe_read to indicate the parsing status
 * of the command read from the frame.
 * 
 * The codes are divided into two categories:
 *  * Positive values (including zero) indicate success, possibly with special
 *    escape codes being parsed. For instance, if the port was skipped or reset,
 *    the function returns a positive value with value #JOYFRAME_SKIPPED or
 *    #JOYFRAME_RESET respectively.
 *  * Negative values indicate errors, such as no device connected, timeout,
 *    or that the frame has been fully parsed.
 */
typedef enum {
    JOYFRAME_RESET      =  2,         ///< Port was reset by the frame
    JOYFRAME_SKIPPED    =  1,         ///< Port was skipped in the frame

    JOYFRAME_ERR_NONE       =  0,
    JOYFRAME_ERR_NO_DEVICE  = -1,     ///< No device connected to this port
    JOYFRAME_ERR_TIMEOUT    = -2,     ///< Device did not respond in time (wrong command?)
    JOYFRAME_ERR_FINISHED   = -4,     ///< The frame does not contain a command for this port
} joyframe_err_t;


/**
 * @brief Initialize reading a Joybus frame
 * 
 * @hideinitializer
 */
#define joyframe_read_begin(pkt) ({ \
    pkt[63] = 0; \
})

/**
 * @brief Skip a number of ports in the Joybus frame
 * 
 * If you are not interested in reading the commands for a number of ports,
 * you can use this macro to skip them.
 */
#define joyframe_read_skip(pkt, n) ({ \
    int __n = (n); \
    for (int i = 0; i < __n; i++) { \
        (void)joyframe_read(pkt, NULL, NULL); \
    } \
})

/**
 * @brief Read the next command from the frame
 *
 * This function reads the command for next port from the Joybus frame. 
 * It is designed to parse arbitrary frames even without previous knowledge of
 * the contents, and also including malformed ones.
 * 
 * Each call to this function returns the command for the next port in sequence,
 * including the command ID, the data received from the device and the length
 * of the data.
 * 
 * It is mandatory to also check the return value of this function, as it can
 * indicate errors such as no device connected, timeout, etc. Handling errors
 * is important, as joybus devices are hot-pluggable and errors can always happen.
 * 
 * If the frame contained the special "skip" or "reset" commands for a certain
 * port (as written via #joyframe_write_skip or #joyframe_write_reset), the
 * function return a non-zero positive value, and the variables pointed by
 * `cmd`, `data` and `data_len` are not modified.
 *
 * @param[in]  pkt        Joybus frame buffer
 * @param[out] cmd        Command byte read from the frame (optional, can be NULL)
 * @param[out] data       Pointer to the data buffer read from the frame (optional, can be NULL)
 * @param[out] data_len   Length of the data buffer read from the frame (optional, can be NULL)
 * @return joyframe_err_t   Negative values for errors, 0 or positive values for success
 */
__attribute__ ((warn_unused_result))
joyframe_err_t joyframe_read(joyframe_t pkt, uint8_t *cmd, uint8_t **data, int *data_len);


/**
 * @name Helpers to write/read common Joybus commands
 * 
 * These helpers show how to implement wrappers for #joyframe_write and #joyframe_read
 * to help library code and user code to build and parse Joybus frames.
 * 
 * The only two commands that we treat as valid for all Joybus devices (and are
 * in fact implemented and emitted by the joybus library itself) are IDENTIFY
 * and RESET.
 * 
 * All other commands handled here as just meant as examples, and will only work
 * when issued to the correct type of device. For instance, the N64 controller
 * read command will only work when issued to an N64 controller or to a device
 * that emulates it.
 * 
 * \{
 */

/** @brief Write the RESET command to a joyframe */
inline void joyframe_write_RESET(joyframe_t pkt)
{
    joyframe_write(pkt, 1, 3, 0xFF);
}

/**
 * @brief Read the result of the RESET command from a joyframe
 * 
 * @param pkt                   Joybus frame buffer
 * @param[out] device_id        Pointer to store the device ID (optional, can be NULL)
 * @param[out] device_status    Pointer to store the device status (optional, can be NULL)
 * @return true if the command was successfully read, false otherwise
 */
__attribute__ ((warn_unused_result))
inline bool joyframe_read_RESET(joyframe_t pkt, uint16_t *device_id, uint8_t *device_status)
{
    uint8_t cmd = 0; uint8_t *data = NULL; int data_len = 0;
    if (joyframe_read(pkt, &cmd, &data, &data_len) != JOYFRAME_ERR_NONE) return false;
    if (cmd != 0xFF || data_len != 3) return false;
    if (device_id) *device_id = (data[0] << 8) | data[1];
    if (device_status) *device_status = data[2];
    return true;
}

/** @brief Write the IDENTIFY command to a joyframe */
inline void joyframe_write_IDENTIFY(joyframe_t pkt)
{
    joyframe_write(pkt, 1, 3, 0x00);
}

/** @brief Read the result of an IDENTIFY command from a joyframe
 * 
 * Identifiers are defined in joybus.h as JOYBUS_IDENTIFIER_* macros.
 * Device status is device-specific.
 * 
 * @param pkt                   Joybus frame buffer
 * @param[out] device_id        Pointer to store the device ID (optional, can be NULL)
 * @param[out] device_status    Pointer to store the device status (optional, can be NULL)
 * @return true if the command was successfully read, false otherwise
 */
__attribute__ ((warn_unused_result))
inline bool joyframe_read_IDENTIFY(joyframe_t pkt, uint16_t *device_id, uint8_t *device_status)
{
    uint8_t cmd = 0; uint8_t *data = NULL; int data_len = 0;
    if (joyframe_read(pkt, &cmd, &data, &data_len) != JOYFRAME_ERR_NONE) return false;
    if (cmd != 0x00 || data_len != 3) return false;
    if (device_id) *device_id = (data[0] << 8) | data[1];
    if (device_status) *device_status = data[2];
    return true;
}

/** @brief Write the N64 controller read command to a joyframe */
inline void joyframe_write_N64_CONTROLLER_READ(joyframe_t pkt)
{
    joyframe_write(pkt, 1, 4, 0x01);
}

/** 
 * @brief Write the N64 accessory read command to a joyframe
 * 
 * @param pkt       Joybus frame buffer
 * @param address   Accessory address to read from (16-bit, must include checksum)
 */
inline void joyframe_write_N64_ACCESSORY_READ(joyframe_t pkt, uint16_t address)
{
    uint8_t *send_ptr = joyframe_write(pkt, 3, 33, 0x02);
    *send_ptr++ = address >> 8;
    *send_ptr++ = address & 0xFF;
}


/** 
 * @brief Read the result of a N64 accessory read command from a joyframe
 * 
 * @param pkt           Joybus frame buffer
 * @param[out] data     Pointer to store the data read (32 bytes)
 * @param[out] checksum Pointer to store the checksum read (1 byte)
 * @return true if the command was successfully read, false otherwise
 */
__attribute__ ((warn_unused_result))
inline bool joyframe_read_N64_ACCESSORY_READ(joyframe_t pkt, uint8_t **data, uint8_t *checksum)
{
    uint8_t cmd = 0; int data_len = 0;
    if (joyframe_read(pkt, &cmd, data, &data_len) != JOYFRAME_ERR_NONE) return false;
    if (cmd != 0x02 || data_len != 33) return false;
    *checksum = (*data)[32];
    return true;
}

/**
 * @brief Write the N64 accessory write command to a joyframe
 *
 * @param pkt       Joybus frame buffer
 * @param address   Accessory address to write to (16-bit, must include checksum)
 * @param data      Pointer to the data to write (32 bytes)
 */
inline void joyframe_write_N64_ACCESSORY_WRITE(joyframe_t pkt, uint16_t address, const uint8_t data[32])
{
    uint8_t *send_ptr = joyframe_write(pkt, 35, 1, 0x03);
    *send_ptr++ = address >> 8;
    *send_ptr++ = address & 0xFF;
    memcpy(send_ptr, data, 32);
}

/**
 * @brief Read the result of a N64 accessory write command from a joyframe
 * 
 * @param pkt           Joybus frame buffer
 * @param[out] checksum Pointer to store the checksum read (1 byte)
 * @return true if the command was successfully read, false otherwise
 */
__attribute__ ((warn_unused_result))
inline bool joyframe_read_N64_ACCESSORY_WRITE(joyframe_t pkt, uint8_t *checksum)
{
    uint8_t cmd = 0; uint8_t *data = NULL; int data_len = 0;
    if (joyframe_read(pkt, &cmd, &data, &data_len) != JOYFRAME_ERR_NONE) return false;
    if (cmd != 0x03 || data_len != 1) return false;
    *checksum = data[0];
    return true;
}

/**
 * \}
 */

#endif // LIBDRAGON_JOYFRAME_H
