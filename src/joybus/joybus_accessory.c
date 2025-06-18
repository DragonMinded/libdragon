/**
 * @file joybus_accessory.c
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joybus Accessory utilities
 * @ingroup joybus
 */

#include <string.h>

#include "joybus_accessory_internal.h"

/**
 * @addtogroup joybus
 * @{
 */

uint16_t joybus_accessory_calculate_addr_checksum(uint16_t addr)
{
    static const uint16_t xor_table[16] = {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x15, 0x1F, 0x0B,
        0x16, 0x19, 0x07, 0x0E,
        0x1C, 0x0D, 0x1A, 0x01
    };
    uint16_t checksum = 0;
    addr &= JOYBUS_ACCESSORY_ADDR_MASK_OFFSET;
    for (int i = 15; i >= 5; i--)
    {
        if ((addr >> i) & 0x1)
        {
            checksum ^= xor_table[i];
        }
    }
    checksum &= JOYBUS_ACCESSORY_ADDR_MASK_CHECKSUM;
    return addr | checksum;
}

uint8_t joybus_accessory_calculate_data_crc(const uint8_t *data)
{
    unsigned int crc = 0;

    for (int i = 0; i < JOYBUS_ACCESSORY_DATA_SIZE; i++)
    {
        unsigned int x = crc ^ data[i];
        crc = 0;
        if (x & 0x80) { crc ^= 0x89; }
        if (x & 0x40) { crc ^= 0x86; }
        if (x & 0x20) { crc ^= 0x43; }
        if (x & 0x10) { crc ^= 0xE3; }
        if (x & 0x08) { crc ^= 0xB3; }
        if (x & 0x04) { crc ^= 0x9B; }
        if (x & 0x02) { crc ^= 0x8F; }
        if (x & 0x01) { crc ^= 0x85; }
    }

    return crc;
}

joybus_accessory_io_status_t joybus_accessory_compare_data_crc(
    const uint8_t *data,
    uint8_t data_crc
)
{
    uint8_t expected_crc = joybus_accessory_calculate_data_crc(data);
    if (expected_crc == data_crc)
    {
        return JOYBUS_ACCESSORY_IO_STATUS_OK;
    }
    if (expected_crc == (data_crc ^ 0xFF))
    {
        return JOYBUS_ACCESSORY_IO_STATUS_NO_PAK;
    }
    return JOYBUS_ACCESSORY_IO_STATUS_BAD_CRC;
}

void joybus_accessory_read_async(
    int port,
    uint16_t addr,
    joybus_callback_t callback,
    void *ctx
)
{
    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    joybus_cmd_n64_accessory_read_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_N64_ACCESSORY_READ,
        .addr_checksum = joybus_accessory_calculate_addr_checksum(addr),
    } };
    size_t i = port;
    // Set the command metadata
    input[i++] = sizeof(cmd.send);
    input[i++] = sizeof(cmd.recv);
    // Micro-optimization: Minimize copy length
    const size_t recv_offset = offsetof(typeof(cmd), recv);
    memcpy(&input[i], &cmd.send, recv_offset);
    i += sizeof(cmd);
    // Close out the Joybus operation block
    input[i] = 0xFE;
    input[sizeof(input) - 1] = 0x01;
    // Execute the Joybus operation asynchronously
    joybus_exec_async(input, callback, ctx);
}

void joybus_accessory_write_async(
    int port,
    uint16_t addr,
    const uint8_t *data,
    joybus_callback_t callback,
    void *ctx
)
{
    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    joybus_cmd_n64_accessory_write_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_N64_ACCESSORY_WRITE,
        .addr_checksum = joybus_accessory_calculate_addr_checksum(addr),
    } };
    size_t i = port;
    // Set the command metadata
    input[i++] = sizeof(cmd.send);
    input[i++] = sizeof(cmd.recv);
    // Micro-optimization: Minimize copy length
    const size_t data_offset = offsetof(typeof(cmd.send), data);
    memcpy(&input[i], &cmd.send, data_offset);
    memcpy(&input[i + data_offset], data, sizeof(cmd.send.data));
    i += sizeof(cmd);
    // Close out the Joybus operation block
    input[i] = 0xFE;
    input[sizeof(input) - 1] = 0x01;
    // Execute the Joybus operation asynchronously
    joybus_exec_async(input, callback, ctx);
}

int joybus_accessory_read(int port, uint16_t addr, uint8_t *data)
{
    joybus_cmd_n64_accessory_read_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_N64_ACCESSORY_READ,
        .addr_checksum = joybus_accessory_calculate_addr_checksum(addr),
    } };
    // Execute the command
    joybus_exec_cmd_struct(port, cmd);
    // Copy the data from the command receive buffer
    memcpy(data, cmd.recv.data, sizeof(cmd.recv.data));
    // Check the CRC against the data to see if the read was successful
    return joybus_accessory_compare_data_crc(data, cmd.recv.data_crc);
}

int joybus_accessory_write(int port, uint16_t addr, const uint8_t *data)
{
    joybus_cmd_n64_accessory_write_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_N64_ACCESSORY_WRITE,
        .addr_checksum = joybus_accessory_calculate_addr_checksum(addr),
    } };
    // Copy the data to the command send buffer
    memcpy(cmd.send.data, data, sizeof(cmd.send.data));
    // Execute the command
    joybus_exec_cmd_struct(port, cmd);
    // Check the data CRC to see if the write was successful
    return joybus_accessory_compare_data_crc(data, cmd.recv.data_crc);
}

/** @} */ /* joybus */
