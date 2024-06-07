/**
 * @file joybus_rtc.c
 * @brief Joybus Real-Time Clock Utilities
 * @ingroup rtc
 */

#include "joybus_commands.h"
#include "joybus_internal.h"
#include "joybus_rtc.h"
#include "n64sys.h"
#include "rtc_utils.h"

/**
 * @addtogroup rtc
 * @{
 */

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
 * Without this delay, the #rtc_is_writable test may fail intermittently on
 * 64drive hw2.
 */
#define JOYBUS_RTC_WRITE_FINISHED_DELAY 500

/**
 * @brief The Joybus RTC is running.
 *
 * It is safe to read the current time from the RTC.
 */
#define JOYBUS_RTC_STATUS_RUNNING 0x00
/**
 * @brief The Joybus RTC is stopped.
 *
 * It is safe to write new time data to the RTC.
 */
#define JOYBUS_RTC_STATUS_STOPPED  0x80

/**
 * @brief Control mode for setting the Joybus RTC date/time.
 *
 * Clears the write-protection bits and sets the stop bit.
 */
#define JOYBUS_RTC_CONTROL_MODE_SET 0x0004
/**
 * @brief Control mode for normal operation of the Joybus RTC.
 *
 * Clears the stop bit sets the write-protection bits.
 *
 * 0x0100 is the block 1 write-protection bit.
 * 0x0200 is the block 2 write-protection bit.
 */
#define JOYBUS_RTC_CONTROL_MODE_RUN 0x0300

// MARK: Public functions

void joybus_rtc_init( void )
{
    /* Read the calibration data from the control block */
    uint16_t control;
    uint32_t calibration;
    joybus_rtc_read_control( &control, &calibration );

    if ( control != JOYBUS_RTC_CONTROL_MODE_RUN )
    {
        /* Put the RTC into normal operating mode */
        joybus_rtc_write_control( JOYBUS_RTC_CONTROL_MODE_RUN, calibration );
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
    return cmd.recv.status == JOYBUS_RTC_STATUS_STOPPED;
}

uint8_t joybus_rtc_read( uint8_t block, uint64_t * data )
{
    joybus_cmd_rtc_read_block_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_RTC_READ_BLOCK,
        .block = block,
    } };
    joybus_exec_cmd_struct(JOYBUS_RTC_PORT, cmd);
    *data = cmd.recv.dword;
    return cmd.recv.status;
}

uint8_t joybus_rtc_write( uint8_t block, const uint64_t * data )
{
    joybus_cmd_rtc_write_block_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_RTC_WRITE_BLOCK,
        .block = block,
        .dword = *data
    } };
    joybus_exec_cmd_struct(JOYBUS_RTC_PORT, cmd);
    return cmd.recv.status;
}

void joybus_rtc_read_control( uint16_t * control, uint32_t * calibration )
{
    uint64_t data;
    joybus_rtc_read( 0, &data );

    if( control != NULL ) *control = data >> 48;
    if( calibration != NULL ) *calibration = data;
}

time_t joybus_rtc_read_time( void )
{
    uint64_t data;
    joybus_rtc_read( 2, &data );
    uint8_t * bytes = (uint8_t *)&data;

    struct tm rtc_time = (struct tm){
        .tm_sec   = bcd_decode( bytes[0] ),
        .tm_min   = bcd_decode( bytes[1] ),
        .tm_hour  = bcd_decode( bytes[2] - 0x80 ),
        .tm_mday  = bcd_decode( bytes[3] ),
        .tm_wday  = bcd_decode( bytes[4] ),
        .tm_mon   = bcd_decode( bytes[5] ) - 1,
        .tm_year  = bcd_decode( bytes[6] ) + (bcd_decode( bytes[7] ) * 100),
    };

    return mktime( &rtc_time );
}

void joybus_rtc_write_control( uint16_t control, uint32_t calibration )
{
    uint64_t data = (uint64_t)control << 48 | calibration;
    joybus_rtc_write( 0, &data );
}

void joybus_rtc_write_time( time_t new_time )
{
    uint64_t data;
    uint8_t * bytes = (uint8_t *)&data;

    struct tm * rtc_time = gmtime( &new_time );
    bytes[0] = bcd_encode( rtc_time->tm_sec );
    bytes[1] = bcd_encode( rtc_time->tm_min );
    bytes[2] = bcd_encode( rtc_time->tm_hour ) + 0x80;
    bytes[3] = bcd_encode( rtc_time->tm_mday );
    bytes[4] = bcd_encode( rtc_time->tm_wday );
    bytes[5] = bcd_encode( rtc_time->tm_mon + 1 );
    bytes[6] = bcd_encode( rtc_time->tm_year );
    bytes[7] = bcd_encode( rtc_time->tm_year / 100 );

    joybus_rtc_write( 2, &data );
}

bool joybus_rtc_set_time( time_t new_time )
{
  uint32_t calibration;
  /* Read the calibration data from the control block */
  joybus_rtc_read_control( NULL, &calibration );
  /* Prepare the RTC to write the time, preserving the calibration data */
  joybus_rtc_write_control( JOYBUS_RTC_CONTROL_MODE_SET, calibration );
  wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );
  /* Check the RTC status to make sure RTC "set mode" is supported */
  if( !joybus_rtc_is_stopped() ) return false;
  /* Write the updated time to RTC block 2 */
  joybus_rtc_write_time( new_time );
  wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );
  /* Put the RTC back into normal operating mode */
  joybus_rtc_write_control( JOYBUS_RTC_CONTROL_MODE_RUN, calibration );
  return true;
}

void joybus_rtc_wait_for_write_finished( void )
{
    wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );
    while( joybus_rtc_is_stopped() ) { /* Spinloop */ }
    wait_ms( JOYBUS_RTC_WRITE_FINISHED_DELAY );
}

/** @} */ /* rtc */
