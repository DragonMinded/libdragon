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

eeprom_type_t eeprom_present( void );
size_t eeprom_total_blocks( void );
void eeprom_read( uint8_t block, uint8_t * dest );
uint8_t eeprom_write( uint8_t block, const uint8_t * src );
void eeprom_read_bytes( uint8_t * dest, size_t start, size_t len );
void eeprom_write_bytes( const uint8_t * src, size_t start, size_t len );

#ifdef __cplusplus
}
#endif

#endif
