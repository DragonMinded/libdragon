/**
 * @file joybus_rtc.c
 * @brief Joybus Real-Time Clock Utilities
 * @ingroup rtc
 */

#include "debug.h"
#include "joybus_commands.h"
#include "joybus_internal.h"
#include "joybus_rtc.h"
#include "libcart/cart.h"
#include "n64sys.h"
#include "rtc_utils.h"

/**
 * @addtogroup joybus_rtc
 * @{
 */

// MARK: Types

/** @brief Joybus RTC Block Types */
typedef enum
{
    /** @brief Control block */
    JOYBUS_RTC_BLOCK_CONTROL = 0,
    /** @brief Unused block */
    JOYBUS_RTC_BLOCK_UNUSED = 1,
    /** @brief Time block */
    JOYBUS_RTC_BLOCK_TIME = 2,
} joybus_rtc_block_t;

/** @brief Joybus RTC Status Byte */
typedef union
{
    /** @brief raw byte value */
    uint8_t byte;
    /// @cond
    struct __attribute__((packed))
    {
    /// @endcond
        /** @brief "RTC is stopped" bit */
        unsigned stopped : 1;
        unsigned         : 7;
    /// @cond
    };
    /// @endcond
} joybus_rtc_status_t;

/** @brief Joybus RTC Data Chunk */
typedef union
{
    /** @brief double-word access of data */
    uint64_t dword;
    /** @brief byte-wise access of data */
    uint8_t bytes[8];
} joybus_rtc_data_t;

/** @brief Joybus RTC Control Data */
typedef union
{
    /** @brief Raw data chunk */
    joybus_rtc_data_t data;
    /// @cond
    struct __attribute__((packed))
    {
    /// @endcond
        unsigned             : 6;
        /** @brief Lock Block 1 bit */
        unsigned lock_block1 : 1;
        /** @brief Lock Block 2 bit */
        unsigned lock_block2 : 1;
        unsigned             : 5;
        /** @brief Stop bit */
        unsigned stop        : 1;
        unsigned             : 2;
    /// @cond
    };
    /// @endcond
} joybus_rtc_control_t;

// MARK: Constants

/** @brief Joybus RTC port number */
#define JOYBUS_RTC_PORT 4

/**
 * @brief Duration (in milliseconds) to wait after writing a Joybus RTC block.
 *
 * The software should wait for the previous RTC write to finish before issuing
 * another Joybus RTC command. Ideally, you could read the RTC status byte to
 * determine when to proceed, but some RTC reproductions do not correctly
 * implement the RTC status response, so a delay is used for compatibility.
 */
#define JOYBUS_RTC_WRITE_BLOCK_DELAY 20

// MARK: Static functions

/**
 * @brief Read a Joybus RTC block.
 *
 * @param block the RTC block to read
 * @param[out] data destination buffer
 *
 * @return Joybus RTC status byte
 */
static joybus_rtc_status_t joybus_rtc_read( joybus_rtc_block_t block, joybus_rtc_data_t * data )
{
    joybus_cmd_rtc_read_block_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_RTC_READ_BLOCK,
        .block = block,
    } };
    joybus_exec_cmd_struct(JOYBUS_RTC_PORT, cmd);
    data->dword = cmd.recv.dword;
    return (joybus_rtc_status_t) { .byte = cmd.recv.status };
}

/**
 * @brief Write a Joybus RTC block.
 *
 * @param block the RTC block to write
 * @param[out] data source buffer
 *
 * @return Joybus RTC status byte
 */
static joybus_rtc_status_t joybus_rtc_write( joybus_rtc_block_t block, const joybus_rtc_data_t * data )
{
    joybus_cmd_rtc_write_block_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_RTC_WRITE_BLOCK,
        .block = block,
        .dword = data->dword
    } };
    joybus_exec_cmd_struct(JOYBUS_RTC_PORT, cmd);
    return (joybus_rtc_status_t) { .byte = cmd.recv.status };
}

// MARK: Public functions

bool joybus_rtc_detect( void )
{
    static int detected = -1;
    if (detected > -1) return detected;

    joybus_cmd_identify_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_RTC_IDENTIFY,
    } };
    joybus_exec_cmd_struct(JOYBUS_RTC_PORT, cmd);

    detected = cmd.recv.identifier == JOYBUS_IDENTIFIER_CART_RTC;
    return detected;
}

void joybus_rtc_set_stopped( bool stop )
{
    joybus_rtc_control_t control;
    joybus_rtc_read( JOYBUS_RTC_BLOCK_CONTROL, &control.data );

    if ( control.stop != stop )
    {
        control.stop = stop;
        control.lock_block1 = !stop;
        control.lock_block2 = !stop;
        joybus_rtc_write( JOYBUS_RTC_BLOCK_CONTROL, &control.data );
        wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );
    }
}

bool joybus_rtc_is_stopped( void )
{
    joybus_cmd_identify_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_RTC_IDENTIFY,
    } };
    joybus_exec_cmd_struct(JOYBUS_RTC_PORT, cmd);
    joybus_rtc_status_t status = { .byte = cmd.recv.status };
    return status.stopped;
}

time_t joybus_rtc_read_time( void )
{
    if (!joybus_rtc_detect()) return false;

    joybus_rtc_data_t data = {0};
    joybus_rtc_read( JOYBUS_RTC_BLOCK_TIME, &data );
    if (data.dword == 0) return 0;

    /* Decode the RTC date/time into a struct tm */
    struct tm rtc_time = (struct tm){
        .tm_sec   = bcd_decode( data.bytes[0] ),
        .tm_min   = bcd_decode( data.bytes[1] ),
        .tm_hour  = bcd_decode( data.bytes[2] - 0x80 ),
        .tm_mday  = bcd_decode( data.bytes[3] ),
        .tm_wday  = bcd_decode( data.bytes[4] ),
        .tm_mon   = bcd_decode( data.bytes[5] ) - 1,
        .tm_year  = bcd_decode( data.bytes[6] ) + (bcd_decode( data.bytes[7] ) * 100),
    };
    return mktime( &rtc_time );
}

bool joybus_rtc_set_time( time_t new_time )
{
    if (!joybus_rtc_detect())
    {
        return false;
    }

    if (cart_type == CART_ED)
    {
        // Joybus RTC write is not supported on EverDrive 64 V3.
        // TODO: Research and implement ED64 V3-specific RTC write
        return false;
    }

    if (cart_type == CART_EDX)
    {
        // Joybus RTC write is not supported on EverDrive 64 X7.
        // TODO: Research and implement ED64 X7-specific RTC write
        return false;
    }

    joybus_rtc_control_t control;
    joybus_rtc_read( JOYBUS_RTC_BLOCK_CONTROL, &control.data );

    /* Prepare the RTC to write the time */
    control.stop = true;
    control.lock_block1 = false;
    control.lock_block2 = false;
    joybus_rtc_write( JOYBUS_RTC_BLOCK_CONTROL, &control.data );
    wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );

    /* Determine if the RTC implementation supports writes. */
    if( !joybus_rtc_is_stopped() ) return false;

    /* Write the time block to RTC */
    struct tm * rtc_time = gmtime( &new_time );
    joybus_rtc_data_t data = { .bytes = {
        bcd_encode( rtc_time->tm_sec ),
        bcd_encode( rtc_time->tm_min ),
        bcd_encode( rtc_time->tm_hour ) + 0x80,
        bcd_encode( rtc_time->tm_mday ),
        bcd_encode( rtc_time->tm_wday ),
        bcd_encode( rtc_time->tm_mon + 1 ),
        bcd_encode( rtc_time->tm_year ),
        bcd_encode( rtc_time->tm_year / 100 ),
    } };
    joybus_rtc_write( JOYBUS_RTC_BLOCK_TIME, &data );
    wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );

    /* Put the RTC back into normal operating mode */
    control.stop = false;
    control.lock_block1 = true;
    control.lock_block2 = true;
    joybus_rtc_write( JOYBUS_RTC_BLOCK_CONTROL, &control.data );

    return true;
}

/** @} */ /* joybus_rtc */
