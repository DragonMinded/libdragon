/**
 * @file rtc.c
 * @brief Real-Time Clock Subsystem
 * @ingroup rtc
 */

#include "libdragon.h"

/**
 * @defgroup rtc Real-Time Clock Subsystem
 * @ingroup libdragon
 * @brief Real-time clock interface.
 *
 * The Joybus real-time clock is a cartridge peripheral that uses a battery
 * to power a clock that tracks the date, time, and day of the week. The
 * real-time clock keeps running even when the N64 is powered-off. The
 * Joybus RTC is accessed through the serial interface (SI) similar to EEPROM
 * and controllers. The Joybus RTC was only ever available on one official
 * cartridge that was only available in Japan: Dōbutsu no Mori (Animal Forest).
 * There is also a real-time clock included in the N64DD hardware, which is
 * not currently supported by the LibDragon RTC subsystem.
 *
 * To check if the real-time clock is available, call #rtc_init.
 * To read the current time from the RTC, call #rtc_get.
 *
 * Unfortunately, since only one game ever used Joybus RTC (and that game was
 * later re-released on the GameCube in English), real-time clock support in 
 * emulators and flash carts may be incomplete, inaccurate, or non-existent.
 * Many emulators do not actually implement the SI RTC write command and will
 * always respond with the host system's current local time. Some emulators
 * and flash carts support writing to RTC but will not persist the date/time
 * after resetting or powering-off. You can run the `rtctest` example ROM on
 * your preferred emulator or flash cart to what RTC support is available.
 * 
 * Some notable examples of RTC support (as of July 2021):
 * 64drive hw2 fully implements Joybus RTC including writes, but requires
 * delays after setting the time (see #JOYBUS_RTC_WRITE_FINISHED_DELAY).
 * EverDrive64 3.0 and X7 partially support Joybus RTC, with caveats: The RTC
 * must be explicitly enabled in the OS or with a ROM header configuration;
 * RTC will not be detected if the EEPROM save type is used; RTC writes are
 * not supported through the SI, so changing the time must be done in the OS.
 * UltraPIF fully implements an emulated Joybus RTC that can be accessed even
 * when the cartridge does not include the real-time clock circuitry.
 *
 * The only reliable way to check if writes are actually supported is to write
 * a time to the RTC and read the time back out. Many emulators that do
 * support RTC reads will silently ignore RTC writes. You should detect
 * whether writes are supported using #rtc_is_writable so that you can
 * conditionally show the option to change the time if it's supported. If the
 * RTC supports writes, you can call #rtc_set to set the date and time.
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
 * @brief Joybus real-time clock identifier.
 *
 * A magic value in the SI RTC status response that indicates the Joybus RTC
 * is present.
 */
#define JOYBUS_RTC_IDENTIFIER 0x1000

/**
 * @brief Duration (in milliseconds) to wait after writing a Joybus RTC block.
 *
 * The software must wait for the previous RTC write to finish before issuing
 * another RTC write command. Ideally, you could read the RTC status byte to
 * determine when to proceed, but some RTC implementations do not correctly
 * implement the RTC status command, so a delay is used for compatibility.
 */
#define JOYBUS_RTC_WRITE_BLOCK_DELAY 20
/**
 * @brief Duration (in milliseconds) to wait after setting Joybus RTC time.
 *
 * 64drive hw2 only updates the RTC readout several times per second, so it is
 * possible to write it and then read back the previous time before it ticks
 * and updates the "shadow interface" that it presents to the SI controller.
 * 
 * Without this delay, the RTC "is writable" test may fail intermittently.
 */
#define JOYBUS_RTC_WRITE_FINISHED_DELAY 500

/**
 * @brief The Joybus RTC is ready for block 2 to be read.
 */
#define JOYBUS_RTC_STATUS_RUNNING 0x00
/**
 * @brief The Joybus RTC is ready for block 2 to be written.
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
 * @brief Lookup table for number of days in each month.
 * 
 * Used by #rtc_normalize_time to clamp day of month.
 * 
 * Does not account for leap years in the month of February.
 */
static const uint8_t DAYS_IN_MONTH[] =
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/**
 * @brief Read the status of the Joybus real-time clock.
 *
 * The RTC status response contains a byte-swapped identifier half-word that
 * indicates an RTC is present on the cartridge and a status byte to indicate
 * whether the RTC is in the "stopped" state.
 *
 * Generally, you should not need to call this function directly. Prefer
 * calling the high-level #rtc_init and #joybus_rtc_is_stopped functions
 * which handle the magic numbers associated with the response.
 *
 * @return the normalized Joybus real-time clock status response data
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
 * @brief Read a block of data from the Joybus real-time clock.
 *
 * @param[in]   block
 *              Which RTC block to read from (0-2)
 *
 * @param[out]  data
 *              Destination pointer for the RTC block data
 *
 * @return the status byte from the Joybus real-time clock
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

    joybus_exec( input, output );

    *data = output[1];

    return output[2] >> 56;
}

/**
 * @brief Write a block of data to the Joybus real-time clock.
 *
 * @param[in]   block
 *              Which RTC block to write to (0-2)
 *
 * @param[out]  data
 *              RTC block data to write
 *
 * @return the status byte from the Joybus real-time clock
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

    joybus_exec( input, output );

    return output[2] >> 56;
}

/**
 * @brief Read the status of the Joybus real-time clock.
 *
 * The RTC should be stopped when the control data is in
 * #JOYBUS_RTC_CONTROL_MODE_SET and is ready to be written to.
 *
 * @return whether the Joybus RTC status is stopped
 */
static bool joybus_rtc_is_stopped( void )
{
    return (joybus_rtc_status() & 0xFF) == JOYBUS_RTC_STATUS_STOPPED;
}

/**
 * @brief Read the control block from the Joybus real-time clock.
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
 * calling the high-level #rtc_init and #rtc_set functions which
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
 * @brief Write the control block to the Joybus real-time clock.
 *
 * Typically this would be set to one of two "modes":
 * Use #JOYBUS_RTC_CONTROL_MODE_SET before writing to block 2 to stop the RTC.
 * Use #JOYBUS_RTC_CONTROL_MODE_RUN after writing to block 2 to resume the RTC.
 *
 * It may take some time after the SI command responds for the write operation
 * to actually complete. It is recommended to add a delay after this function
 * using #JOYBUS_RTC_WRITE_BLOCK_DELAY to ensure the control block data was fully
 * written. After the delay, check the RTC status byte to confirm that it is
 * in the expected state.
 *
 * It is strongly recommended to propagate the calibration data returned from
 * #joybus_rtc_read_control into the calibration parameter of this function.
 * If you have any reason to modify or omit the calibration data, please
 * update this documentation with an explanation of why and how you would
 * want to do this.
 *
 * Generally, you should not need to call this function directly. Prefer
 * calling the high-level #rtc_init and #rtc_set functions which handle
 * the necessary delays, status checks, and calibration propagation.
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
 * @brief Write a new date/time to the Joybus real-time clock.
 * 
 * If writes are not supported by the emulator or flash cart, this function
 * will fail silently. It is recommended to call #rtc_is_writable early
 * in your program to detect whether the RTC actually supports writes.
 *
 * This will also fail silently if block 2 of the RTC is write-protected.
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
 * calling the high-level #rtc_set function which handles the RTC control
 * block writes, delays, and status checks.
 *
 * @param[in]  rtc_time
 *             Source pointer for the RTC time data structure
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
 * Some flash carts require the RTC to be explicitly enabled before loading
 * the ROM file. Some emulators and flash carts do not support RTC at all.
 * 
 * This function will detect if the RTC is available and if so, will
 * prepare the RTC so that the current time can be read from it.
 * 
 * This operation may take up to 50 milliseconds to complete.
 * 
 * @return whether the RTC was detected
 */
bool rtc_init( void )
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
 * @brief Calculate sane values for arbitrary time inputs.
 * 
 * If your time inputs are already sane, nothing should change.
 * This function will clamp date/time values within the expected ranges,
 * including the correct day-of-month based on year/month. It will also
 * recalculate the day-of-week based on the clamped year/month/day.
 * 
 * This is useful to call while the player is adjusting the time after each
 * input to ensure that the date being set always makes sense before they
 * actually confirm and commit the updated date/time.
 * 
 * Internally, RTC cannot represent dates before 1990-01-01, although some
 * RTC implementations (like UltraPIF) only support dates after 2000-01-01.
 * 
 * For highest compatibility, it is not recommended to set the date past
 * 2038-01-19 03:14:07 UTC, which is the UNIX timestamp Epochalypse.
 * 
 * @param[in,out] rtc_time
 *                Pointer to the RTC time data structure 
 */
void rtc_normalize_time( rtc_time_t * rtc_time )
{
    /* Clamp date/time values that have static limits */
    if( rtc_time->year < 1900 ) rtc_time->year = 1900;
    if( rtc_time->month > 11 ) rtc_time->month = 11;
    if( rtc_time->hour > 23 ) rtc_time->hour = 23;
    if( rtc_time->min > 59 ) rtc_time->min = 59;
    if( rtc_time->sec > 59 ) rtc_time->sec = 59;

    /* Clamp the day of the month based on the year and month */
    uint16_t year = rtc_time->year;
    uint8_t month = rtc_time->month;
    uint8_t days_in_month = DAYS_IN_MONTH[month];
    bool leap_year = year % 400 == 0 || (year % 100 != 0 && year % 4 == 0);
    if( month == 1 && leap_year ) days_in_month = 29;
    if( rtc_time->day > days_in_month ) rtc_time->day = days_in_month;

    /* Recalculate the day of the week based on calendar date */
    month = month + 1; /* Adjust month to 1-indexed */
    if( month < 3 )
    {
        month = month + 12;
        year = year - 1;
    }
    rtc_time->week_day = (
        rtc_time->day
        + (2 * month)
        + (6 * (month + 1) / 10)
        + year
        + (year / 4)
        - (year / 100)
        + (year / 400)
        + 1
    ) % 7;
}

/**
 * @brief Read the current date/time from the real-time clock.
 *
 * Your code should call this once per frame to update the #rtc_time_t
 * data structure.
 *
 * The result of calling this function when the RTC is not present is
 * undefined and may not be safe. Make sure you call #rtc_init at the
 * start of your program to ensure the RTC is detected before reading
 * the current time!
 *
 * @param[out]  rtc_time
 *              Destination pointer for the RTC time data structure
 */
void rtc_get( rtc_time_t * rtc_time )
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
bool rtc_set( rtc_time_t * write_time )
{
    uint32_t calibration;
    /* Read the calibration data from the control block */
    joybus_rtc_read_control( NULL, &calibration );
    /* Prepare the RTC to write the time, preserving the calibration data */
    joybus_rtc_write_control( JOYBUS_RTC_CONTROL_MODE_SET, calibration );
    wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );
    /* Check the RTC status to make sure RTC "set mode" is supported */
    if( !joybus_rtc_is_stopped() ) return false;
    /* Ensure write_time is a valid RTC date/time */
    rtc_normalize_time( write_time );
    /* Write the updated time to RTC block 2 */
    joybus_rtc_write_time( write_time );
    wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );
    /* Put the RTC back into normal operating mode */
    joybus_rtc_write_control( JOYBUS_RTC_CONTROL_MODE_RUN, calibration );
    wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );
    /* Wait for the RTC to start running */
    while( joybus_rtc_is_stopped() ) { /* Spinloop */ }
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
 * This operation will take approximately 1 second to complete.
 *
 * @return whether RTC writes appear to be supported
 */
bool rtc_is_writable( void )
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

    rtc_get( &restore_time );

    bool written = rtc_set( &write_time );
    if( !written ) return false;

    rtc_get( &verify_time );

    bool verified = (
        verify_time.year  == write_time.year  &&
        verify_time.month == write_time.month &&
        verify_time.day   == write_time.day   &&
        verify_time.hour  == write_time.hour  &&
        verify_time.min   == write_time.min
    );

    if( verified ) rtc_set( &restore_time );

    return verified;
}

/** @} */ /* rtc */
