/**
 * @file rtc.c
 * @brief Real-Time Clock Subsystem
 * @ingroup rtc
 */

#include "libdragon.h"

/**
 * @defgroup rtc Real-Time Clock Subsystem
 * @ingroup libdragon
 * @brief Joybus real-time clock interface.
 *
 * The Joybus real-time clock is an accessory that was only included on
 * one official cartridge that was only available in Japan. The Joybus RTC
 * is accessed through the serial interface (SI) similar to EEPROM.
 * A real RTC uses a battery to power a clock that runs even when the N64
 * is powered-off and maintains the date, time, and day of the week.
 *
 * To check if the real-time clock is available, call #joybus_rtc_is_present.
 * If detected, it is a good idea to ensure the RTC control block is in a
 * known-good state by calling the #joybus_rtc_run function before reading
 * the current time with #joybus_rtc_read_time for the first time.
 *
 * The simplest method to change the time is by calling #joybus_rtc_set,
 * which is a high-level convenience function that prepares the RTC and
 * writes the time with the proper delays.
 *
 * In general, since only one game ever used Joybus RTC (and that game was
 * later re-released on the GameCube in English), RTC support by emulators
 * and flash carts tends to be incomplete, inaccurate, or non-existent.
 * Many emulators do not actually implement the RTC write command and will
 * always respond with the host system's current local time. Some emulators
 * and flash carts support writing to RTC but will not persist the date/time
 * after resetting or powering-off.
 *
 * The only way to check if writes are actually supported is to write a
 * time to the RTC and read the time back out. Many emulators that do
 * support RTC reads will silently ignore RTC writes. You should detect
 * whether writes are supported using #joybus_rtc_is_writable so that you can
 * conditionally show the option to change the time if it's supported.
 *
 * The RTC sends and receives the date/time in bytes of binary-coded decimal
 * numbers. This subsystem handles decoding and encoding the date/time into
 * a more generally-useful struct called #rtc_time_t.
 *
 * Special thanks to marshallh and jago85 for their hard work and research
 * reverse-engineering and documenting the inner-workings of the Joybus RTC.
 *
 * @{
 */

/**
 * @brief Real-time clock identifier byte.
 *
 * A magic value in the RTC status response that indicates the RTC is present.
 */
#define JOYBUS_RTC_IDENTIFIER 0x10

/**
 * @brief Duration (in milliseconds) to wait after writing to RTC.
 *
 * The software must wait for the previous RTC write to finish before issuing
 * another RTC write command. Since there is no status returned by the RTC to
 * indicate that a write has completed, the software must wait a fixed duration.
 */
#define JOYBUS_RTC_WRITE_DELAY 20

/**
 * @brief The RTC is ready for block 2 to be read.
 */
#define JOYBUS_RTC_STATUS_READY 0x00
/**
 * @brief The RTC is ready for block 2 to be written.
 */
#define JOYBUS_RTC_STATUS_BUSY  0x80

/**
 * @brief Control mode for setting the RTC date/time.
 *
 * Clears the write-protection bits and sets the busy bit.
 */
#define JOYBUS_RTC_CONTROL_MODE_SET 0x0004
/**
 * @brief Control mode for normal operation of the RTC.
 *
 * Clears the busy bit sets the write-protection bits.
 *
 * 0x0100 is the block 1 write-protection bit.
 * 0x0200 is the block 2 write-protection bit.
 *
 * RTC block 1 has an unknown purpose and is typically unimplemented.
 * RTC block 2 contains the date and time.
 */
#define JOYBUS_RTC_CONTROL_MODE_RUN 0x0300

/**
 * @brief Decode a binary-coded decimal number.
 *
 * @param[in]   bcd
 *              binary-coded decimal number
 *
 * @return the decoded number
 */
static uint8_t bcd_to_byte(uint8_t bcd)
{
    uint8_t hi = (bcd & 0xF0) >> 4;
    uint8_t lo = bcd & 0x0F;
    return (hi * 10) + lo;
}

/**
 * @brief Encode a binary-coded decimal number.
 *
 * @param[in]   byte
 *              number to encode
 *
 * @return the encoded binary-coded decimal number
 */
static uint8_t byte_to_bcd(uint8_t byte)
{
    byte %= 100;
    return ((byte / 10) << 4) | (byte % 10);
}

/**
 * @brief Compare two RTC times for (rough) equality
 *
 * @param[in]   a
 *              Source pointer to an RTC time data structure
 *
 * @param[in]   b
 *              Source pointer to an RTC time data structure
 *
 * @return whether the RTC time data structures are (roughly) equal
 */
static bool joybus_rtc_time_equal( const rtc_time_t * a, const rtc_time_t * b )
{
    return (
        a->year  == b->year  &&
        a->month == b->month &&
        a->day   == b->day   &&
        a->hour  == b->hour  &&
        a->min   == b->min
        /* Skip sec comparison in case the clock ticked between RTC reads */
    );
}

/**
 * @brief Read the status of the real-time clock.
 *
 * The RTC status response contains an identifier byte to indicate that the
 * RTC is present on the cartridge and a status byte to indicate whether the
 * RTC is in the "busy" state.
 *
 * Generally, you should not need to call this function directly. Prefer
 * calling the high-level #joybus_rtc_is_present and #joybus_rtc_is_busy
 * functions which handle the magic numbers associated with the response.
 *
 * @return the RTC status response data
 */
uint16_t joybus_rtc_status( void )
{
    static const uint64_t input[8] =
    {
        0x00000000ff010306,
        0xfffffffe00000000,
        0,
        0,
        0,
        0,
        0,
        1
    };
    static uint64_t output[8];

    controller_exec_pif( input, output );

    uint8_t * recv_bytes = (uint8_t *)&output[1];

    return (uint16_t)recv_bytes[1] << 8 | recv_bytes[2];
}

/**
 * @brief Read a block of data from the real-time clock
 *
 * @param[in]   block
 *              Which RTC block to read from (0-2)
 *
 * @param[out]  data
 *              Destination pointer for the RTC block data
 *
 * @return the busy status of the real-time clock
 */
uint8_t joybus_rtc_read( uint8_t block, uint64_t * data )
{
    assert(block <= 2);

    static uint64_t input[8] =
    {
        0x0000000002090700,
        0xffffffffffffffff,
        0xfffe000000000000,
        0,
        0,
        0,
        0,
        1
    };
    static uint64_t output[8];

    input[0] = (input[0] & 0xFFFFFFFFFFFFFF00) | block;

    controller_exec_pif( input, output );

    *data = output[1];

    return output[2] >> 56;
}

/**
 * @brief Write a block of data to the real-time clock
 *
 * @param[in]   block
 *              Which RTC block to write to (0-2)
 *
 * @param[out]  data
 *              RTC block data to write
 *
 * @return the busy status of the real-time clock
 */
uint8_t joybus_rtc_write( uint8_t block, const uint64_t * data )
{
    assert(block <= 2);

    static uint64_t input[8] =
    {
        0x000000000A010800,
        0xffffffffffffffff,
        0xfffe000000000000,
        0,
        0,
        0,
        0,
        1
    };
    static uint64_t output[8];

    input[0] = (input[0] & 0xFFFFFFFFFFFFFF00) | block;
    input[1] = *data;

    controller_exec_pif( input, output );

    return output[2] >> 56;
}

/**
 * @brief Read the status of the real-time clock.
 *
 * Some flash carts require the RTC to be explicitly enabled.
 * Some emulators and flash carts do not support RTC at all.
 *
 * @return whether the Joybus RTC is present on the cartridge.
 */
bool joybus_rtc_is_present( void )
{
    return (joybus_rtc_status() >> 8) == JOYBUS_RTC_IDENTIFIER;
}

/**
 * @brief Read the status of the real-time clock.
 *
 * The RTC will be busy when the control data is in
 * #JOYBUS_RTC_CONTROL_MODE_SET and is ready to be written to.
 *
 * @return whether the Joybus RTC status is busy
 */
bool joybus_rtc_is_busy( void )
{
    return (joybus_rtc_status() & 0xFF) == JOYBUS_RTC_STATUS_BUSY;
}

/**
 * @brief Read the control block from the real-time clock.
 *
 * The control data should generally be one of the pre-defined modes:
 * #JOYBUS_RTC_CONTROL_MODE_SET when writing to the RTC.
 * #JOYBUS_RTC_CONTROL_MODE_RUN when not writing to the RTC.
 *
 * The calibration data is an opaque value that should be propagated when
 * calling #joybus_rtc_write_control. Emulators and flash carts may return
 * zeroes for this data, which is also fine. Based on reverse-engineering
 * the real hardware, this data is simply described as "magical", but
 * should be preserved in the interest of high-accuracy and compatibility.
 *
 * Generally, you should not need to call this function directly. Prefer
 * calling the high-level #joybus_rtc_run and #joybus_rtc_set functions which
 * handle the delays and calibration propagation. Call #joybus_rtc_status or
 * #joybus_rtc_is_busy to determine which control mode the RTC is in.
 *
 * @param[out]  control
 *              Destination pointer for the RTC control data.
 *
 * @param[out]  calibration
 *              Destination pointer for the RTC calibration data.
 */
void joybus_rtc_read_control( uint16_t * control, uint32_t * calibration )
{
    uint64_t data;
    joybus_rtc_read( 0, &data );

    if( control != NULL ) *control = data >> 48;
    if( calibration != NULL ) *calibration = data;
}

/**
 * @brief Read the current date/time from the real-time clock.
 *
 * Your code should call this once per frame to update the rtc_time_t
 * data structure.
 *
 * Before calling this function for the first time, it is recommended
 * that you call #joybus_rtc_run to ensure the RTC is ready and running.
 *
 * The result of calling this function when the RTC is not present is
 * undefined and may not be safe. Make sure you call #joybus_rtc_status
 * at the start of your program to ensure the RTC is detected before
 * reading the current time!
 *
 * @param[out]  rtc_time
 *              Destination pointer for the RTC time data structure
 */
void joybus_rtc_read_time( rtc_time_t * rtc_time )
{
    uint8_t data[sizeof(uint64_t)];
    joybus_rtc_read( 2, (uint64_t *)data );

    rtc_time->sec = bcd_to_byte(data[0]);
    rtc_time->min = bcd_to_byte(data[1]);
    rtc_time->hour = bcd_to_byte(data[2] - 0x80);
    rtc_time->day = bcd_to_byte(data[3]);
    rtc_time->week_day = bcd_to_byte(data[4]);
    rtc_time->month = bcd_to_byte(data[5]) - 1;
    rtc_time->year = bcd_to_byte(data[6]);
    rtc_time->year += (uint16_t)bcd_to_byte(data[7]) * 100;
    rtc_time->year += 1900;
}

/**
 * @brief Write the control block to the real-time clock.
 *
 * Typically this would be set to one of two "modes":
 * Use #JOYBUS_RTC_CONTROL_MODE_SET before writing to block 2 to set the time.
 * Use #JOYBUS_RTC_CONTROL_MODE_RUN after writing to block 2 to resume the RTC.
 *
 * It may take up to 20 milliseconds after this function returns for the
 * write to actually complete on real hardware. It is recommended to wait
 * using #JOYBUS_RTC_WRITE_DELAY after setting the RTC control block before
 * calling #joybus_rtc_read_time or #joybus_rtc_write_time to ensure the
 * control block data was fully written.
 *
 * It is strongly recommended to propagate the calibration data returned from
 * #joybus_rtc_read_control into the calibration parameter of this function.
 * If you have any reason to modify or omit the calibration data, please
 * update this documentation with an explanation of why and how you would
 * want to do this.
 *
 * Generally, you should not need to call this function directly. Prefer
 * calling the high-level #joybus_rtc_run and #joybus_rtc_set functions which
 * handle the delays and calibration propagation.
 *
 * @param[in]  control
 *             The control block data
 *
 * @param[in]  calibration
 *             The opaque calibration data from #joybus_rtc_read_control
 */
void joybus_rtc_write_control( uint16_t control, uint32_t calibration )
{
    uint64_t data = (uint64_t)control << 48 | calibration;
    joybus_rtc_write( 0, &data );
}

/**
 * @brief Write a new date/time to the real-time clock.
 *
 * This will fail silently if block 2 of the RTC is write-protected.
 *
 * The RTC control block should be set to #JOYBUS_RTC_CONTROL_MODE_SET
 * using #joybus_rtc_write_control before calling this function.
 *
 * The RTC control block should be set to #JOYBUS_RTC_CONTROL_MODE_RUN
 * using #joybus_rtc_write_control after calling this function.
 *
 * It may take up to 20 milliseconds after this function returns for the
 * write to actually complete on real hardware. It is recommended to wait
 * using #JOYBUS_RTC_WRITE_DELAY before setting the RTC control block back
 * to #JOYBUS_RTC_CONTROL_MODE_RUN after setting the time.
 *
 * Generally, you should not need to call this function directly. Prefer
 * calling the high-level #joybus_rtc_set function which handles the RTC
 * control block writes, delays, and detects ignored writes.
 *
 * @param[in]  rtc_time
 *             Source pointer for the RTC time data structure
 *
 * @return the status of the real-time clock
 */
void joybus_rtc_write_time( const rtc_time_t * rtc_time )
{
    uint8_t data[sizeof(uint64_t)];

    int year = rtc_time->year - 1900;
    data[0] = byte_to_bcd(rtc_time->sec);
    data[1] = byte_to_bcd(rtc_time->min);
    data[2] = byte_to_bcd(rtc_time->hour) + 0x80;
    data[3] = byte_to_bcd(rtc_time->day);
    data[4] = byte_to_bcd(rtc_time->week_day);
    data[5] = byte_to_bcd(rtc_time->month + 1);
    data[6] = byte_to_bcd(year);
    data[7] = byte_to_bcd(year / 100);

    joybus_rtc_write( 2, (uint64_t *)data );
}

/**
 * @brief High-level convenience helper to prepare the RTC after startup.
 *
 * This function is a good idea to call immediately after verifying
 * that the RTC is present and before the first call to
 * #joybus_rtc_read_time so that you can be sure the RTC is ready.
 *
 * This function will take approximately 25 milliseconds to complete.
 *
 * It is not necessary to call this after calling #joybus_rtc_set.
 */
void joybus_rtc_run( void )
{
    uint32_t calibration;
    /* Read the calibration data from the control block */
    joybus_rtc_read_control( NULL, &calibration );
    /* Put the RTC into normal operating mode */
    joybus_rtc_write_control( JOYBUS_RTC_CONTROL_MODE_RUN, calibration );
    wait_ms( JOYBUS_RTC_WRITE_DELAY );
}

/**
 * @brief High-level convenience helper to set the RTC date/time.
 *
 * Prepares the RTC for writing, sets the new time, and resumes the clock.
 *
 * This process will take approximately 70 milliseconds to complete.
 * Unfortunately, the best way to ensure that writes to the RTC have
 * actually finished is by waiting for a fixed duration. Emulators may not
 * accurately reflect this, but this delay is necessary on real hardware.
 *
 * @param[in]   rtc_time
 *              Source pointer for the RTC time data structure
 */
void joybus_rtc_set( const rtc_time_t * write_time )
{
    uint32_t calibration;
    /* Read the calibration data from the control block */
    joybus_rtc_read_control( NULL, &calibration );
    /* Prepare the RTC to write the time, preserving the calibration data */
    joybus_rtc_write_control( JOYBUS_RTC_CONTROL_MODE_SET, calibration );
    wait_ms( JOYBUS_RTC_WRITE_DELAY );
    /* Check the RTC status to make sure RTC block 2 writes are supported */
    if( joybus_rtc_is_busy() )
    {
        /* Write the updated time to RTC block 2 */
        joybus_rtc_write_time( write_time );
        wait_ms( JOYBUS_RTC_WRITE_DELAY );
        /* Put the RTC back into normal operating mode */
        joybus_rtc_write_control( JOYBUS_RTC_CONTROL_MODE_RUN, calibration );
        wait_ms( JOYBUS_RTC_WRITE_DELAY );
    }
}

/**
 * @brief Determine whether the RTC supports writing the time.
 *
 * Some emulators and flash carts do not support writing to the RTC, so
 * this function makes an attempt to detect silent write failures and will
 * return `false` if it is unable to change the time on the RTC.
 *
 * This function is useful if your program wants to conditionally offer the
 * ability to set the time based on hardware/emulator support.
 *
 * Unfortunately this operation may introduce a slight drift in the clock,
 * but it is the only way to determine if the RTC supports the write command.
 *
 * This function may take up to 150 milliseconds to complete.
 *
 * @return whether RTC writes appear to be supported on the cartridge.
 */
bool joybus_rtc_is_writable( void )
{
    rtc_time_t restore_time;
    rtc_time_t verify_time;
    /* These values are arbitrary but unlikely to be the current date/time. */
    rtc_time_t test_time;
    test_time.year     = 2003;
    test_time.month    = 10;
    test_time.day      = 30;
    test_time.week_day = 0;
    test_time.hour     = 1;
    test_time.min      = 2;
    test_time.sec      = 3;

    joybus_rtc_read_time( &restore_time );
    joybus_rtc_set( &test_time );
    joybus_rtc_read_time( &verify_time );
    joybus_rtc_set( &restore_time );
    return joybus_rtc_time_equal( &test_time, &verify_time );
}

/** @} */ /* rtc */
