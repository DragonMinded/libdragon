/**
 * @file eeprom.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief EEPROM support
 * @ingroup eeprom
 */

#include <string.h>
#include <stdlib.h>
#include "eeprom.h"
#include "joybus.h"
#include "joybus_commands.h"

/** @brief Joybus port for the cartridge connector */
#define EEPROM_PORT 4

static bool eeprom_maybe_busy = false;

static void eeprom_maybe_wait( void )
{
    if ( eeprom_maybe_busy )
    {
        joybus_cmd_identify_port_t cmd = { .send = {
            .command = JOYBUS_COMMAND_ID_IDENTIFY,
        } };
        do { joybus_exec_cmd_struct( EEPROM_PORT, cmd ); }
        while ( cmd.recv.status & JOYBUS_IDENTIFY_STATUS_EEPROM_BUSY );
        eeprom_maybe_busy = false;
    }
}

eeprom_type_t eeprom_present( void )
{
    joybus_cmd_identify_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_IDENTIFY,
    } };
    joybus_exec_cmd_struct( EEPROM_PORT, cmd );
    switch( cmd.recv.identifier )
    {
        case JOYBUS_IDENTIFIER_CART_EEPROM_16KBIT: return EEPROM_16K;
        case JOYBUS_IDENTIFIER_CART_EEPROM_4KBIT: return EEPROM_4K;
        default: return EEPROM_NONE;
    }
}

size_t eeprom_total_blocks( void )
{
    switch ( eeprom_present() )
    {
        case EEPROM_16K: return 256;
        case EEPROM_4K: return 64;
        default: return 0;
    }
}

void eeprom_read( uint8_t block, uint8_t * dest )
{
    eeprom_maybe_wait();

    joybus_cmd_eeprom_read_block_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_EEPROM_READ_BLOCK,
        .block = block,
    } };
    joybus_exec_cmd_struct( EEPROM_PORT, cmd );
    memcpy( dest, cmd.recv.data, EEPROM_BLOCK_SIZE );
}

uint8_t eeprom_write( uint8_t block, const uint8_t * src )
{
    eeprom_maybe_wait();

    joybus_cmd_eeprom_write_block_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_EEPROM_WRITE_BLOCK,
        .block = block,
    } };
    memcpy( cmd.send.data, src, EEPROM_BLOCK_SIZE );
    joybus_exec_cmd_struct( EEPROM_PORT, cmd );

    eeprom_maybe_busy = true;
    assert(cmd.recv.status == 0x00);
    return cmd.recv.status;
}

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
