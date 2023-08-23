/**
 * @file joybus_n64_accessory.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joybus N64 Accessory utilities
 * @ingroup joypad 
 */

#ifndef __JOYBUS_N64_ACCESSORY_H
#define __JOYBUS_N64_ACCESSORY_H

#include <stdint.h>

#include "joybus_commands.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JOYBUS_N64_ACCESSORY_DATA_SIZE 32
#define JOYBUS_N64_ACCESSORY_TRANSFER_BANK_SIZE 0x4000
#define JOYBUS_N64_ACCESSORY_TRANSFER_BANK_MASK 0x3FFF

/**
 * @anchor JOYBUS_N64_ACCESSORY_ADDR_MASK
 * @name Joybus N64 accessory address masks
 * @{
 */
#define JOYBUS_N64_ACCESSORY_ADDR_MASK_OFFSET    0xFFE0
#define JOYBUS_N64_ACCESSORY_ADDR_MASK_CHECKSUM  0x001F
/** @} */

/**
 * @anchor JOYBUS_N64_ACCESSORY_ADDR
 * @name Joybus N64 accessory addresses
 * @{
 */
#define JOYBUS_N64_ACCESSORY_ADDR_LABEL            0x0000
#define JOYBUS_N64_ACCESSORY_ADDR_PROBE            0x8000
#define JOYBUS_N64_ACCESSORY_ADDR_RUMBLE_MOTOR     0xC000
#define JOYBUS_N64_ACCESSORY_ADDR_BIO_PULSE        0xC000
#define JOYBUS_N64_ACCESSORY_ADDR_SNAP_STATE       0xC000
#define JOYBUS_N64_ACCESSORY_ADDR_TRANSFER_BANK    0xA000
#define JOYBUS_N64_ACCESSORY_ADDR_TRANSFER_STATUS  0xB000
#define JOYBUS_N64_ACCESSORY_ADDR_TRANSFER_CART    0xC000
/** @} */

/**
 * @anchor JOYBUS_N64_ACCESSORY_PROBE
 * @name Joybus N64 accessory probe values
 * @{
 */
#define JOYBUS_N64_ACCESSORY_PROBE_RUMBLE_PAK       0x80
#define JOYBUS_N64_ACCESSORY_PROBE_BIO_SENSOR       0x81
#define JOYBUS_N64_ACCESSORY_PROBE_TRANSFER_PAK_ON  0x84
#define JOYBUS_N64_ACCESSORY_PROBE_SNAP_STATION     0x85
#define JOYBUS_N64_ACCESSORY_PROBE_TRANSFER_PAK_OFF 0xFE
/** @} */

/**
 * @anchor JOYBUS_N64_TRANSFER_PAK_STATUS
 * @name Joybus N64 Transfer Pak status flags
 * @{
 */
#define JOYBUS_N64_TRANSFER_PAK_STATUS_ACCESS       (1<<0)
#define JOYBUS_N64_TRANSFER_PAK_STATUS_BOOTING      (1<<2)
#define JOYBUS_N64_TRANSFER_PAK_STATUS_RESET        (1<<3)
#define JOYBUS_N64_TRANSFER_PAK_STATUS_CART_PULLED  (1<<6)
#define JOYBUS_N64_TRANSFER_PAK_STATUS_POWER        (1<<7)
/** @} */

/**
 * @anchor JOYBUS_N64_SNAP_STATION_STATE
 * @name Joybus N64 Snap Station state values
 * @{
 */
#define JOYBUS_N64_SNAP_STATION_STATE_IDLE           0x00
#define JOYBUS_N64_SNAP_STATION_STATE_PRE_SAVE       0xCC
#define JOYBUS_N64_SNAP_STATION_STATE_POST_SAVE      0x33
#define JOYBUS_N64_SNAP_STATION_STATE_RESET_CONSOLE  0x5A
#define JOYBUS_N64_SNAP_STATION_STATE_PRE_ROLL       0x01
#define JOYBUS_N64_SNAP_STATION_STATE_CAPTURE_PHOTO  0x02
#define JOYBUS_N64_SNAP_STATION_STATE_POST_ROLL      0x04
#define JOYBUS_N64_SNAP_STATION_STATE_BUSY           0x08
/** @} */

/**
 * @brief Joybus N64 Transfer Pak Status wrapper.
 * 
 * Type union that unpacks the raw Transfer Pak status byte
 * to conveniently access the flags through a struct.
 * If you prefer bitwise operations, you can use
 * #JOYBUS_N64_TRANSFER_PAK_STATUS values as masks.
 */
typedef union
{
    uint8_t raw;
    struct __attribute__((packed))
    {
        unsigned access : 1;
        unsigned : 1;
        unsigned booting : 1;
        unsigned reset : 1;
        unsigned : 2;
        unsigned cart_pulled : 1;
        unsigned power : 1;
    };
} joybus_n64_transfer_pak_status_t;

/** @brief Joybus N64 accessory data CRC status values */
typedef enum
{
    JOYBUS_N64_ACCESSORY_DATA_CRC_STATUS_OK = 0,
    JOYBUS_N64_ACCESSORY_DATA_CRC_STATUS_NO_PAK,
    JOYBUS_N64_ACCESSORY_DATA_CRC_STATUS_MISMATCH,
} joybus_n64_accessory_data_crc_status_t;

uint16_t joybus_n64_accessory_addr_checksum(uint16_t addr);
uint8_t joybus_n64_accessory_data_checksum(const uint8_t *data);
int joybus_n64_accessory_data_crc_compare(const uint8_t *data, uint8_t data_crc);

void joybus_n64_accessory_read_async(int port, uint16_t addr, joybus_callback_t callback, void *ctx);
void joybus_n64_accessory_write_async(int port, uint16_t addr, const uint8_t *data, joybus_callback_t callback, void *ctx);

int joybus_n64_accessory_read_sync(int port, uint16_t addr, uint8_t *data);
int joybus_n64_accessory_write_sync(int port, uint16_t addr, const uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif
