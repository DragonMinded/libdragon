/**
 * @file eeprom.c
 * @brief EEPROM support
 * @ingroup eeprom
 */

#include <string.h>
#include <stdlib.h>
#include "eeprom.h"
#include "joybus.h"

/**
 * @brief Read the status of the EEPROM.
 *
 * The EEPROM status response contains a byte-swapped identifier half-word that
 * indicates which size EEPROM is present on the cartridge and a status byte.
 *
 * @return the normalized Joybus EEPROM status response data.
 */
static uint32_t eeprom_status( void )
{
    const uint64_t input[JOYBUS_BLOCK_DWORDS] =
    {
        0x00000000ff010300,
        0xfffffffffe000000,
        0,
        0,
        0,
        0,
        0,
        1
    };
    uint64_t output[JOYBUS_BLOCK_DWORDS];

    joybus_exec( input, output );

    uint8_t * recv_bytes = (uint8_t *)&output[1];

    return (
        /* Intentional un-swap of identifier bytes */
        ((uint32_t)recv_bytes[1] << 16) |
        ((uint32_t)recv_bytes[0] << 8)  |
        recv_bytes[2]
    );
}

/**
 * @brief Probe the EEPROM interface on the cartridge.
 *
 * Inspect the identifier half-word of the EEPROM status response to
 * determine which EEPROM save type is available (if any).
 *
 * @return which EEPROM type was detected on the cartridge.
 */
eeprom_type_t eeprom_present( void )
{
    switch( eeprom_status() >> 8 )
    {
        case 0xC000: return EEPROM_16K;
        case 0x8000: return EEPROM_4K;
        default: return EEPROM_NONE;
    }
}

/**
 * @brief Determine how many blocks of EEPROM exist on the cartridge.
 *
 * @return 0 if EEPROM was not detected
 *         or the number of EEPROM 8-byte save blocks available.
 */
size_t eeprom_total_blocks( void )
{
    switch ( eeprom_present() )
    {
        case EEPROM_16K: return 256;
        case EEPROM_4K: return 64;
        default: return 0;
    }
}

/**
 * @brief Read a block from EEPROM.
 *
 * @param[in]  block
 *             Block to read data from. Joybus accesses EEPROM in 8-byte blocks.
 *
 * @param[out] dest
 *             Destination buffer for the eight bytes read from EEPROM.
 */
void eeprom_read( uint8_t block, uint8_t * dest )
{
    const uint64_t input[JOYBUS_BLOCK_DWORDS] =
    {
        0x0000000002080400 | block,
        0xffffffffffffffff,
        0xfe00000000000000,
        0,
        0,
        0,
        0,
        1
    };
    uint64_t output[JOYBUS_BLOCK_DWORDS];

    joybus_exec( input, output );

    memcpy( dest, &output[1], EEPROM_BLOCK_SIZE );
}

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
uint8_t eeprom_write( uint8_t block, const uint8_t * src )
{
    uint64_t input[JOYBUS_BLOCK_DWORDS] =
    {
        0x000000000a010500 | block,
        0x0000000000000000,
        0xfffe000000000000,
        0,
        0,
        0,
        0,
        1
    };
    uint64_t output[JOYBUS_BLOCK_DWORDS];

    memcpy( &input[1], src, EEPROM_BLOCK_SIZE );

    joybus_exec( input, output );

    return output[2] >> 56;
}

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
void eeprom_read_bytes( uint8_t * dest, size_t start, size_t len )
{
    size_t bytes_left = len;
    uint8_t buf[EEPROM_BLOCK_SIZE];
    uint8_t current_block = start / EEPROM_BLOCK_SIZE;
    // If we need to read a partial block to start off...
    size_t block_offset = start % EEPROM_BLOCK_SIZE;
    if (block_offset)
    {
        eeprom_read( current_block++, buf );
        bytes_left -= (EEPROM_BLOCK_SIZE - block_offset);
        while (block_offset < EEPROM_BLOCK_SIZE)
        {
            *dest++ = buf[block_offset++];
        }
    }
    // Read whole blocks at a time
    while ( bytes_left >= EEPROM_BLOCK_SIZE )
    {
        eeprom_read( current_block++, dest );
        dest += EEPROM_BLOCK_SIZE;
        bytes_left -= EEPROM_BLOCK_SIZE;
    }
    // If we need to read a partial block at the end...
    if (bytes_left)
    {
        eeprom_read( current_block, buf );
        memcpy( dest, buf, bytes_left );
    }
}

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
void eeprom_write_bytes( const uint8_t * src, size_t start, size_t len )
{
    size_t bytes_left = len;
    uint8_t buf[EEPROM_BLOCK_SIZE];
    uint8_t current_block = start / EEPROM_BLOCK_SIZE;
    // If we need to write a partial block to start off...
    size_t block_offset = start % EEPROM_BLOCK_SIZE;
    if (block_offset)
    {
        eeprom_read( current_block, buf );
        bytes_left -= (EEPROM_BLOCK_SIZE - block_offset);
        while (block_offset < EEPROM_BLOCK_SIZE)
        {
            buf[block_offset++] = *src++;
        }
        eeprom_write( current_block++, buf );
    }
    // Write whole blocks at a time
    while (bytes_left >= EEPROM_BLOCK_SIZE)
    {
        memcpy( buf, src, EEPROM_BLOCK_SIZE );
        eeprom_write( current_block++, buf );
        src += EEPROM_BLOCK_SIZE;
        bytes_left -= EEPROM_BLOCK_SIZE;
    }
    // If we need to write a partial block at the end...
    if (bytes_left)
    {
        eeprom_read( current_block, buf );
        memcpy( buf, src, bytes_left );
        eeprom_write( current_block, buf );
    }
}
