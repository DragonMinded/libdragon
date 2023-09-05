/**
 * @file joybus.h
 * @brief Joybus Subsystem
 * @ingroup joybus
 */
#ifndef __LIBDRAGON_JOYBUS_H
#define __LIBDRAGON_JOYBUS_H

#include <stddef.h>
#include <stdint.h>

/**
 * @addtogroup joybus
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @anchor JOYBUS_PAYLOAD_SIZES
 * @name Joybus payload sizes
 * @{
 */
/**
 * @brief Size of a Joybus input/output block in bytes.
 */
#define JOYBUS_BLOCK_SIZE 64
/**
 * @brief Size of a Joybus input/output block in double-words.
 */
#define JOYBUS_BLOCK_DWORDS ( JOYBUS_BLOCK_SIZE / sizeof(uint64_t) )
/** 
 * @brief Size of a Joybus N64 accessory read/write payload in bytes. 
 */
#define JOYBUS_ACCESSORY_DATA_SIZE  32
/** @} */ /* JOYBUS_PAYLOAD_SIZES */

/**
 * @brief Count of Joybus ports.
 * 
 * The N64 has four joypad ports, plus one additional port exposed
 * on the cartridge connector for EEPROM and real-time clock support.
 */
#define JOYBUS_PORT_COUNT 5

/**
 * @brief Joypad Identifier type
 *
 * For known values, see @ref JOYBUS_IDENTIFIER and
 * @ref JOYBUS_IDENTIFIER_GCN_BITFIELD.
 * 
 * @note For GameCube peripherals, the Joybus identifier should be
 * interpreted as a bitfield rather than a single value.
 */
typedef uint16_t joybus_identifier_t;

/**
 * @anchor JOYBUS_IDENTIFIER
 * @name Joybus identifier values
 * @{
 */
/**
 * @brief Joybus identifier for an unknown or malfunctioning device.
 */
#define JOYBUS_IDENTIFIER_UNKNOWN               0x0000
/**
 * @brief Joybus identifier for a port with no device connected.
 */
#define JOYBUS_IDENTIFIER_NONE                  0xFFFF
/**
 * @brief Joybus identifier for the Nintendo 64 voice recognition peripheral (NUS-020).
 * 
 * Also known as VRU in North America and VRS in Japan.
 */
#define JOYBUS_IDENTIFIER_N64_VOICE_RECOGNITION 0x0001
/**
 * @brief Joybus identifier for the Nintendo 64 Randnet keyboard peripheral (RND-001).
 */
#define JOYBUS_IDENTIFIER_N64_RANDNET_KEYBOARD  0x0002
/**
 * @brief Joybus identifier for the unreleased 64GB Link Cable.
 */
#define JOYBUS_IDENTIFIER_64GB_LINK_CABLE       0x0003
/**
 * @brief Joybus identifier for a Game Boy Advance Link Cable (DOL-011).
 */
#define JOYBUS_IDENTIFIER_GBA_LINK_CABLE        0x0004
/**
 * @brief Joybus identifier for cartridge-based real-time clock.
 */
#define JOYBUS_IDENTIFIER_CART_RTC              0x0010
/**
 * @brief Joybus identifier for cartridge-based 4Kbit EEPROM save type.
 */
#define JOYBUS_IDENTIFIER_CART_EEPROM_4KBIT     0x0080
/**
 * @brief Joybus identifier for cartridge-based 16Kbit EEPROM save type.
 */
#define JOYBUS_IDENTIFIER_CART_EEPROM_16KBIT    0x00C0
/**
 * @brief Joybus identifier for a standard Nintendo 64 controller (NUS-005).
 */
#define JOYBUS_IDENTIFIER_N64_CONTROLLER        0x0500
/**
 * @brief Joybus identifier for the Nintendo 64 mouse peripheral (NUS-017).
 */
#define JOYBUS_IDENTIFIER_N64_MOUSE             0x0200
/** @} */

/**
 * @anchor JOYBUS_IDENTIFIER_GCN_BITFIELD
 * @name Joybus identifier bitfield for GameCube peripherals
 * 
 * Note that for GameCube peripherals, the Joybus identifier is
 * interpreted as a bitfield rather than a single value.
 * 
 * In particular, Wavebird controllers will return a different identifiers
 * depending on wireless state.
 * 
 * To identify a device that acts like a standard GameCube controller,
 * check the #JOYBUS_IDENTIFIER_MASK_PLATFORM and the
 * #JOYBUS_IDENTIFIER_MASK_GCN_CONTROLLER values.
 * 
 * @{
 */
/**
 * @brief Joybus identifier platform bitfield mask.
 * 
 * Bits 11-12 of the Joybus identifier signify the intended platform:
 * 
 * * Bit 11 is set for GameCube devices.
 * * Bit 12 is zero for all known devices.
*/
#define JOYBUS_IDENTIFIER_MASK_PLATFORM         0x1800
/**
 * @brief GameCube Joybus identifier platform value.
 * 
 * Bit 11 of the Joybus identifier is one for GameCube devices.
 * Bit 12 of the Joybus identifier is zero for all known devices.
 */
#define JOYBUS_IDENTIFIER_PLATFORM_GCN          0x0800
/**
 * @brief Joybus identifier GameCube standard controller flag.
 * 
 * For GameCube platform devices, this bit is set if the device acts like a standard controller.
 */
#define JOYBUS_IDENTIFIER_MASK_GCN_CONTROLLER   0x0100
/**
 * @brief Joybus identifier GameCube rumble support flag.
 * 
 * For GameCube controllers, this bit is set if the controller DOES NOT support rumble functionality.
 */
#define JOYBUS_IDENTIFIER_MASK_GCN_NORUMBLE     0x2000
/**
 * @brief Joybus identifier GameCube wireless flag.
 * 
 * For GameCube controllers, this bit is set if the controller is a wireless controller.
 */
#define JOYBUS_IDENTIFIER_MASK_GCN_WIRELESS     0x8000
/** @} */ /* JOYBUS_IDENTIFIER_GCN_BITFIELD */

/**
 * @anchor JOYBUS_IDENTIFY_STATUS
 * @name Joybus identify status values
 * @{
 */
/**
 * @brief Joybus identify status byte mask for N64 accessory presence values.
 */
#define JOYBUS_IDENTIFY_STATUS_ACCESSORY_MASK             0x03
/**
 * @brief Joybus identify status for an N64 controller that does not support accessories.
 * 
 * Some third-party controllers incorrectly use this status to mean absence of an accessory.
 * Therefore, this value is treated as a synonym for JOYBUS_IDENTIFY_STATUS_ACCESSORY_ABSENT.
 */
#define JOYBUS_IDENTIFY_STATUS_ACCESSORY_UNSUPPORTED      0x00
/**
 * @brief Joybus identify status for an N64 controller with an accessory present.
 */
#define JOYBUS_IDENTIFY_STATUS_ACCESSORY_PRESENT          0x01
/**
 * @brief Joybus identify status for an N64 controller with no accessory present.
 */
#define JOYBUS_IDENTIFY_STATUS_ACCESSORY_ABSENT           0x02
/**
 * @brief Joybus identify status for an N64 controller with an accessory present
 * that has changed since it was last identified.
 */
#define JOYBUS_IDENTIFY_STATUS_ACCESSORY_CHANGED          0x03
/**
 * @brief Joybus identify status bit for a VRU/VRS that is initialized and ready.
 */
#define JOYBUS_IDENTIFY_STATUS_VOICE_RECOGNITON_READY     0x01
/**
 * @brief Joybus identify status bit that signifies the previous accessory command
 * had a checksum error.
 */
#define JOYBUS_IDENTIFY_STATUS_COMMAND_CHECKSUM_ERROR     0x04
/**
 * @brief Joybus identify status bit for GameCube controllers that indicates whether
 * the rumble motor is currently active.
 */
#define JOYBUS_IDENTIFY_STATUS_GCN_RUMBLE_ACTIVE          0x08
/**
 * @brief Joybus identify status bit for EEPROM devices that indicates a write is in-progress.
 */
#define JOYBUS_IDENTIFY_STATUS_EEPROM_BUSY                0x80
/** @} */

void joybus_exec( const void * inblock, void * outblock );

/**
 * @brief Sends a Joybus command to notify N64Digital of the current Game ID.
 * 
 * This function is mostly intended to be used by flashcart menu software.
 * 
 * @see https://gitlab.com/pixelfx-public/n64-game-id#n64-game-id-per-game-settings-for-n64digital
 * 
 * @param rom_check_code ROM check code (ROM header bytes 0x10-0x17)
 * @param media_format Media category code (ROM header byte 0x3B)
 * @param region_code Region code (ROM header byte 0x3E)
 */
void joybus_send_game_id(uint64_t rom_check_code, uint8_t media_format, uint8_t region_code);

/**
 * @brief Sends a Joybus command to clear the current Game ID.
 * 
 * This function is mostly intended to be used by flashcart menu software.
 * 
 * @see https://gitlab.com/pixelfx-public/n64-game-id#special-ids
 */
void joybus_clear_game_id(void);

#ifdef __cplusplus
}
#endif

/** @} */ /* joybus */

#endif
