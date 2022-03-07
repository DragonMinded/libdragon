/**
 * @file controller.c
 * @brief Controller Subsystem
 * @ingroup controller
 */

#include <string.h>
#include "libdragon.h"
#include "joybusinternal.h"

/**
 * @defgroup controller Controller Subsystem
 * @ingroup libdragon
 * @brief Controller and accessory interface.
 *
 * The controller subsystem is in charge of communication with all controllers 
 * and accessories plugged into the N64 controller ports. The controller subsystem
 * leverages the @ref joybus "Joybus Subsystem" to provide controller data and
 * interface with accessories such as the Controller Pak, Rumble Pak, Transfer Pak,
 * and the Voice-Recognition Unit.
 *
 * Code wishing to communicate with a controller or an accessory should first call
 * #controller_init. The controller subsystem performs an automatic background
 * scanning of all controllers, in an efficient way, saving the current status
 * in a local cache. Alternatively, it is possible to execute direct, blocking
 * controller I/O reads, though they might be quite slow.
 * 
 * To read the controller status from this cache, call #controller_scan once per
 * frame (or whenever you want to perform the reading), and then call #get_keys_down,
 * #get_keys_up, #get_keys_held and #get_keys_pressed, that will return the
 * status of all keys relative to the previous inspection. #get_dpad_direction will
 * return a number signifying the polar direction that the D-Pad is being
 * pressed in.
 *
 * To perform direct reads to the controllers, call #controller_read.  This will
 * return a structure consisting of all button states on all controllers currently
 * inserted. Note that this function takes about 10% of a frame's worth of time.
 *
 * Controllers can be enumerated with 
 * #get_controllers_present.  Similarly, accessories can be enumerated with
 * #get_accessories_present and #identify_accessory.
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

/** @brief The sampled controller data just read by autoscan */
static volatile struct controller_data next;
/** @brief The current sampled controller data accessible via get_keys_* functions */
static struct controller_data current;
/** @brief The previously sampled controller data */
static struct controller_data prev;
/** @brief True if there is a pending controller autoscan */
static volatile bool controller_autoscan_in_progress = false;

static void controller_interrupt_update(uint64_t *output, void *ctx)
{
    memcpy((void*)&next, output, sizeof(struct controller_data));
    controller_autoscan_in_progress = false;
}

static void controller_interrupt(void) 
{
    static const unsigned long long SI_read_con_block[8] =
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
    
    if (!controller_autoscan_in_progress) {    
        controller_autoscan_in_progress = true;
        joybus_exec_async(SI_read_con_block, controller_interrupt_update, NULL);
    }
}

/** 
 * @brief Initialize the controller subsystem.
 * 
 * After initialization, the controllers will be scanned automatically in
 * background one time per frame. You can access the last scanned status
 * using #get_keys_down, #get_keys_up, #get_keys_held #get_keys_pressed,
 * and #get_dpad_direction.
 */
void controller_init( void )
{
    memset(&prev, 0, sizeof(struct controller_data));
    memset(&current, 0, sizeof(struct controller_data));
    memset((void*)&next, 0, sizeof(struct controller_data));
    register_VI_handler(controller_interrupt);
}

/**
 * @brief Read the controller button status for all controllers
 *
 * Read the controller button status immediately and return results to data.
 * 
 * @note This function is slow: it blocks for about 10% of a frame time. To avoid
 *       this hit, use the managed functions (#get_keys_down, etc.).
 *
 * @param[out] output
 *             Structure to place the returned controller button status
 *             
 */
void controller_read( struct controller_data * output )
{
    static const unsigned long long SI_read_con_block[8] =
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

    joybus_exec( SI_read_con_block, output );
}

/**
 * @brief Read the controller button status for all controllers, GC version
 *
 * Read the controller button status immediately and return results to data.
 *
 * @param[out] outdata
 *             Structure to place the returned controller button status
 * @param[in]  rumble
 *             Set to 1 to start rumble, 0 to stop it.
 *
 */
void controller_read_gc( struct controller_data * outdata, const uint8_t rumble[4] )
{
    static const unsigned long long SI_read_con_block[8] =
    {
        0x0308400300ffffff,
        0xffffffffff030840,
        0x0300ffffffffffff,
        0xffff0308400300ff,
        0xffffffffffffff03,
        0x08400300ffffffff,
        0xfffffffffe000000,
        1
    };

    static unsigned long long output[8], input[8];

    memcpy( input, SI_read_con_block, 64 );

    // Fill in the rumbles
    if (rumble[0])
        input[0] |= 1LLU << 24;
    if (rumble[1])
        input[2] |= 1LLU << 48;
    if (rumble[2])
        input[3] |= 1LLU << 8;
    if (rumble[3])
        input[5] |= 1LLU << 32;

    joybus_exec( input, output );

    memcpy( &outdata->gc[0], ((uint8_t *) output) + 5, 8 );
    memcpy( &outdata->gc[1], ((uint8_t *) output) + 5 + 13, 8 );
    memcpy( &outdata->gc[2], ((uint8_t *) output) + 5 + 13 * 2, 8 );
    memcpy( &outdata->gc[3], ((uint8_t *) output) + 5 + 13 * 3, 8 );
}

/**
 * @brief Read the controller origin status for all controllers, GC version
 *
 * This returns the values set on power up, or the values the user requested
 * by reseting the controller by holding X-Y-start. Apps should use these
 * as the center stick values. The meaning of the two deadzone values is unknown.
 *
 * @param[out] outdata
 *             Structure to place the returned controller button status
 */
void controller_read_gc_origin( struct controller_origin_data * outdata )
{
    static const unsigned long long SI_read_con_block[8] =
    {
        0x010a41ffffffffff,
        0xffffffffff010a41,
        0xffffffffffffffff,
        0xffff010a41ffffff,
        0xffffffffffffff01,
        0x0a41ffffffffffff,
        0xfffffffffe000000,
        1
    };

    static unsigned long long output[8];

    joybus_exec( SI_read_con_block, output );

    memcpy( &outdata->gc[0], ((uint8_t *) output) + 3, 10 );
    memcpy( &outdata->gc[1], ((uint8_t *) output) + 3 + 13, 10 );
    memcpy( &outdata->gc[2], ((uint8_t *) output) + 3 + 13 * 2, 10 );
    memcpy( &outdata->gc[3], ((uint8_t *) output) + 3 + 13 * 3, 10 );
}

/**
 * @brief Fetch the current controller state.
 * 
 * This function must be called once per frame, or any time we want to update
 * the state of the controllers. After calling this function, you can use
 * #get_keys_down, #get_keys_up, #get_keys_held, #get_keys_pressed and
 * #get_dpad_direction to inspect the controller state.
 * 
 * This function is very fast. In fact, controllers are read in background
 * asynchronously under interrupt, so this function just synchronizes the
 * internal state.
 */
void controller_scan( void )
{
    prev = current;

    disable_interrupts();
    memcpy(&current, (void*)&next, sizeof(struct controller_data));
    enable_interrupts();
}

/**
 * @brief Get keys that were pressed since the last inspection
 *
 * Return keys pressed since last detection. This returns a standard
 * #SI_controllers_state_t struct identical to #controller_read. However, buttons
 * are only set if they were pressed down since the last read.
 *
 * @return A structure representing which buttons were just pressed down
 */
struct controller_data get_keys_down( void )
{
    struct controller_data ret = current;

    /* Figure out which wasn't pressed last time and is now */
    for(int i = 0; i < 4; i++)
    {
        ret.c[i].data = (current.c[i].data) & ~(prev.c[i].data);
    }

    return ret;
}

/**
 * @brief Get keys that were released since the last inspection
 *
 * Return keys released since last detection. This returns a standard
 * #SI_controllers_state_t struct identical to #controller_read. However, buttons
 * are only set if they were released since the last read.
 *
 * @return A structure representing which buttons were just released
 */
struct controller_data get_keys_up( void )
{
    /* Start with baseline */
    struct controller_data ret = current;

    /* Figure out which was pressed last time and isn't now */
    for(int i = 0; i < 4; i++)
    {
        ret.c[i].data = ~(current.c[i].data) & (prev.c[i].data);
    }

    return ret;
}

/**
 * @brief Get keys that were held since the last inspection
 *
 * Return keys held since last detection. This returns a standard
 * #SI_controllers_state_t struct identical to #controller_read. However, buttons
 * are only set if they were held since the last read.
 *
 * @return A structure representing which buttons were held
 */
struct controller_data get_keys_held( void )
{
    /* Start with baseline */
    struct controller_data ret = current;

    /* Figure out which was pressed last time and now as well */
    for(int i = 0; i < 4; i++)
    {
        ret.c[i].data = (current.c[i].data) & (prev.c[i].data);
    }

    return ret;
}

/**
 * @brief Get keys that are currently pressed, regardless of previous state
 *
 * This function works identically to #controller_read except that it returns
 * the cached data from the last background autoscan.
 *
 * @return A structure representing which buttons were pressed
 */
struct controller_data get_keys_pressed( void )
{
    return current;
}

/**
 * @brief Return the DPAD calculated direction
 *
 * Return the direction of the DPAD specified in controller.  Follows standard
 * polar coordinates, where 0 = 0, pi/4 = 1, pi/2 = 2, etc...  Returns -1 when
 * not pressed.
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

    joybus_exec(SI_read_controllers_block,SI_debug);

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
int get_controllers_present( void )
{
    int ret = 0;
    struct controller_data output;
    static const unsigned long long SI_read_controllers_block[8] =
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

    joybus_exec( SI_read_controllers_block, &output );

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
    static const unsigned long long SI_read_status_block[8] =
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

    joybus_exec( SI_read_status_block, output );
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
int get_accessories_present(struct controller_data *out)
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

    if (out)
        memcpy( out, &output, sizeof(struct controller_data) );

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

    joybus_exec( SI_read_mempak_block, &output );

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

    joybus_exec( SI_write_mempak_block, &output );

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
 * @brief Check if connected accesory is transfer pak by setting power to the device on and off and checking that it responds as expected.
 *
 * @param[in] controller
 *            The controller (0-3) to identify accessories on
 *
 * @return true if accessory behaves like a transfer pak, false otherwise.
 */
static bool __is_transfer_pak( int controller )
{
    uint8_t data[32];
    memset( data, 0x84, 32 );
    write_mempak_address( controller, 0x8000, data );
    read_mempak_address( controller, 0x8000, data );

    bool result = (data[0] == 0x84);

    memset( data, 0xFE, 32 );
    write_mempak_address( controller, 0x8000, data );
    read_mempak_address( controller, 0x8000, data );

    return result & (data[0] == 0x00);
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
 * @retval #ACCESSORY_TRANSFERPAK The accessory connected is a transferpak
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
                        return __is_transfer_pak( controller ) ? ACCESSORY_TRANSFERPAK : ACCESSORY_MEMPAK;
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
