/**
 * @file joybus_n64_accessory.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joybus N64 Accessory utilities
 * @ingroup joybus
 */

#include <string.h>

#include "joybus_n64_accessory_internal.h"

/**
 * @addtogroup joybus
 * @{
 */

/**
 * @brief Applies the checksum to a Joybus N64 accessory read/write address.
 * 
 * When reading or writing a particular address on the accessory, the command
 * will send the top 11 bits of a 16 bit address, plus a 5 bit checksum.
 * 
 * @param addr The address to calculate the checksum for.
 * 
 * @return The address with the checksum applied.
 */
uint16_t joybus_n64_accessory_calculate_addr_checksum(uint16_t addr)
{
    static const uint16_t xor_table[16] = {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x15, 0x1F, 0x0B,
        0x16, 0x19, 0x07, 0x0E,
        0x1C, 0x0D, 0x1A, 0x01
    };
    uint16_t checksum = 0;
    addr &= JOYBUS_N64_ACCESSORY_ADDR_MASK_OFFSET;
    for (int i = 15; i >= 5; i--)
    {
        if ((addr >> i) & 0x1)
        {
            checksum ^= xor_table[i];
        }
    }
    checksum &= JOYBUS_N64_ACCESSORY_ADDR_MASK_CHECKSUM;
    return addr | checksum;
}

/**
 * @brief Calculates the CRC8 checksum for a Joybus N64 accessory read/write data block.
 * 
 * Uses a standard CRC8 algorithm with a seed of 0x00 and a polynomial of 0x85.
 * 
 * @param[in] data The 32-byte accessory read/write data block
 * 
 * @return The calculated CRC8 checksum for the data block 
 */
uint8_t joybus_n64_accessory_calculate_data_crc(const uint8_t *data)
{
    uint8_t ret = 0;
    for (int i = 0; i <= JOYBUS_N64_ACCESSORY_DATA_SIZE; i++)
    {
        for (int j = 7; j >= 0; j--)
        {
            int tmp = 0;
            if (ret & 0x80)
            {
                tmp = 0x85;
            }
            ret <<= 1;
            if (i < JOYBUS_N64_ACCESSORY_DATA_SIZE && data[i] & (0x01 << j))
            {
                ret |= 0x1;
            }
            ret ^= tmp;
        }
    }
    return ret;
}

/**
 * @brief Calculates the CRC8 checksum for an accessory read/write data block and
 * compares it against the provided checksum.
 * 
 * @param[in] data The 32-byte accessory read/write data block
 * @param data_crc The CRC8 checksum to compare against
 * 
 * @retval #JOYBUS_N64_ACCESSORY_IO_STATUS_OK The checksums match.
 * @retval #JOYBUS_N64_ACCESSORY_IO_STATUS_NO_PAK The checksum indicates that no accessory is present.
 * @retval #JOYBUS_N64_ACCESSORY_IO_STATUS_BAD_CRC The data checksum does not match the provided checksum.
 */
joybus_n64_accessory_io_status_t joybus_n64_accessory_compare_data_crc(
    const uint8_t *data,
    uint8_t data_crc
)
{
    uint8_t expected_crc = joybus_n64_accessory_calculate_data_crc(data);
    if (expected_crc == data_crc)
    {
        return JOYBUS_N64_ACCESSORY_IO_STATUS_OK;
    }
    if (expected_crc == (data_crc ^ 0xFF))
    {
        return JOYBUS_N64_ACCESSORY_IO_STATUS_NO_PAK;
    }
    return JOYBUS_N64_ACCESSORY_IO_STATUS_BAD_CRC;
}

/**
 * @brief Asynchronously perform a Joybus N64 accessory read command.
 * 
 * @param port The controller port of the accessory to read from.
 * @param addr The accessory address to read from.
 * @param callback A function pointer to call when the operation completes.
 * @param ctx A user data pointer to pass into the callback function.
 */
void joybus_n64_accessory_read_async(
    int port,
    uint16_t addr,
    joybus_callback_t callback,
    void *ctx
)
{
    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    size_t i = port;

    const joybus_cmd_n64_accessory_read_port_t send_cmd = {
        .send_len = sizeof(send_cmd.send_bytes),
        .recv_len = sizeof(send_cmd.recv_bytes),
        .command = JOYBUS_COMMAND_ID_N64_ACCESSORY_READ,
        .addr_checksum = joybus_n64_accessory_calculate_addr_checksum(addr),
    };
    // Micro-optimization: Minimize copy length
    const size_t recv_offset = offsetof(typeof(send_cmd), recv_bytes);
    memcpy(&input[i], &send_cmd, recv_offset);
    i += sizeof(send_cmd);

    // Close out the Joybus operation block
    input[i] = 0xFE;
    input[sizeof(input) - 1] = 0x01;

    joybus_exec_async(input, callback, ctx);
}

/**
 * @brief Asynchronously perform a Joybus N64 accessory write command.
 * 
 * @param port The controller port of the accessory to write to.
 * @param addr The accessory address to write to.
 * @param[in] data The data to write to the accessory.
 * @param callback A function pointer to call when the operation completes.
 * @param ctx A user data pointer to pass into the callback function.
 */
void joybus_n64_accessory_write_async(
    int port,
    uint16_t addr,
    const uint8_t *data,
    joybus_callback_t callback,
    void *ctx
)
{
    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    size_t i = port;

    const joybus_cmd_n64_accessory_write_port_t send_cmd = {
        .send_len = sizeof(send_cmd.send_bytes),
        .recv_len = sizeof(send_cmd.recv_bytes),
        .command = JOYBUS_COMMAND_ID_N64_ACCESSORY_WRITE,
        .addr_checksum = joybus_n64_accessory_calculate_addr_checksum(addr),
    };
    // Micro-optimization: Minimize copy length
    const size_t data_offset = offsetof(typeof(send_cmd), data);
    memcpy(&input[i], &send_cmd, data_offset);
    memcpy(&input[i + data_offset], data, sizeof(send_cmd.data));
    i += sizeof(send_cmd);

    // Close out the Joybus operation block
    input[i] = 0xFE;
    input[sizeof(input) - 1] = 0x01;

    joybus_exec_async(input, callback, ctx);
}

/**
 * @brief Synchronously perform a Joybus N64 accessory read command.
 * 
 * @param port The controller port of the accessory to read from.
 * @param addr The accessory address to start reading from.
 * @param[out] data The 32 bytes of data that was read from the accessory.
 *
 * @retval #JOYBUS_N64_ACCESSORY_IO_STATUS_OK The data was read successfully.
 * @retval #JOYBUS_N64_ACCESSORY_IO_STATUS_NO_PAK No accessory is present.
 * @retval #JOYBUS_N64_ACCESSORY_IO_STATUS_BAD_CRC The data was not read successfully.
 */
int joybus_n64_accessory_read_sync(
    int port,
    uint16_t addr,
    uint8_t *data
)
{
    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    uint8_t output[JOYBUS_BLOCK_SIZE] = {0};
    size_t i = port;

    const joybus_cmd_n64_accessory_read_port_t send_cmd = {
        .send_len = sizeof(send_cmd.send_bytes),
        .recv_len = sizeof(send_cmd.recv_bytes),
        .command = JOYBUS_COMMAND_ID_N64_ACCESSORY_READ,
        .addr_checksum = joybus_n64_accessory_calculate_addr_checksum(addr),
    };
    // Micro-optimization: Minimize copy length
    const size_t recv_offset = offsetof(typeof(send_cmd), recv_bytes);
    memcpy(&input[i], &send_cmd, recv_offset);
    i += sizeof(send_cmd);

    // Close out the Joybus operation block
    input[i] = 0xFE;
    input[sizeof(input) - 1] = 0x01;

    // Execute and wait for the Joybus operation
    joybus_exec(input, output);
    const joybus_cmd_n64_accessory_read_port_t *recv_cmd =
        (void *)&output[port];

    // Copy the data and check the CRC to see if the read was successful
    memcpy(data, recv_cmd->data, sizeof(recv_cmd->data));
    return joybus_n64_accessory_compare_data_crc(data, recv_cmd->data_crc);
}

/**
 * @brief Synchronously perform a Joybus N64 accessory write command.
 * 
 * @param port The controller port of the accessory to write to.
 * @param addr The accessory address to start writing to.
 * @param[in] data The 32 bytes of data to write to the accessory.
 *
 * @retval #JOYBUS_N64_ACCESSORY_IO_STATUS_OK The data was written successfully.
 * @retval #JOYBUS_N64_ACCESSORY_IO_STATUS_NO_PAK No accessory is present.
 * @retval #JOYBUS_N64_ACCESSORY_IO_STATUS_BAD_CRC The data was not written successfully.
 */
int joybus_n64_accessory_write_sync(
    int port,
    uint16_t addr,
    const uint8_t *data
)
{
    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    uint8_t output[JOYBUS_BLOCK_SIZE] = {0};
    size_t i = port;

    const joybus_cmd_n64_accessory_write_port_t send_cmd = {
        .send_len = sizeof(send_cmd.send_bytes),
        .recv_len = sizeof(send_cmd.recv_bytes),
        .command = JOYBUS_COMMAND_ID_N64_ACCESSORY_WRITE,
        .addr_checksum = joybus_n64_accessory_calculate_addr_checksum(addr),
    };
    // Micro-optimization: Minimize copy length
    const size_t data_offset = offsetof(typeof(send_cmd), data);
    memcpy(&input[i], &send_cmd, data_offset);
    memcpy(&input[i + data_offset], data, sizeof(send_cmd.data));
    i += sizeof(send_cmd);

    // Execute and wait for the Joybus operation
    joybus_exec(input, output);
    const joybus_cmd_n64_accessory_write_port_t *recv_cmd =
        (void *)&output[port];
    
    // Check the data CRC to see if the write was successful
    return joybus_n64_accessory_compare_data_crc(data, recv_cmd->data_crc);
}

/** @} */ /* joybus */
