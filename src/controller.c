/**
 * @file controller.c
 * @brief Controller Subsystem
 * @ingroup controller
 */
#include <string.h>
#include "libdragon.h"
#include "regsinternal.h"

/**
 * @defgroup controller Controller Subsystem
 * @ingroup libdragon
 * @brief Controller and accessory interface.
 *
 * The controller subsystem is in charge of communication with all controllers 
 * and accessories plugged into the N64.  The controller subsystem is responsible
 * for interfacing with the serial interface (SI) registers to provide controller
 * data, mempak and rumblepak interfacing, and EEPROM interfacing.
 *
 * Code wishing to communicate with a controller or an accessory should first call
 * #controller_init.  Once the controller subsystem has been initialized, code can
 * either scan the controller interface for changes or perform direct reads from
 * the controller interface.  Controllers can be enumerated with 
 * #get_controllers_present.  Similarly, accessories can be enumerated with
 * #get_accessories_present and #identify_accesory.
 *
 * To read controllers in a managed fashion, call #controller_scan at the beginning
 * of each frame.  From there, #get_keys_down, #get_keys_up, #get_keys_held and
 * #get_keys_pressed will return the status of all keys relative to the last scan.
 * #get_dpad_direction will return a number signifying the polar direction that the
 * D-Pad is being pressed in.
 *
 * To read controllers in a non-managed fashion, call #controller_read.  This will
 * return a structure consisting of all button states on all controllers currently
 * inserted.
 *
 * To enable or disable rumbling on a controller, use #rumble_start and #rumble_stop.
 * These functions will turn rumble on and off at full speed respectively, so if
 * different rumble effects are desired, consider using the @ref timer for accurate
 * timing.
 *
 * A mempak attached to a controller can be treated in one of two ways: as a raw binary
 * string, or as a formatted mempak with notes.  The former allows storage of any
 * data as long as it fits, in any format convenient to the coder, but destroys any
 * non-homebrew data on the mempak.  The latter is recommended as it is completely
 * compatible with official N64 games, though it allows less data to be stored due to
 * filesystem overhead.  To read and write raw sectors, use #read_mempak_address and
 * #write_mempak_address.  The @ref mempak handles reading and writing from the mempak
 * in a way compatible with official games.
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

/** @brief The current sampled controller data */
static struct controller_data current;
/** @brief The previously sampled controller data */
static struct controller_data last;

/** 
 * @brief Initialize the controller subsystem 
 */
void controller_init()
{
    memset(&current, 0, sizeof(current));
    memset(&last, 0, sizeof(last));
}

/**
 * @brief Wait until the SI is finished with a DMA request
 */
static void __SI_DMA_wait(void)
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
static void __controller_exec_PIF( void *inblock, void *outblock )
{
    volatile uint64_t inblock_temp[8];
    volatile uint64_t outblock_temp[8];

    data_cache_hit_writeback_invalidate(inblock_temp, 64);
    memcpy(UncachedAddr(inblock_temp), inblock, 64);

    /* Be sure another thread doesn't get into a resource fight */
    disable_interrupts();

    __SI_DMA_wait();

    SI_regs->DRAM_addr = inblock_temp; // only cares about 23:0
    SI_regs->PIF_addr_write = PIF_RAM; // is it really ever anything else?

    __SI_DMA_wait();

    data_cache_hit_writeback_invalidate(outblock_temp, 64);

    SI_regs->DRAM_addr = outblock_temp;
    SI_regs->PIF_addr_read = PIF_RAM;

    __SI_DMA_wait();

    /* Now that we've copied, its safe to let other threads go */
    enable_interrupts();

    memcpy(outblock, UncachedAddr(outblock_temp), 64);
}

/**
 * @brief Probe the EEPROM interface
 *
 * Prove the EEPROM to see if it exists on this cartridge.
 *
 * @return Nonzero if EEPROM present, zero if EEPROM not present
 */
int eeprom_present()
{
    static unsigned long long SI_eeprom_status_block[8] =
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
    static unsigned long long output[8];

    __controller_exec_PIF(SI_eeprom_status_block,output);

    /* We are looking for 0x80 in the second byte returned, which
     * signifies that there is an EEPROM present.*/
    if( ((output[1] >> 48) & 0xFF) == 0x80 )
    {
        /* EEPROM found! */
        return 1;
    }
    else
    {
        /* EEPROM not found! */
        return 0;
    }
}

/**
 * @brief Read a block from EEPROM
 * 
 * @param[in]  block
 *             Block to read data from.  The N64 accesses eeprom in 8 byte blocks.
 * @param[out] buf
 *             Buffer to place the eight bytes read from EEPROM.
 */
void eeprom_read(int block, uint8_t * const buf)
{
    static unsigned long long SI_eeprom_read_block[8] =
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
    static unsigned long long output[8];

	SI_eeprom_read_block[0] = 0x0000000002080400 | (block & 255);
    __controller_exec_PIF(SI_eeprom_read_block,output);
    memcpy( buf, &output[1], 8 );
}

/**
 * @brief Write a block to EEPROM
 *
 * @param[in] block
 *            Block to write data to.  The N64 accesses eeprom in 8 byte blocks.
 * @param[in] data
 *            Eight bytes of data to write to block specified
 */
void eeprom_write(int block, const uint8_t * const data)
{
    static unsigned long long SI_eeprom_write_block[8] =
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
    static unsigned long long output[8];

	SI_eeprom_write_block[0] = 0x000000000a010500 | (block & 255);
    memcpy( &SI_eeprom_write_block[1], data, 8 );
    __controller_exec_PIF(SI_eeprom_write_block,output);
}

/**
 * @brief Read the controller button status for all controllers
 *
 * Read the controller button status immediately and return results to data.  If
 * calling this function, one should not also call #controller_scan as this
 * does not update the internal state of controllers.
 *
 * @param[out] output
 *             Structure to place the returned controller button status
 */
void controller_read(struct controller_data * output)
{
    static unsigned long long SI_read_con_block[8] =
    {
        0xff010401ffffffff,
        0xff010401ffffffff,
        0xff010401ffffffff,
        0xff010401ffffffff,
        0xfe00000000000000,
        0,
        0,
        1
    };

    __controller_exec_PIF(SI_read_con_block,output);
}

/**
 * @brief Scan the controllers to determine the current button state
 *
 * Scan the four controller ports and calculate the buttons state.  This
 * must be called before calling #get_keys_down, #get_keys_up, 
 * #get_keys_held, #get_keys_pressed or #get_dpad_direction.
 */
void controller_scan()
{
    /* Remember last */
    memcpy(&last, &current, sizeof(current));

    /* Grab current */
    memset(&current, 0, sizeof(current));
    controller_read(&current);
}

/**
 * @brief Get keys that were pressed since the last inspection
 *
 * Return keys pressed since last detection.  This returns a standard
 * #controller_data struct identical to #controller_read.  However, buttons
 * are only set if they were pressed down since the last #controller_scan.
 *
 * @return A structure representing which buttons were just pressed down
 */
struct controller_data get_keys_down()
{
    struct controller_data ret;

    /* Start with baseline */
    memcpy(&ret, &current, sizeof(current));

    /* Figure out which wasn't pressed last time and is now */
    for(int i = 0; i < 4; i++)
    {
        ret.c[i].data = (current.c[i].data) & ~(last.c[i].data);
    }

    return ret;
}

/**
 * @brief Get keys that were released since the last inspection
 *
 * Return keys released since last detection.  This returns a standard
 * #controller_data struct identical to #controller_read.  However, buttons
 * are only set if they were released since the last #controller_scan.
 *
 * @return A structure representing which buttons were just released
 */
struct controller_data get_keys_up()
{
    struct controller_data ret;

    /* Start with baseline */
    memcpy(&ret, &current, sizeof(current));

    /* Figure out which was pressed last time and isn't now */
    for(int i = 0; i < 4; i++)
    {
        ret.c[i].data = ~(current.c[i].data) & (last.c[i].data);
    }

    return ret;
}

/**
 * @brief Get keys that were held since the last inspection
 *
 * Return keys held since last detection.  This returns a standard
 * #controller_data struct identical to #controller_read.  However, buttons
 * are only set if they were held since the last #controller_scan.
 *
 * @return A structure representing which buttons were held
 */
struct controller_data get_keys_held()
{
    struct controller_data ret;

    /* Start with baseline */
    memcpy(&ret, &current, sizeof(current));

    /* Figure out which was pressed last time and now as well */
    for(int i = 0; i < 4; i++)
    {
        ret.c[i].data = (current.c[i].data) & (last.c[i].data);
    }

    return ret;
}

/**
 * @brief Get keys that are currently pressed, regardless of previous state
 *
 * This function works identically to #controller_read except for it is safe
 * to call when using #controller_scan.
 *
 * @return A structure representing which buttons were pressed
 */
struct controller_data get_keys_pressed()
{
    return current;
}

/**
 * @brief Return the DPAD calculated direction
 *
 * Return the direction of the DPAD specified in controller.  Follows standard
 * polar coordinates, where 0 = 0, pi/4 = 1, pi/2 = 2, etc...  Returns -1 when
 * not pressed.  Must be used in conjunction with #controller_scan
 *
 * @param[in] controller
 *            The controller (0-3) to inspect
 *
 * @return A value 0-7 to represent which direction is held, or -1 when not pressed
 */
int get_dpad_direction( int controller )
{
    /* Diagonals first because it could only be right angles otherwise */
    if( current.c[controller & 0x3].up && current.c[controller & 0x3].left )
    {
        return 3;
    }

    if( current.c[controller & 0x3].up && current.c[controller & 0x3].right )
    {
        return 1;
    }

    if( current.c[controller & 0x3].down && current.c[controller & 0x3].left )
    {
        return 5;
    }

    if( current.c[controller & 0x3].down && current.c[controller & 0x3].right )
    {
        return 7;
    }

    if( current.c[controller & 0x3].right )
    {
        return 0;
    }

    if( current.c[controller & 0x3].up )
    {
        return 2;
    }

    if( current.c[controller & 0x3].left )
    {
        return 4;
    }

    if( current.c[controller & 0x3].down )
    {
        return 6;
    }

    return -1;
}

/**
 * @brief Execute a raw PIF command
 *
 * Send an arbitrary command to a controller and receive arbitrary data back
 *
 * @param[in]  controller
 *             The controller (0-3) to send the command to
 * @param[in]  command
 *             The command byte to send
 * @param[in]  bytesout
 *             The number of parameter bytes the command requires
 * @param[in]  bytesin
 *             The number of result bytes expected
 * @param[in]  out
 *             The parameter bytes to send with the command
 * @param[out] in
 *             The result bytes returned by the operation
 */
void execute_raw_command( int controller, int command, int bytesout, int bytesin, unsigned char *out, unsigned char *in )
{
    unsigned long long SI_debug[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    unsigned long long SI_read_controllers_block[8] = { 0, 0, 0, 0, 0, 0, 0, 1 };
    uint8_t *data = (uint8_t *)SI_read_controllers_block;

    // Room for command itself
    data[controller + 0] = bytesout + 1;
    data[controller + 1] = bytesin;
    data[controller + 2] = command;

    memcpy( &data[controller + 3], out, bytesout );
    memset( &data[controller + 3 + bytesout], 0xFF, bytesin );
    data[controller + 3 + bytesout + bytesin] = 0xFE;

    __controller_exec_PIF(SI_read_controllers_block,SI_debug);

    data = (uint8_t *)SI_debug;
    memcpy( in, &data[controller + 3 + bytesout], bytesin );
}

/**
 * @brief Return a bitmask representing which controllers are present
 *
 * Queries the controller interface and returns a bitmask specifying which
 * controllers are present.  See #CONTROLLER_1_INSERTED, #CONTROLLER_2_INSERTED,
 * #CONTROLLER_3_INSERTED and #CONTROLLER_4_INSERTED.
 *
 * @return A bitmask representing controllers present
 */
int get_controllers_present()
{
    int ret = 0;
    struct controller_data output;
    static unsigned long long SI_read_controllers_block[8] =
    {
        0xff010401ffffffff,
        0xff010401ffffffff,
        0xff010401ffffffff,
        0xff010401ffffffff,
        0xfe00000000000000,
        0,
        0,
        1
    };

    __controller_exec_PIF(SI_read_controllers_block,&output);

    if( output.c[0].err == ERROR_NONE ) { ret |= CONTROLLER_1_INSERTED; }
    if( output.c[1].err == ERROR_NONE ) { ret |= CONTROLLER_2_INSERTED; }
    if( output.c[2].err == ERROR_NONE ) { ret |= CONTROLLER_3_INSERTED; }
    if( output.c[3].err == ERROR_NONE ) { ret |= CONTROLLER_4_INSERTED; }

    return ret;
}

/**
 * @brief Return whether the given accessory is recognized
 *
 * @param[in] data
 *            Data as returned from PIF for a given controller
 *
 * @return Nonzero if valid accessory, zero otherwise
 */
static int __is_valid_accessory( uint32_t data )
{
    if( ((data >> 8) & 0xFFFF) == 0x0001 )
    {
        /* This is a rumble pak, mem pak or transfer pak */
        return 1;
    }
    else if( ((data >> 8) & 0xFFFF) == 0x0100 )
    {
        /* This is a VRU */
        return 1;
    }

    return 0;
}

/**
 * @brief Query the PIF as to the status of accessories
 *
 * @param[out] output
 *             Structure to place the result of the accessory query
 */
static void __get_accessories_present( struct controller_data *output )
{
    static unsigned long long SI_read_status_block[8] =
    {
        0xff010300ffffffff,
        0xff010300ffffffff,
        0xff010300ffffffff,
        0xff010300ffffffff,
        0xfe00000000000000,
        0,
        0,
        1
    };

    __controller_exec_PIF(SI_read_status_block,output);
}

/**
 * @brief Return a bitmask specifying which controllers have recognized accessories
 *
 * Queries the controller interface and returns a bitmask specifying which
 * controllers have recognized accessories present.  See #CONTROLLER_1_INSERTED, 
 * #CONTROLLER_2_INSERTED, #CONTROLLER_3_INSERTED and #CONTROLLER_4_INSERTED.
 *
 * @return A bitmask representing accessories recognized
 */
int get_accessories_present()
{
    struct controller_data output;
    int ret = 0;

    /* Grab the actual accessory data */
    __get_accessories_present( &output );

    /* The third byte means something only if this is a standard controller, the VRU will return on the second byte */
    if( (output.c[0].err == ERROR_NONE) && __is_valid_accessory( output.c[0].data ) ) { ret |= CONTROLLER_1_INSERTED; }
    if( (output.c[1].err == ERROR_NONE) && __is_valid_accessory( output.c[1].data ) ) { ret |= CONTROLLER_2_INSERTED; }
    if( (output.c[2].err == ERROR_NONE) && __is_valid_accessory( output.c[2].data ) ) { ret |= CONTROLLER_3_INSERTED; }
    if( (output.c[3].err == ERROR_NONE) && __is_valid_accessory( output.c[3].data ) ) { ret |= CONTROLLER_4_INSERTED; }

    return ret;
}

/**
 * @brief Calculate the 5 bit CRC on a mempak address
 *
 * This function, given an address intended for a mempak read or write, will
 * calculate the CRC on the address, returning the corrected address | CRC.
 *
 * @param[in] address
 *            The mempak address to calculate CRC over
 *
 * @return The mempak address | CRC
 */
static uint16_t __calc_address_crc( uint16_t address )
{
    /* CRC table */
    uint16_t xor_table[16] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x15, 0x1F, 0x0B, 0x16, 0x19, 0x07, 0x0E, 0x1C, 0x0D, 0x1A, 0x01 };
    uint16_t crc = 0;

    /* Make sure we have a valid address */
    address &= ~0x1F;

    /* Go through each bit in the address, and if set, xor the right value into the output */
    for( int i = 15; i >= 5; i-- )
    {
        /* Is this bit set? */
        if( ((address >> i) & 0x1) )
        {
           crc ^= xor_table[i];
        }
    }

    /* Just in case */
    crc &= 0x1F;

    /* Create a new address with the CRC appended */
    return address | crc;
}

/**
 * @brief Calculate the 8 bit CRC over a 32-byte block of data
 *
 * This function calculates the 8 bit CRC appropriate for checking a 32-byte
 * block of data intended for or retrieved from a mempak.
 *
 * @param[in] data
 *            Pointer to 32 bytes of data to run the CRC over
 *
 * @return The calculated 8 bit CRC over the data
 */
static uint8_t __calc_data_crc( uint8_t *data )
{
    uint8_t ret = 0;

    for( int i = 0; i <= 32; i++ )
    {
        for( int j = 7; j >= 0; j-- )
        {
            int tmp = 0;

            if( ret & 0x80 )
            {
                tmp = 0x85;
            }

            ret <<= 1;

            if( i < 32 )
            {
                if( data[i] & (0x01 << j) )
                {
                    ret |= 0x1;
                }
            }

            ret ^= tmp;
        }
    }

    return ret;
}

/**
 * @brief Read a chunk of data from a mempak
 *
 * Given a controller and an address, read 32 bytes from a mempak and
 * return them in data.
 *
 * @param[in]  controller
 *             Which controller to read the data from (0-3)
 * @param[in]  address
 *             A 32 byte aligned offset to read from on the mempak
 * @param[out] data
 *             Buffer to place 32 bytes of data read from the mempak
 *
 * @retval 0  if reading was successful
 * @retval -1 if the controller was out of range
 * @retval -2 if there was no mempak present in the controller
 * @retval -3 if the mempak returned invalid data
 */
int read_mempak_address( int controller, uint16_t address, uint8_t *data )
{
    uint8_t output[64];
    uint8_t SI_read_mempak_block[64];
    int ret;

    /* Controller must be in range */
    if( controller < 0 || controller > 3 ) { return -1; }

    /* Last byte must be 0x01 to signal to the SI to process data */
    memset( SI_read_mempak_block, 0, 64 );
    SI_read_mempak_block[56] = 0xfe;
    SI_read_mempak_block[63] = 0x01;

    /* Start command at the correct channel to read from the right mempak */
    SI_read_mempak_block[controller]     = 0x03;
    SI_read_mempak_block[controller + 1] = 0x21;
    SI_read_mempak_block[controller + 2] = 0x02;

    /* Calculate CRC on address */
    uint16_t read_address = __calc_address_crc( address );
    SI_read_mempak_block[controller + 3] = (read_address >> 8) & 0xFF;
    SI_read_mempak_block[controller + 4] = read_address & 0xFF;

    /* Leave room for 33 bytes (32 bytes + CRC) to come back */
    memset( &SI_read_mempak_block[controller + 5], 0xFF, 33 );

    __controller_exec_PIF(SI_read_mempak_block,&output);

    /* Copy data correctly out of command */
    memcpy( data, &output[controller + 5], 32 );

    /* Validate CRC */
    uint8_t crc = __calc_data_crc( &output[controller + 5] );

    if( crc == output[controller + 5 + 32] )
    {
        /* Data was read successfully */
        ret = 0;
    }
    else
    {
        if( crc == (output[controller + 5 + 32] ^ 0xFF) )
        {
            /* Pak not present! */
            ret = -2;
        }
        else
        {
            /* Pak returned bad data */
            ret = -3;
        }
    }

    return ret;
}

/**
 * @brief Write a chunk of data to a mempak
 *
 * Given a controller and an address, write 32 bytes to a mempak from data.
 *
 * @param[in]  controller
 *             Which controller to write the data to (0-3)
 * @param[in]  address
 *             A 32 byte aligned offset to write to on the mempak
 * @param[out] data
 *             Buffer to source 32 bytes of data to write to the mempak
 *
 * @retval 0  if writing was successful
 * @retval -1 if the controller was out of range
 * @retval -2 if there was no mempak present in the controller
 * @retval -3 if the mempak returned invalid data
 */
int write_mempak_address( int controller, uint16_t address, uint8_t *data )
{
    uint8_t output[64];
    uint8_t SI_write_mempak_block[64];
    int ret;

    /* Controller must be in range */
    if( controller < 0 || controller > 3 ) { return -1; }

    /* Last byte must be 0x01 to signal to the SI to process data */
    memset( SI_write_mempak_block, 0, 64 );
    SI_write_mempak_block[56] = 0xfe;
    SI_write_mempak_block[63] = 0x01;

    /* Start command at the correct channel to write from the right mempak */
    SI_write_mempak_block[controller]     = 0x23;
    SI_write_mempak_block[controller + 1] = 0x01;
    SI_write_mempak_block[controller + 2] = 0x03;

    /* Calculate CRC on address */
    uint16_t write_address = __calc_address_crc( address );
    SI_write_mempak_block[controller + 3] = (write_address >> 8) & 0xFF;
    SI_write_mempak_block[controller + 4] = write_address & 0xFF;

    /* Place the data to be written */
    memcpy( &SI_write_mempak_block[controller + 5], data, 32 );

    /* Leave room for CRC to come back */
    SI_write_mempak_block[controller + 5 + 32] = 0xFF;

    __controller_exec_PIF(SI_write_mempak_block,&output);

    /* Calculate CRC on output */
    uint8_t crc = __calc_data_crc( &output[controller + 5] );

    if( crc == output[controller + 5 + 32] )
    {
        /* Data was written successfully */
        ret = 0;
    }
    else
    {
        if( crc == (output[controller + 5 + 32] ^ 0xFF) )
        {
            /* Pak not present! */
            ret = -2;
        }
        else
        {
            /* Pak returned bad data */
            ret = -3;
        }
    }

    return ret;
}

/**
 * @brief Identify the accessory connected to a controller
 *
 * Given a controller, identify the particular accessory type inserted.
 *
 * @param[in] controller
 *            The controller (0-3) to identify accessories on
 *
 * @retval #ACCESSORY_RUMBLEPAK The accessory connected is a rumblepak
 * @retval #ACCESSORY_MEMPAK The accessory connected is a mempak
 * @retval #ACCESSORY_VRU The accessory connected is a VRU
 * @retval #ACCESSORY_NONE The accessory was not recognized
 */
int identify_accessory( int controller )
{
    uint8_t data[32];
    struct controller_data output;

    /* Grab the actual accessory data */
    __get_accessories_present( &output );

    if( __is_valid_accessory( output.c[controller].data ) )
    {
        switch( ( output.c[controller].data >> 8 ) & 0xFFFF )
        {
            case 0x0001: /* Mempak/rumblepak/transferpak */
            {
                /* Init string one */
                memset( data, 0xfe, 32 );
                write_mempak_address( controller, 0x8000, data );

                /* Init string two */
                memset( data, 0x80, 32 );
                write_mempak_address( controller, 0x8000, data );

                /* Get register contents */
                if( read_mempak_address( controller, 0x8000, data ) == 0 )
                {
                    /* Should really check all bytes, but this should suffice */
                    if( data[0] == 0x80 )
                    {
                        return ACCESSORY_RUMBLEPAK;
                    }
                    else
                    {
                        return ACCESSORY_MEMPAK;
                    }
                }

                /* For good measure */
                break;
            }
            case 0x0100: /* VRU! */
            {
                return ACCESSORY_VRU;
            }
        }
    }

    /* Couldn't identify */
    return ACCESSORY_NONE;
}

/**
 * @brief Turn rumble on for a particular controller
 *
 * @param[in] controller
 *            The controller (0-3) who's rumblepak should activate
 */
void rumble_start( int controller )
{
    uint8_t data[32];

    /* Unsure of why we have to do this multiple times */
    memset( data, 0x01, 32 );
    write_mempak_address( controller, 0xC000, data );
    write_mempak_address( controller, 0xC000, data );
    write_mempak_address( controller, 0xC000, data );
}

/**
 * @brief Turn rumble off for a particular controller
 *
 * @param[in] controller
 *            The controller (0-3) who's rumblepak should deactivate
 */
void rumble_stop( int controller )
{
    uint8_t data[32];

    /* Unsure of why we have to do this multiple times */
    memset( data, 0x00, 32 );
    write_mempak_address( controller, 0xC000, data );
    write_mempak_address( controller, 0xC000, data );
    write_mempak_address( controller, 0xC000, data );
}

/** @} */ /* controller */
