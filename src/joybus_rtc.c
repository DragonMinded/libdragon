/**
 * @file joybus_rtc.c
 * @brief Joybus Real-Time Clock Utilities
 * @ingroup rtc
 */

#include "debug.h"
#include "joybus_commands.h"
#include "joybus_internal.h"
#include "joybus_rtc.h"
#include "n64sys.h"
#include "rtc_utils.h"

/**
 * @addtogroup rtc
 * @{
 */

// MARK: Types

typedef enum
{
    JOYBUS_RTC_BLOCK_CONTROL = 0,
    JOYBUS_RTC_BLOCK_UNUSED = 1,
    JOYBUS_RTC_BLOCK_TIME = 2,
} joybus_rtc_block_t;

typedef union
{
    uint8_t byte;
    /// @cond
    struct __attribute__((packed))
    {
    /// @endcond
        unsigned stopped : 1;
        unsigned         : 7;
    /// @cond
    };
    /// @endcond
} joybus_rtc_status_t;

typedef union
{
    uint64_t dword;
    uint8_t bytes[8];
} joybus_rtc_data_t;

typedef union
{
    joybus_rtc_data_t data;
    /// @cond
    struct __attribute__((packed))
    {
    /// @endcond
        unsigned             : 6;
        unsigned lock_block1 : 1;
        unsigned lock_block2 : 1;
        unsigned             : 5;
        unsigned stop        : 1;
        unsigned             : 2;
    /// @cond
    };
    /// @endcond
} joybus_rtc_control_t;

// MARK: Constants

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

/**
 * @brief Duration (in milliseconds) to wait after setting Joybus RTC time.
 *
 * 64drive hw2 only updates the RTC readout a few times per second, so it is
 * possible to write a new time, then read back the previous time before the
 * 64drive clock ticks to update the "shadow interface" that the SI reads from.
 *
 * Without this delay, the #rtc_is_persistent test may fail intermittently on
 * 64drive hw2.
 */
#define JOYBUS_RTC_WRITE_FINISHED_DELAY 500

// MARK: Static functions

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

void joybus_rtc_init( void )
{
    joybus_rtc_control_t control;
    joybus_rtc_read( JOYBUS_RTC_BLOCK_CONTROL, &control.data );

    if ( control.stop )
    {
        /* Put the RTC into normal operating mode */
        control.stop = false;
        control.lock_block1 = true;
        control.lock_block2 = true;
        joybus_rtc_write( JOYBUS_RTC_BLOCK_CONTROL, &control.data );
        wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );
    }
}

bool joybus_rtc_detect( void )
{
    joybus_cmd_identify_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_RTC_IDENTIFY,
    } };
    joybus_exec_cmd_struct(JOYBUS_RTC_PORT, cmd);
    return cmd.recv.identifier == JOYBUS_IDENTIFIER_CART_RTC;
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
    joybus_rtc_data_t data;
    joybus_rtc_read( JOYBUS_RTC_BLOCK_TIME, &data );

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

void joybus_rtc_write_time( time_t new_time )
{
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
}

bool joybus_rtc_set_time( time_t new_time )
{
    joybus_rtc_control_t control;
    joybus_rtc_read( JOYBUS_RTC_BLOCK_CONTROL, &control.data );

    /* Prepare the RTC to write the time */
    control.stop = true;
    control.lock_block1 = false;
    control.lock_block2 = false;
    joybus_rtc_write( JOYBUS_RTC_BLOCK_CONTROL, &control.data );
    wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );

    if( !joybus_rtc_is_stopped() ) return false;

    joybus_rtc_write_time( new_time );
    wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );

    /* Put the RTC back into normal operating mode */
    control.stop = false;
    control.lock_block1 = true;
    control.lock_block2 = true;
    joybus_rtc_write( JOYBUS_RTC_BLOCK_CONTROL, &control.data );

    return true;
}

void joybus_rtc_wait_for_write_finished( void )
{
    while( joybus_rtc_is_stopped() ) { /* Spinloop */ }
    wait_ms( JOYBUS_RTC_WRITE_FINISHED_DELAY );
}

/** @} */ /* rtc */
