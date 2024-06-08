/**
 * @file eeprom.h
 * @brief EEPROM support
 * @ingroup eeprom
 */

#ifndef __LIBDRAGON_EEPROM_H
#define __LIBDRAGON_EEPROM_H

/**
 * @defgroup peripherals Peripherals subsystem
 * @ingroup libdragon
 * @brief Management of serial peripherals, reachable through Joybus
 * 
 * This module contains higher-level routine to access different peripherals
 * that are accessible via the JoyBus protocol and the PIF serial chip.
 */

/**
 * @defgroup eeprom EEPROM subsystem
 * @ingroup peripherals
 * @brief Management of EEPROM for saves
 * 
 * This subsytem is made of two different APIs:
 * 
 *  * A lower-level API (eeprom.h) for raw low-level access to EEPROM bytes
 *  * A higher-level API (eepromfs.h) for higher-level access to EEPROM
 *    with structured data.
 *
 */

#include <stdint.h>

/**
 * @brief EEPROM Probe Values
 * @see #eeprom_present
 */
typedef enum eeprom_type_t
{
    /** @brief No EEPROM present */
    EEPROM_NONE = 0,
    /** @brief 4 kilobit (64-block) EEPROM present */
    EEPROM_4K   = 1,
    /** @brief 16 kilobit (256-block) EEPROM present */
    EEPROM_16K  = 2
} eeprom_type_t; 

/**
 * @brief Size of an EEPROM save block in bytes.
 */
#define EEPROM_BLOCK_SIZE 8

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Probe the EEPROM interface on the cartridge.
 *
 * Inspect the identifier half-word of the EEPROM status response to
 * determine which EEPROM save type is available (if any).
 *
 * @return which EEPROM type was detected on the cartridge.
 */
eeprom_type_t eeprom_present( void );

/**
 * @brief Determine how many blocks of EEPROM exist on the cartridge.
 *
 * @return 0 if EEPROM was not detected
 *         or the number of EEPROM 8-byte save blocks available.
 */
size_t eeprom_total_blocks( void );

/**
 * @brief Read a block from EEPROM.
 *
 * @param[in]  block
 *             Block to read data from. Joybus accesses EEPROM in 8-byte blocks.
 *
 * @param[out] dest
 *             Destination buffer for the eight bytes read from EEPROM.
 */
void eeprom_read( uint8_t block, uint8_t * dest );

/**
 * @brief Write a block to EEPROM.
 *
 * @param[in] block
 *            Block to write data to. Joybus accesses EEPROM in 8-byte blocks.
 *
 * @param[in] src
 *            Source buffer for the eight bytes of data to write to EEPROM.
 *
 * @return the EEPROM status byte
 */
uint8_t eeprom_write( uint8_t block, const uint8_t * src );

/**
 * @brief Read a buffer of bytes from EEPROM.
 *
 * This is a high-level convenience helper that abstracts away the
 * one-at-a-time EEPROM block access pattern.
 *
 * @param[out] dest
 *             Destination buffer to read data into
 * @param[in]  start
 *             Byte offset in EEPROM to start reading data from
 * @param[in]  len
 *             Byte length of data to read into buffer
 */
void eeprom_read_bytes( uint8_t * dest, size_t start, size_t len );

/**
 * @brief Write a buffer of bytes to EEPROM.
 *
 * This is a high-level convenience helper that abstracts away the
 * one-at-a-time EEPROM block access pattern.
 *
 * Each EEPROM block write takes approximately 15 milliseconds;
 * this operation may block for a while with large buffer sizes:
 *
 * * 4k EEPROM: 64 blocks * 15ms = 960ms!
 * * 16k EEPROM: 256 blocks * 15ms = 3840ms!
 *
 * You may want to pause audio before calling this.
 *
 * @param[in] src
 *            Source buffer containing data to write
 *
 * @param[in] start
 *            Byte offset in EEPROM to start writing data to
 *
 * @param[in] len
 *            Byte length of the src buffer
 */
void eeprom_write_bytes( const uint8_t * src, size_t start, size_t len );

#ifdef __cplusplus
}
#endif

#endif
