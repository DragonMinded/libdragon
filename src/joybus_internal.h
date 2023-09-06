/**
 * @file joybus_internal.h
 * @brief Joybus internal API
 * @ingroup joybus
 */

#ifndef __LIBDRAGON_JOYBUS_INTERNAL_H
#define __LIBDRAGON_JOYBUS_INTERNAL_H

#include <stdint.h>
#include <string.h>

/**
 * @addtogroup joybus
 * @{
 */

/**
 * @brief Execute a Joybus command synchronously on the given port.
 * 
 * This function is intended to be called internally by the
 * #joybus_cmd_exec macro.
 * 
 * @param      port
 *             The Joybus port (0-4) to send the command to.
 * @param      send_len
 *             Number of bytes in the send_data request payload
 *             (including command ID).
 * @param      recv_len
 *             Number of bytes in the recv_data response payload.
 * @param[in]  send_data
 *             Buffer of send_len bytes to send to the Joybus device
 *             (including command ID byte).
 * @param[out] recv_data
 *             Buffer of recv_len bytes for the reply from the Joybus device.
 */
inline void __joybus_cmd_exec(
    int port,
    size_t send_len,
    size_t recv_len,
    const uint8_t *send_data,
    uint8_t *recv_data
)
{
    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    uint8_t output[JOYBUS_BLOCK_SIZE] = {0};
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
    // Execute the Joybus operation
    joybus_exec(input, output);
    // Copy recv_data from the output buffer
    memcpy(recv_data, &output[i - recv_len], recv_len);
}

/**
 * @brief Execute a Joybus command struct synchronously.
 * 
 * This macro is a convenience wrapper around #__joybus_cmd_exec
 * that automatically sets the send_len and recv_len based on the
 * size of the send and recv fields of the command struct.
 * 
 * @param         port The Joybus port to execute the command on.
 * @param[in,out] cmd The command struct to execute with.
 */
#define joybus_cmd_exec(port, cmd) \
    __joybus_cmd_exec(             \
        port,                      \
        sizeof(cmd.send),          \
        sizeof(cmd.recv),          \
        (void *)&cmd.send,         \
        (void *)&cmd.recv          \
    )

/** @brief Callback function signature for #joybus_exec_async */
typedef void (*joybus_callback_t)(uint64_t *out_dwords, void *ctx);

void joybus_exec_async(const void * input, joybus_callback_t callback, void *ctx);

/** @} */ /* joybus */

#endif
