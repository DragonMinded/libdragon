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

/** @brief EEPROM is accessed in 8-byte blocks */
#define EEPROM_BLOCK_SIZE 8

#ifdef __cplusplus
extern "C" {
#endif

void joybus_exec( const void *inblock, void *outblock );
eeprom_type_t eeprom_present( void );
size_t eeprom_total_blocks( void );
void eeprom_read( int block, uint8_t * dest );
void eeprom_write( int block, const uint8_t * src );
void eeprom_read_bytes( uint8_t * dest, size_t start, size_t len );
void eeprom_write_bytes( const uint8_t * src, size_t start, size_t len );

#ifdef __cplusplus
}
#endif

/** @} */ /* joybus */

#endif
