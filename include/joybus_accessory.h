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

int joybus_accessory_read_sync(int port, uint16_t addr, uint8_t *data);
int joybus_accessory_write_sync(int port, uint16_t addr, const uint8_t *data);

#ifdef __cplusplus
}
#endif

/** @} */ /* joybus */

#endif
