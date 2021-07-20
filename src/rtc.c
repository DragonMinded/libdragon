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
 * The Joybus real-time clock is an peripheral that uses a battery to power
 * a clock that runs even when the N64 is powered-off and maintains the date,
 * time, and day of the week. The Joybus RTC is accessed through the serial
 * interface (SI) similar to EEPROM and controllers. The Joybus RTC was only
 * ever included on one official cartridge that was only available in Japan. 
 *
 * To check if the real-time clock is available, call #joybus_rtc_init.
 * To read the current time from the RTC, call #joybus_rtc_get.
 *
 * In general, since only one game ever used Joybus RTC (and that game was
 * later re-released on the GameCube in English), RTC support by emulators
 * and flash carts tends to be incomplete, inaccurate, or non-existent.
 * Many emulators do not actually implement the RTC write command and will
 * always respond with the host system's current local time. Some emulators
 * and flash carts support writing to RTC but will not persist the date/time
 * after resetting or powering-off.
 * 
 * Some notable examples of RTC support (as of July 2021):
 * 64drive hw2 fully implements RTC including writes, but requires hard-coded
 * delays after setting the time (see #JOYBUS_RTC_WRITE_FINISHED_DELAY).
 * EverDrive64 3.0 and X7 partially support the RTC, with caveats: The RTC
 * must be explicitly enabled in the OS or with a ROM header configuration;
 * RTC will not be detected if the EEPROM save type is used; RTC writes are
 * not supported through the SI, so changing the time must be done in the OS.
 * UltraPIF fully implements an emulated RTC that can be accessed even when
 * the cartridge does not contain a Real-Time Clock or battery.
 *
 * The only reliable way to check if writes are actually supported is to write
 * a time to the RTC and read the time back out. Many emulators that do
 * support RTC reads will silently ignore RTC writes. You should detect
 * whether writes are supported using #joSybus_rtc_is_writable so that you can
 * conditionally show the option to change the time if it's supported. If the
 * RTC supports writes, you can call #joybus_rtc_set to set the date and time.
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
 * @brief Real-time clock identifier.
 *
 * A magic value in the RTC status response that indicates the RTC is present.
 */
#define JOYBUS_RTC_IDENTIFIER 0x1000

/**
 * @brief Duration (in milliseconds) to wait after writing an RTC block.
 *
 * The software must wait for the previous RTC write to finish before issuing
 * another RTC write command. Since there is no status returned by the RTC to
 * indicate that a write has completed, the software must wait a fixed duration.
 */
#define JOYBUS_RTC_WRITE_BLOCK_DELAY 20
/**
 * @brief Duration (in milliseconds) to wait after setting the RTC time.
 *
 * 64drive's RTC updates the RTC readout several times per second, so it is
 * possible to write it and then read back the previous time before it ticks
 * and updates the "shadow interface" that it presents to the SI controller.
 * 
 * Without this delay, the RTC "is writable" test may fail intermittently.
 */
#define JOYBUS_RTC_WRITE_FINISHED_DELAY 500

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
 * RTC block 1 has an unknown purpose and is not used.
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
 * @brief Read the status of the real-time clock.
 *
 * The RTC status response contains a byte-swapped identifier half-word that
 * indicates an RTC is present on the cartridge and a status byte to indicate
 * whether the RTC is in the "busy" state.
 *
 * Generally, you should not need to call this function directly. Prefer
 * calling the high-level #joybus_rtc_init and #joybus_rtc_is_busy functions
 * which handle the magic numbers associated with the response.
 *
 * @return the RTC status response data
 */
static uint32_t joybus_rtc_status( void )
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

    return (
        /* Intentional swap of identifier bytes */
        ((uint32_t)recv_bytes[1] << 16) |
        ((uint32_t)recv_bytes[0] << 8)  |
        recv_bytes[2]
    );
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
 * @return the status of the real-time clock
 */
static uint8_t joybus_rtc_read( uint8_t block, uint64_t * data )
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
 * @return the status of the real-time clock
 */
static uint8_t joybus_rtc_write( uint8_t block, const uint64_t * data )
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
 * The RTC will be busy when the control data is in
 * #JOYBUS_RTC_CONTROL_MODE_SET and is ready to be written to.
 *
 * @return whether the Joybus RTC status is busy
 */
static bool joybus_rtc_is_busy( void )
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
 * zeroes for this data, which is also fine. In the interest of compatibility
 * and accuracy, this data should be preserved, but its actual purpose is
 * unknown.
 *
 * Generally, you should not need to call this function directly. Prefer
 * calling the high-level #joybus_rtc_init and #joybus_rtc_set functions which
 * handle the delays and calibration propagation.
 *
 * @param[out]  control
 *              Destination pointer for the RTC control data.
 *
 * @param[out]  calibration
 *              Destination pointer for the RTC calibration data.
 */
static void joybus_rtc_read_control( uint16_t * control, uint32_t * calibration )
{
    uint64_t data;
    joybus_rtc_read( 0, &data );

    if( control != NULL ) *control = data >> 48;
    if( calibration != NULL ) *calibration = data;
}

/**
 * @brief Write the control block to the real-time clock.
 *
 * Typically this would be set to one of two "modes":
 * Use #JOYBUS_RTC_CONTROL_MODE_SET before writing to block 2 to set the time.
 * Use #JOYBUS_RTC_CONTROL_MODE_RUN after writing to block 2 to resume the RTC.
 *
 * On real hardware it may take some time after the SI command responds for
 * the write operation to actually complete. It is recommended to add a delay
 * using #JOYBUS_RTC_WRITE_BLOCK_DELAY after setting the RTC control block before
 * calling #joybus_rtc_get or #joybus_rtc_write_time to ensure the
 * control block data was fully written.
 *
 * It is strongly recommended to propagate the calibration data returned from
 * #joybus_rtc_read_control into the calibration parameter of this function.
 * If you have any reason to modify or omit the calibration data, please
 * update this documentation with an explanation of why and how you would
 * want to do this.
 *
 * Generally, you should not need to call this function directly. Prefer
 * calling the high-level #joybus_rtc_init and #joybus_rtc_set functions which
 * handle the delays and calibration propagation.
 *
 * @param[in]  control
 *             The control block data
 *
 * @param[in]  calibration
 *             The opaque calibration data from #joybus_rtc_read_control
 */
static void joybus_rtc_write_control( uint16_t control, uint32_t calibration )
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
 * using #JOYBUS_RTC_WRITE_BLOCK_DELAY before setting the RTC control block
 * back to #JOYBUS_RTC_CONTROL_MODE_RUN after setting the time. On 64drive
 * it is recommended to wait using #JOYBUS_RTC_WRITE_FINISHED_DELAY before
 * reading the time to ensure the internal clock has ticked and copied
 * the new time data into the "SI shadow interface".
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
static void joybus_rtc_write_time( const rtc_time_t * rtc_time )
{
    uint64_t data;
    uint8_t * bytes = (uint8_t *)&data;

    int year = rtc_time->year - 1900;
    bytes[0] = byte_to_bcd(rtc_time->sec);
    bytes[1] = byte_to_bcd(rtc_time->min);
    bytes[2] = byte_to_bcd(rtc_time->hour) + 0x80;
    bytes[3] = byte_to_bcd(rtc_time->day);
    bytes[4] = byte_to_bcd(rtc_time->week_day);
    bytes[5] = byte_to_bcd(rtc_time->month + 1);
    bytes[6] = byte_to_bcd(year);
    bytes[7] = byte_to_bcd(year / 100);

    joybus_rtc_write( 2, &data );
}

/**
 * @brief High-level convenience helper to initialize the RTC subsystem.
 *
 * Some flash carts require the RTC to be explicitly enabled.
 * Some emulators and flash carts do not support RTC at all.
 * 
 * This function will detect if the RTC is available and if so, will
 * prepare the RTC so that the current time can be read from it.
 * 
 * This operation may take up to 50 milliseconds to complete.
 * 
 * @return whether the RTC was detected on the cartridge
 */
bool joybus_rtc_init( void )
{
    if( (joybus_rtc_status() >> 8) != JOYBUS_RTC_IDENTIFIER ) return false;

    /* Read the calibration data from the control block */
    uint32_t calibration;
    joybus_rtc_read_control( NULL, &calibration );

    /* Put the RTC into normal operating mode */
    joybus_rtc_write_control( JOYBUS_RTC_CONTROL_MODE_RUN, calibration );
    wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );

    return true;
}

/**
 * @brief Read the current date/time from the real-time clock.
 *
 * Your code should call this once per frame to update the rtc_time_t
 * data structure.
 *
 * The result of calling this function when the RTC is not present is
 * undefined and may not be safe. Make sure you call #joybus_rtc_init
 * at the start of your program to ensure the RTC is detected before
 * reading the current time!
 *
 * @param[out]  rtc_time
 *              Destination pointer for the RTC time data structure
 */
void joybus_rtc_get( rtc_time_t * rtc_time )
{
    uint64_t data;
    joybus_rtc_read( 2, &data );

    uint8_t * bytes = (uint8_t *)&data;

    rtc_time->sec = bcd_to_byte(bytes[0]);
    rtc_time->min = bcd_to_byte(bytes[1]);
    rtc_time->hour = bcd_to_byte(bytes[2] - 0x80);
    rtc_time->day = bcd_to_byte(bytes[3]);
    rtc_time->week_day = bcd_to_byte(bytes[4]);
    rtc_time->month = bcd_to_byte(bytes[5]) - 1;
    rtc_time->year = bcd_to_byte(bytes[6]);
    rtc_time->year += (uint16_t)bcd_to_byte(bytes[7]) * 100;
    rtc_time->year += 1900;
}

/**
 * @brief High-level convenience helper to set the RTC date/time.
 *
 * Prepares the RTC for writing, sets the new time, and resumes the clock.
 *
 * This process will take approximately 570 milliseconds to complete.
 * Unfortunately, the best way to ensure that writes to the RTC have
 * actually finished is by waiting for a fixed duration. Emulators may not
 * accurately reflect this, but this delay is necessary on real hardware.
 *
 * @param[in]   rtc_time
 *              Source pointer for the RTC time data structure
 * 
 * @return false if the RTC does not support entering "set mode"
 */
bool joybus_rtc_set( const rtc_time_t * write_time )
{
    uint32_t calibration;
    /* Read the calibration data from the control block */
    joybus_rtc_read_control( NULL, &calibration );
    /* Prepare the RTC to write the time, preserving the calibration data */
    joybus_rtc_write_control( JOYBUS_RTC_CONTROL_MODE_SET, calibration );
    wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );
    /* Check the RTC status to make sure RTC block 2 writes are supported */
    if( !joybus_rtc_is_busy() ) return false;
    /* Write the updated time to RTC block 2 */
    joybus_rtc_write_time( write_time );
    wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );
    /* Put the RTC back into normal operating mode */
    joybus_rtc_write_control( JOYBUS_RTC_CONTROL_MODE_RUN, calibration );
    wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );
    /* Wait for the RTC to stop being busy */
    while( joybus_rtc_is_busy() ) { /* Spinloop */ }
    wait_ms( JOYBUS_RTC_WRITE_FINISHED_DELAY );
    return true;
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
 * This operation may take approximately 1 second to complete.
 *
 * @return whether RTC writes appear to be supported on the cartridge.
 */
bool joybus_rtc_is_writable( void )
{
    rtc_time_t restore_time;
    rtc_time_t verify_time;
    /* These values are arbitrary but unlikely to be the current date/time. */
    rtc_time_t write_time;
    write_time.year     = 2003;
    write_time.month    = 10;
    write_time.day      = 30;
    write_time.week_day = 0;
    write_time.hour     = 1;
    write_time.min      = 2;
    write_time.sec      = 3;

    joybus_rtc_get( &restore_time );

    bool written = joybus_rtc_set( &write_time );
    if( !written ) return false;

    joybus_rtc_get( &verify_time );

    bool verified = (
        verify_time.year  == write_time.year  &&
        verify_time.month == write_time.month &&
        verify_time.day   == write_time.day   &&
        verify_time.hour  == write_time.hour  &&
        verify_time.min   == write_time.min
    );

    if( verified ) joybus_rtc_set( &restore_time );

    return verified;
}

/** @} */ /* rtc */
