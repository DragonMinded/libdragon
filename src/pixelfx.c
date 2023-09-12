/**
 * @file pixelfx.c
 * @brief PixelFX integration routines
 * @ingroup pixelfx
 */

#include <string.h>

#include "joybus_commands.h"
#include "joybus_internal.h"

/**
 * @addtogroup pixelfx
 * @{
 */

void pixelfx_send_game_id(uint64_t rom_check_code, uint8_t media_format, uint8_t region_code)
{
    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    const joybus_cmd_pixelfx_n64_game_id_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_PIXELFX_N64_GAME_ID,
        .rom_check_code = rom_check_code,
        .media_format = media_format,
        .region_code = region_code,
    } };
    size_t i = 0;
    // Set the command metadata
    input[i++] = sizeof(cmd.send);
    input[i++] = sizeof(cmd.recv);
    // Copy the send_data into the input buffer
    memcpy(&input[i], &cmd, sizeof(cmd));
    i += sizeof(cmd);
    // Close out the Joybus operation block
    input[i] = 0xFE;
    input[sizeof(input) - 1] = 0x01;
    // This is a fire-and-forget command with no expected response
    joybus_exec_async(input, NULL, NULL);
}

void pixelfx_clear_game_id( void ) { pixelfx_send_game_id(0, 0, 0); }

/** @} */ /* pixelfx */
