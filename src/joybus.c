/**
 * @file joybus.c
 * @brief Joybus Subsystem
 * @ingroup joybus
 */

#include <string.h>
#include "libdragon.h"
#include "regsinternal.h"

/**
 * @defgroup joybus Joybus Subsystem
 * @ingroup libdragon
 * @brief Joybus peripheral interface.
 *
 * The Joybus subsystem is in charge of communication with all controllers,
 * accessories, and peripherals plugged into the N64 controller ports as well
 * as some peripherals on the cartridge. The Joybus subsystem is responsible
 * for communicating with the serial interface (SI) registers to send commands
 * to controllers (including Controller Paks, Rumble Paks, and Transfer Paks),
 * the VRU, EEPROM save memory, and the cartridge-based real-time clock.
 *
 * For Controller Pak functions, refer to the
 * @ref mempak "Mempak Filesystem Routines".
 *
 * For Transfer Pak functions, refer to the
 * @ref transferpak "Transfer Pak Interface".
 *
 * For all other controller-related functions, refer to the
 * @ref controller "Controller Subsystem".
 *
 * For real-time clock functions, refer to the
 * @ref rtc "Real-time Clock Subsystem".
 *
 * @{
 */

/**
 * @name SI status register bit definitions
 * @{
 */

/** @brief SI DMA busy */
#define SI_STATUS_DMA_BUSY  ( 1 << 0 )
/** @brief SI IO busy */
#define SI_STATUS_IO_BUSY   ( 1 << 1 )
/** @} */

/** @brief Structure used to interact with SI registers */
static volatile struct SI_regs_s * const SI_regs = (struct SI_regs_s *)0xa4800000;
/** @brief Location of the PIF RAM */
static void * const PIF_RAM = (void *)0x1fc007c0;

/**
 * @brief Wait until the SI is finished with a DMA request
 */
static void __SI_DMA_wait( void )
{
    while (SI_regs->status & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY)) ;
}

/**
 * @brief Send a block of data to the PIF and fetch the result
 *
 * @param[in]  inblock
 *             The formatted block to send to the PIF
 * @param[out] outblock
 *             The buffer to place the output from the PIF
 */
void joybus_exec( const void *inblock, void *outblock )
{
    volatile uint64_t inblock_temp[8];
    volatile uint64_t outblock_temp[8];

    data_cache_hit_writeback_invalidate(inblock_temp, 64);
    memcpy(UncachedAddr(inblock_temp), inblock, 64);

    /* Be sure another thread doesn't get into a resource fight */
    disable_interrupts();

    __SI_DMA_wait();

    SI_regs->DRAM_addr = inblock_temp; // only cares about 23:0
    MEMORY_BARRIER();
    SI_regs->PIF_addr_write = PIF_RAM; // is it really ever anything else?
    MEMORY_BARRIER();

    __SI_DMA_wait();

    data_cache_hit_writeback_invalidate(outblock_temp, 64);

    SI_regs->DRAM_addr = outblock_temp;
    MEMORY_BARRIER();
    SI_regs->PIF_addr_read = PIF_RAM;
    MEMORY_BARRIER();

    __SI_DMA_wait();

    /* Now that we've copied, its safe to let other threads go */
    enable_interrupts();

    memcpy(outblock, UncachedAddr(outblock_temp), 64);
}

/**
 * @brief Probe the EEPROM interface on the cartridge.
 *
 * @return Which EEPROM type was detected on the cartridge.
 */
eeprom_type_t eeprom_present( void )
{
    static const uint64_t SI_eeprom_status_block[8] =
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
    static uint64_t output[8];

    joybus_exec( SI_eeprom_status_block, output );

    /* We are looking at the second byte returned, which
     * signifies which size EEPROM (if any) is present.*/
    switch( (output[1] >> 48) & 0xFF )
    {
        case 0xC0: return EEPROM_16K;
        case 0x80: return EEPROM_4K;
        default: return EEPROM_NONE;
    }
}

/**
 * @brief Return how many blocks of EEPROM exist on the cartridge.
 *
 * @return The capacity of the detected EEPROM type, or 0 if no EEPROM is present.
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
 * @brief Read a block from EEPROM
 *
 * @param[in]  block
 *             Block to read data from. The N64 accesses EEPROM in 8-byte blocks.
 * @param[out] dest
 *             Destination buffer for the eight bytes read from EEPROM.
 */
void eeprom_read( int block, uint8_t * dest )
{
    static uint64_t SI_eeprom_read_block[8] =
    {
        0x0000000002080400,				// LSB is block
        0xffffffffffffffff,				// return data will be this quad
        0xfe00000000000000,
        0,
        0,
        0,
        0,
        1
    };
    static uint64_t output[8];

    SI_eeprom_read_block[0] = 0x0000000002080400 | (block & 255);
    joybus_exec( SI_eeprom_read_block, output );
    memcpy( dest, &output[1], EEPROM_BLOCK_SIZE );
}

/**
 * @brief Write a block to EEPROM
 *
 * @param[in] block
 *            Block to write data to. The N64 accesses EEPROM in 8-byte blocks.
 * @param[in] src
 *            Source buffer for the eight bytes of data to write to EEPROM.
 */
void eeprom_write( int block, const uint8_t * src )
{
    static uint64_t SI_eeprom_write_block[8] =
    {
        0x000000000a010500,				// LSB is block
        0x0000000000000000,				// send data is this quad
        0xfffe000000000000,				// MSB will be status of write
        0,
        0,
        0,
        0,
        1
    };
    static uint64_t output[8];

    SI_eeprom_write_block[0] = 0x000000000a010500 | (block & 255);
    memcpy( &SI_eeprom_write_block[1], src, EEPROM_BLOCK_SIZE );
    joybus_exec( SI_eeprom_write_block, output );
}

/**
 * @brief Read a buffer of bytes from EEPROM
 *
 * This is a convenience helper that abstracts away the 8-byte block access pattern.
 *
 * @param[out] buf
 *             Buffer to read data into
 * @param[in]  start
 *             Byte offset to start reading data from
 * @param[in]  len
 *             Byte length of data to read into buffer
 */
void eeprom_read_bytes( uint8_t * dest, size_t start, size_t len )
{
	uint8_t buf[EEPROM_BLOCK_SIZE];
	size_t bytes_left = len;
	size_t current_block = start / EEPROM_BLOCK_SIZE;
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
 * @brief Write a buffer of bytes to EEPROM
 *
 * This is a convenience helper that abstracts away the 8-byte block access pattern.
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
 *            Buffer of data to write
 * @param[in] start
 *            Byte offset to start writing data to
 * @param[in] len
 *            Byte length of the src buffer
 */
void eeprom_write_bytes( const uint8_t * src, size_t start, size_t len )
{
	uint8_t buf[EEPROM_BLOCK_SIZE];
	size_t bytes_left = len;
	size_t current_block = start / EEPROM_BLOCK_SIZE;
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

/** @} */ /* joybus */
