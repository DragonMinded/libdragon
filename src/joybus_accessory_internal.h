/**
 * @file joybus_accessory_internal.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joybus accessory internal utilities
 * @ingroup joybus
 */

#ifndef __LIBDRAGON_JOYBUS_ACCESSORY_INTERNAL_H
#define __LIBDRAGON_JOYBUS_ACCESSORY_INTERNAL_H

#include <stdint.h>

#include "joybus.h"
#include "joybus_commands.h"
#include "joybus_internal.h"
#include "joybus_accessory.h"

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
#define JOYBUS_ACCESSORY_TRANSFER_BANK_SIZE 0x4000
/**
 * @brief Mask for Transfer Pak bank area address.
 */
#define JOYBUS_ACCESSORY_TRANSFER_BANK_MASK 0x3FFF

/**
 * @anchor JOYBUS_ACCESSORY_ADDR_MASK
 * @name Joybus N64 accessory address masks
 * @{
 */
/**
 * @brief Mask for Joybus N64 accessory read/write address offset.
 */
#define JOYBUS_ACCESSORY_ADDR_MASK_OFFSET    0xFFE0
/**
 * @brief Mask for Joybus N64 accessory read/write address checksum.
 */
#define JOYBUS_ACCESSORY_ADDR_MASK_CHECKSUM  0x001F
/** @} */ /* JOYBUS_ACCESSORY_ADDR_MASK */

/**
 * @anchor JOYBUS_ACCESSORY_ADDR
 * @name Joybus N64 accessory known address values
 * @{
 */
/**
 * @brief Controller Pak label address.
 */
#define JOYBUS_ACCESSORY_ADDR_LABEL            0x0000
/**
 * @brief Accessory probe address.
 */
#define JOYBUS_ACCESSORY_ADDR_PROBE            0x8000
/**
 * @brief Rumble Pak motor control address.
 */
#define JOYBUS_ACCESSORY_ADDR_RUMBLE_MOTOR     0xC000
/**
 * @brief Bio Sensor pulse read address.
 */
#define JOYBUS_ACCESSORY_ADDR_BIO_PULSE        0xC000
/**
 * @brief Pokemon Snap Station state address.
 */
#define JOYBUS_ACCESSORY_ADDR_SNAP_STATE       0xC000
/**
 * @brief Transfer Pak bank selection address.
 */
#define JOYBUS_ACCESSORY_ADDR_TRANSFER_BANK    0xA000
/**
 * @brief Transfer Pak status registers address.
 */
#define JOYBUS_ACCESSORY_ADDR_TRANSFER_STATUS  0xB000
/**
 * @brief Transfer Pak GB cartridge read/write address.
 */
#define JOYBUS_ACCESSORY_ADDR_TRANSFER_CART    0xC000
/** @} */ /* JOYBUS_ACCESSORY_ADDR */

/**
 * @anchor JOYBUS_ACCESSORY_PROBE
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
#define JOYBUS_ACCESSORY_PROBE_ABSENT           0x00
/**
 * @brief Rumble Pak identifier value.
 */
#define JOYBUS_ACCESSORY_PROBE_RUMBLE_PAK       0x80
/**
 * @brief Bio Sensor identifier value.
 */
#define JOYBUS_ACCESSORY_PROBE_BIO_SENSOR       0x81
/**
 * @brief Transfer Pak power-on identifier value.
 * 
 * When this value is written to the probe address,
 * the Transfer Pak will power on and respond to probe
 * reads with this value. 
 * 
 * If the accessory is not a Transfer Pak or the Transfer Pak
 * is not powered on, the probe read will return the
 * #JOYBUS_ACCESSORY_PROBE_ABSENT value.
 */
#define JOYBUS_ACCESSORY_PROBE_TRANSFER_PAK_ON  0x84
/**
 * @brief Pokemon Snap Station identifier value.
 */
#define JOYBUS_ACCESSORY_PROBE_SNAP_STATION     0x85
/**
 * @brief Transfer Pak power-off identifier value.
 * 
 * When this value is written to the probe address,
 * the Transfer Pak will power off and respond to probe
 * reads with the #JOYBUS_ACCESSORY_PROBE_ABSENT value.
 */
#define JOYBUS_ACCESSORY_PROBE_TRANSFER_PAK_OFF 0xFE
/** @} */ /* JOYBUS_ACCESSORY_PROBE */

/**
 * @anchor JOYBUS_SNAP_STATION_STATE
 * @name Joybus N64 Snap Station state values
 * @{
 */
/** @brief Snap Station "Idle" state. */
#define JOYBUS_SNAP_STATION_STATE_IDLE           0x00
/** @brief Snap Station "Pre-Save" state. */
#define JOYBUS_SNAP_STATION_STATE_PRE_SAVE       0xCC
/** @brief Snap Station "Post-Save" state. */
#define JOYBUS_SNAP_STATION_STATE_POST_SAVE      0x33
/** @brief Snap Station "Reset Console" state. */
#define JOYBUS_SNAP_STATION_STATE_RESET_CONSOLE  0x5A
/** @brief Snap Station "Pre-Roll" state. */
#define JOYBUS_SNAP_STATION_STATE_PRE_ROLL       0x01
/** @brief Snap Station "Capture Photo" state. */
#define JOYBUS_SNAP_STATION_STATE_CAPTURE_PHOTO  0x02
/** @brief Snap Station "Post-Roll" state. */
#define JOYBUS_SNAP_STATION_STATE_POST_ROLL      0x04
/** @brief Snap Station "Busy" state. */
#define JOYBUS_SNAP_STATION_STATE_BUSY           0x08
/** @} */ /* JOYBUS_SNAP_STATION_STATE */

/**
 * @anchor JOYBUS_TRANSFER_PAK_STATUS
 * @name Joybus N64 Transfer Pak status flags
 * @{
 */
/** @brief Transfer Pak "Access" status bit. */
#define JOYBUS_TRANSFER_PAK_STATUS_ACCESS       (1<<0)
/** @brief Transfer Pak "Booting" status bit. */
#define JOYBUS_TRANSFER_PAK_STATUS_BOOTING      (1<<2)
/** @brief Transfer Pak "Reset" status bit. */
#define JOYBUS_TRANSFER_PAK_STATUS_RESET        (1<<3)
/** @brief Transfer Pak "Cart Pulled" status bit. */
#define JOYBUS_TRANSFER_PAK_STATUS_CART_PULLED  (1<<6)
/** @brief Transfer Pak "Powered-On" status bit. */
#define JOYBUS_TRANSFER_PAK_STATUS_POWER        (1<<7)
/** @} */ /* JOYBUS_TRANSFER_PAK_STATUS */

/**
 * @brief Joybus N64 Transfer Pak Status wrapper.
 *
 * Type union that unpacks the raw Transfer Pak status byte
 * to conveniently access the flags through a struct.
 * If you prefer bitwise operations, you can use
 * @ref JOYBUS_TRANSFER_PAK_STATUS values as masks.
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
} joybus_transfer_pak_status_t;

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
uint16_t joybus_accessory_calculate_addr_checksum(uint16_t addr);

/**
 * @brief Calculates the CRC8 checksum for a Joybus N64 accessory read/write data block.
 * 
 * Uses a CRC8 algorithm with a seed of 0x00 and a polynomial of 0x85.
 * 
 * @param[in] data The 32-byte accessory read/write data block
 * 
 * @return The calculated CRC8 checksum for the data block 
 */
uint8_t joybus_accessory_calculate_data_crc(const uint8_t *data);

/**
 * @brief Calculates the CRC8 checksum for an accessory read/write data block and
 * compares it against the provided checksum.
 * 
 * @param[in] data The 32-byte accessory read/write data block
 * @param data_crc The CRC8 checksum to compare against
 * 
 * @retval #JOYBUS_ACCESSORY_IO_STATUS_OK The checksums match.
 * @retval #JOYBUS_ACCESSORY_IO_STATUS_NO_PAK The checksum indicates that no accessory is present.
 * @retval #JOYBUS_ACCESSORY_IO_STATUS_BAD_CRC The data checksum does not match the provided checksum.
 */
joybus_accessory_io_status_t joybus_accessory_compare_data_crc(const uint8_t *data, uint8_t data_crc);

/**
 * @brief Asynchronously perform a Joybus N64 accessory read command.
 * 
 * @param port The controller port of the accessory to read from.
 * @param addr The accessory address to read from.
 * @param callback A function pointer to call when the operation completes.
 * @param ctx A user data pointer to pass into the callback function.
 */
void joybus_accessory_read_async(int port, uint16_t addr, joybus_callback_t callback, void *ctx);

/**
 * @brief Asynchronously perform a Joybus N64 accessory write command.
 * 
 * @param port The controller port of the accessory to write to.
 * @param addr The accessory address to write to.
 * @param[in] data The data to write to the accessory.
 * @param callback A function pointer to call when the operation completes.
 * @param ctx A user data pointer to pass into the callback function.
 */
void joybus_accessory_write_async(int port, uint16_t addr, const uint8_t *data, joybus_callback_t callback, void *ctx);

#ifdef __cplusplus
}
#endif

/** @} */ /* joybus */

#endif
