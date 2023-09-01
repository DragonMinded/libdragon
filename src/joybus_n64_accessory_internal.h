/**
 * @file joybus_n64_accessory_internal.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joybus N64 accessory internal utilities
 * @ingroup joybus
 */

#ifndef __JOYBUS_N64_ACCESSORY_INTERNAL_H
#define __JOYBUS_N64_ACCESSORY_INTERNAL_H

#include <stdint.h>

#include "joybus.h"
#include "joybus_commands.h"
#include "joybus_internal.h"
#include "joybus_n64_accessory.h"

/**
 * @addtogroup joybus
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Size of Transfer Pak bank area in bytes.
 */
#define JOYBUS_N64_ACCESSORY_TRANSFER_BANK_SIZE 0x4000
/**
 * @brief Mask for Transfer Pak bank area address.
 */
#define JOYBUS_N64_ACCESSORY_TRANSFER_BANK_MASK 0x3FFF

/**
 * @anchor JOYBUS_N64_ACCESSORY_ADDR_MASK
 * @name Joybus N64 accessory address masks
 * @{
 */
/**
 * @brief Mask for Joybus N64 accessory read/write address offset.
 */
#define JOYBUS_N64_ACCESSORY_ADDR_MASK_OFFSET    0xFFE0
/**
 * @brief Mask for Joybus N64 accessory read/write address checksum.
 */
#define JOYBUS_N64_ACCESSORY_ADDR_MASK_CHECKSUM  0x001F
/** @} */ /* JOYBUS_N64_ACCESSORY_ADDR_MASK */

/**
 * @anchor JOYBUS_N64_ACCESSORY_ADDR
 * @name Joybus N64 accessory known address values
 * @{
 */
/**
 * @brief Controller Pak label address.
 */
#define JOYBUS_N64_ACCESSORY_ADDR_LABEL            0x0000
/**
 * @brief Accessory probe address.
 */
#define JOYBUS_N64_ACCESSORY_ADDR_PROBE            0x8000
/**
 * @brief Rumble Pak motor control address.
 */
#define JOYBUS_N64_ACCESSORY_ADDR_RUMBLE_MOTOR     0xC000
/**
 * @brief Bio Sensor pulse read address.
 */
#define JOYBUS_N64_ACCESSORY_ADDR_BIO_PULSE        0xC000
/**
 * @brief Pokemon Snap Station state address.
 */
#define JOYBUS_N64_ACCESSORY_ADDR_SNAP_STATE       0xC000
/**
 * @brief Transfer Pak bank selection address.
 */
#define JOYBUS_N64_ACCESSORY_ADDR_TRANSFER_BANK    0xA000
/**
 * @brief Transfer Pak status registers address.
 */
#define JOYBUS_N64_ACCESSORY_ADDR_TRANSFER_STATUS  0xB000
/**
 * @brief Transfer Pak GB cartridge read/write address.
 */
#define JOYBUS_N64_ACCESSORY_ADDR_TRANSFER_CART    0xC000
/** @} */ /* JOYBUS_N64_ACCESSORY_ADDR */

/**
 * @anchor JOYBUS_N64_ACCESSORY_PROBE
 * @name Joybus N64 accessory probe values
 * @{
 */
/**
 * @brief Absent accessory identifier value.
 * 
 * For Rumble Pak, Transfer Pak, and Snap Station,
 * you must write the expected identifier to the probe address
 * and then read it back. If the expected accessory is not
 * connected, this value will be returned.
 */
#define JOYBUS_N64_ACCESSORY_PROBE_ABSENT           0x00
/**
 * @brief Rumble Pak identifier value.
 */
#define JOYBUS_N64_ACCESSORY_PROBE_RUMBLE_PAK       0x80
/**
 * @brief Bio Sensor identifier value.
 */
#define JOYBUS_N64_ACCESSORY_PROBE_BIO_SENSOR       0x81
/**
 * @brief Transfer Pak power-on identifier value.
 * 
 * When this value is written to the probe address,
 * the Transfer Pak will power on and respond to probe
 * reads with this value. 
 * 
 * If the accessory is not a Transfer Pak or the Transfer Pak
 * is not powered on, the probe read will return the
 * #JOYBUS_N64_ACCESSORY_PROBE_ABSENT value.
 */
#define JOYBUS_N64_ACCESSORY_PROBE_TRANSFER_PAK_ON  0x84
/**
 * @brief Pokemon Snap Station identifier value.
 */
#define JOYBUS_N64_ACCESSORY_PROBE_SNAP_STATION     0x85
/**
 * @brief Transfer Pak power-off identifier value.
 * 
 * When this value is written to the probe address,
 * the Transfer Pak will power off and respond to probe
 * reads with the #JOYBUS_N64_ACCESSORY_PROBE_ABSENT value.
 */
#define JOYBUS_N64_ACCESSORY_PROBE_TRANSFER_PAK_OFF 0xFE
/** @} */ /* JOYBUS_N64_ACCESSORY_PROBE */

/**
 * @anchor JOYBUS_N64_SNAP_STATION_STATE
 * @name Joybus N64 Snap Station state values
 * @{
 */
/** @brief Snap Station "Idle" state. */
#define JOYBUS_N64_SNAP_STATION_STATE_IDLE           0x00
/** @brief Snap Station "Pre-Save" state. */
#define JOYBUS_N64_SNAP_STATION_STATE_PRE_SAVE       0xCC
/** @brief Snap Station "Post-Save" state. */
#define JOYBUS_N64_SNAP_STATION_STATE_POST_SAVE      0x33
/** @brief Snap Station "Reset Console" state. */
#define JOYBUS_N64_SNAP_STATION_STATE_RESET_CONSOLE  0x5A
/** @brief Snap Station "Pre-Roll" state. */
#define JOYBUS_N64_SNAP_STATION_STATE_PRE_ROLL       0x01
/** @brief Snap Station "Capture Photo" state. */
#define JOYBUS_N64_SNAP_STATION_STATE_CAPTURE_PHOTO  0x02
/** @brief Snap Station "Post-Roll" state. */
#define JOYBUS_N64_SNAP_STATION_STATE_POST_ROLL      0x04
/** @brief Snap Station "Busy" state. */
#define JOYBUS_N64_SNAP_STATION_STATE_BUSY           0x08
/** @} */ /* JOYBUS_N64_SNAP_STATION_STATE */

/**
 * @anchor JOYBUS_N64_TRANSFER_PAK_STATUS
 * @name Joybus N64 Transfer Pak status flags
 * @{
 */
/** @brief Transfer Pak "Access" status bit. */
#define JOYBUS_N64_TRANSFER_PAK_STATUS_ACCESS       (1<<0)
/** @brief Transfer Pak "Booting" status bit. */
#define JOYBUS_N64_TRANSFER_PAK_STATUS_BOOTING      (1<<2)
/** @brief Transfer Pak "Reset" status bit. */
#define JOYBUS_N64_TRANSFER_PAK_STATUS_RESET        (1<<3)
/** @brief Transfer Pak "Cart Pulled" status bit. */
#define JOYBUS_N64_TRANSFER_PAK_STATUS_CART_PULLED  (1<<6)
/** @brief Transfer Pak "Powered-On" status bit. */
#define JOYBUS_N64_TRANSFER_PAK_STATUS_POWER        (1<<7)
/** @} */ /* JOYBUS_N64_TRANSFER_PAK_STATUS */

/**
 * @brief Joybus N64 Transfer Pak Status wrapper.
 *
 * Type union that unpacks the raw Transfer Pak status byte
 * to conveniently access the flags through a struct.
 * If you prefer bitwise operations, you can use
 * @ref JOYBUS_N64_TRANSFER_PAK_STATUS values as masks.
 */
typedef union
{
    /** @brief Transfer Pak raw status byte. */
    uint8_t raw;
    /// @cond
    struct __attribute__((packed))
    {
    /// @endcond
        /** @brief Transfer Pak "Access" status bit. */
        unsigned access : 1;
        unsigned : 1;  ///< Unused padding
        /** @brief Transfer Pak "Booting" status bit. */
        unsigned booting : 1;
        /** @brief Transfer Pak "Reset" status bit. */
        unsigned reset : 1;
        unsigned : 2; ///< Unused padding
        /** @brief Transfer Pak "Cart Pulled" status bit. */
        unsigned cart_pulled : 1;
        /** @brief Transfer Pak "Powered-On" status bit. */
        unsigned power : 1;
    /// @cond
    };
    /// @endcond
} joybus_n64_transfer_pak_status_t;

uint16_t joybus_n64_accessory_calculate_addr_checksum(uint16_t addr);
uint8_t joybus_n64_accessory_calculate_data_crc(const uint8_t *data);
joybus_n64_accessory_io_status_t joybus_n64_accessory_compare_data_crc(const uint8_t *data, uint8_t data_crc);

void joybus_n64_accessory_read_async(int port, uint16_t addr, joybus_callback_t callback, void *ctx);
void joybus_n64_accessory_write_async(int port, uint16_t addr, const uint8_t *data, joybus_callback_t callback, void *ctx);

#ifdef __cplusplus
}
#endif

/** @} */ /* joybus */

#endif
