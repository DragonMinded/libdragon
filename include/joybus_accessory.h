/**
 * @file joybus_accessory.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joybus accessory utilities
 * @ingroup joybus
 */

#ifndef __LIBDRAGON_JOYBUS_ACCESSORY_H
#define __LIBDRAGON_JOYBUS_ACCESSORY_H

#include <stdint.h>

/**
 * @addtogroup joybus
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Joybus accessory read/write status values */
typedef enum
{
    /** @brief Joybus accessory data communcation was successful. */
    JOYBUS_ACCESSORY_IO_STATUS_OK        = 0,
    /** @brief No N64 controller is connected. */
    JOYBUS_ACCESSORY_IO_STATUS_NO_DEVICE = -1,
    /** @brief No N64 accessory is connected. */
    JOYBUS_ACCESSORY_IO_STATUS_NO_PAK    = -2,
    /** @brief Joybus accessory communication was not successful. */
    JOYBUS_ACCESSORY_IO_STATUS_BAD_CRC   = -3,
} joybus_accessory_io_status_t;

/**
 * @brief Synchronously perform a Joybus N64 accessory read command.
 * 
 * @param port The controller port of the accessory to read from.
 * @param addr The accessory address to start reading from.
 * @param[out] data The 32 bytes of data that was read from the accessory.
 *
 * @retval #JOYBUS_ACCESSORY_IO_STATUS_OK The data was read successfully.
 * @retval #JOYBUS_ACCESSORY_IO_STATUS_NO_PAK No accessory is present.
 * @retval #JOYBUS_ACCESSORY_IO_STATUS_BAD_CRC The data was not read successfully.
 */
int joybus_accessory_read(int port, uint16_t addr, uint8_t *data);

/**
 * @brief Synchronously perform a Joybus N64 accessory write command.
 * 
 * @param port The controller port of the accessory to write to.
 * @param addr The accessory address to start writing to.
 * @param[in] data The 32 bytes of data to write to the accessory.
 *
 * @retval #JOYBUS_ACCESSORY_IO_STATUS_OK The data was written successfully.
 * @retval #JOYBUS_ACCESSORY_IO_STATUS_NO_PAK No accessory is present.
 * @retval #JOYBUS_ACCESSORY_IO_STATUS_BAD_CRC The data was not written successfully.
 */
int joybus_accessory_write(int port, uint16_t addr, const uint8_t *data);

#ifdef __cplusplus
}
#endif

/** @} */ /* joybus */

#endif
