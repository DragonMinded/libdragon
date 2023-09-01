/**
 * @file joybus_n64_accessory.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joybus N64 Accessory utilities
 * @ingroup joybus
 */

#ifndef __JOYBUS_N64_ACCESSORY_H
#define __JOYBUS_N64_ACCESSORY_H

#include <stdint.h>

/**
 * @addtogroup joybus
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Joybus N64 accessory read/write status values */
typedef enum
{
    /** @brief N64 accessory data communcation was successful. */
    JOYBUS_N64_ACCESSORY_IO_STATUS_OK        = 0,
    /** @brief No N64 controller is connected. */
    JOYBUS_N64_ACCESSORY_IO_STATUS_NO_DEVICE = -1,
    /** @brief No N64 accessory is connected. */
    JOYBUS_N64_ACCESSORY_IO_STATUS_NO_PAK    = -2,
    /** @brief N64 accessory communication was not successful. */
    JOYBUS_N64_ACCESSORY_IO_STATUS_BAD_CRC   = -3,
} joybus_n64_accessory_io_status_t;

int joybus_n64_accessory_read_sync(int port, uint16_t addr, uint8_t *data);
int joybus_n64_accessory_write_sync(int port, uint16_t addr, const uint8_t *data);

#ifdef __cplusplus
}
#endif

/** @} */ /* joybus */

#endif
