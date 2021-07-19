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
 * one official cartridge that was only available in Japan. The RTC
 * interfaces through the serial interface (SI) similar to EEPROM.
 * The RTC uses a battery to power a clock that runs even when the N64
 * is powered-off and maintains the date, time, and day of the week.
 * 
 * In general, since only one game ever used RTC (and that game was later
 * re-released on the GameCube in English), the RTC is not well-supported.
 * Several emulators and flash carts support the RTC with varying
 * degrees of completeness and accuracy. Most emulators do not actually
 * implement the RTC write command, and will always respond with the
 * host system's current local time. Some flash carts will support
 * writing to RTC but will not persist the date/time after powering-off.
 * 
 * The only way to check if writes are actually supported is to write a
 * time to the RTC and read the time back out. Many emulators that do
 * support RTC reads will silently ignore RTC writes.
 * 
 * The RTC sends and receives the date/time in bytes of binary-coded decimal
 * numbers. This subsystem handles decoding and encoding the date/time into
 * a more generally-useful struct called #rtc_time_t. 
 * 
 * To check if the real-time clock is available, call #joybus_rtc_present.
 * If present, it is a good idea to ensure the RTC control block is in a
 * known-good state by calling #joybus_rtc_write_control function with 
 * #JOYBUS_RTC_CONTROL_MODE_RUN and waiting for #JOYBUS_RTC_WRITE_DELAY
 * milliseconds before reading the current time with #joybus_rtc_read_time.
 * 
 * The simplest method to change the time is by calling #joybus_rtc_set,
 * which is a high-level convenience function that prepares the RTC and
 * writes the time with the proper delays.
 * 
 * Unfortunately, the best way to ensure that writes to the RTC have
 * actually finished is by waiting for a fixed duration. Emulators may not
 * accurately reflect this, but this delay is necessary on real hardware.
 * 
 * Special thanks to marshallh and jago85 for their hard work and research
 * reverse-engineering and documenting the inner-workings of the Joybus RTC.
 * 
 * @{
 */

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
 * @brief Read the identifier and status of the real-time clock.
 * 
 * @param[out]  identifier
 *              Destination pointer for the RTC identifier byte (or NULL).
 * @param[out]  status 
 *              Destination pointer for the RTC status byte (or NULL).
 */
void joybus_rtc_status( uint8_t * identifier, uint8_t * status )
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

    uint8_t * recv_buf = (uint8_t *)&output[1];

    if( identifier != NULL ) *identifier = recv_buf[1];
    if( status != NULL ) *status = recv_buf[2];
}

/**
 * @brief Probe the RTC interface on the cartridge.
 *
 * Some flash carts require the RTC to be explicitly enabled and
 * will not report the RTC as present by default.
 * 
 * Some emulators do not support the RTC correctly or at all.
 *
 * @return whether the real-time clock was detected on the cartridge.
 */
bool joybus_rtc_present( void )
{
    uint8_t identifier;
    joybus_rtc_status( &identifier, NULL );
    return identifier == JOYBUS_RTC_IDENTIFIER;
}

/**
 * @brief Read the status of the real-time clock.
 * 
 * The RTC will be busy when the control block is in
 * #JOYBUS_RTC_CONTROL_MODE_SET and is ready to be written to.
 * 
 * @return whether the Joybus RTC is busy
 */
bool joybus_rtc_busy( void )
{
    uint8_t status;
    joybus_rtc_status( NULL, &status );
    return status == JOYBUS_RTC_STATUS_BUSY;
}

/**
 * @brief Read the control block data from the real-time clock.
 * 
 * The control block contains the bits for:
 * #JOYBUS_RTC_CONTROL_MODE_SET when writing to the RTC.
 * #JOYBUS_RTC_CONTROL_BLOCK1_WRITE_PROTECTED is not used.
 * #JOYBUS_RTC_CONTROL_BLOCK2_WRITE_PROTECTED when not writing to the RTC.
 * 
 * @return the RTC control block data
 */
uint16_t joybus_rtc_read_control( void )
{
    static const uint64_t input[8] =
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

    controller_exec_pif( input, output );

    uint8_t * recv_buf = (uint8_t *)&output[1];

    return (((uint16_t)recv_buf[1]) << 8) | recv_buf[0];
}

/**
 * @brief Read the current date/time from the real-time clock.
 *
 * @param[out]  rtc_time
 *              Structure to place the current time
 * 
 * @return the status of the real-time clock
 */
uint8_t joybus_rtc_read_time( rtc_time_t * rtc_time )
{
    static const uint64_t input[8] =
    {
        0x0000000002090702,
        0xffffffffffffffff,
        0xfffe000000000000,
        0,
        0,
        0,
        0,
        1
    };
    static uint64_t output[8];

    controller_exec_pif( input, output );

    uint8_t * recv_buf = (uint8_t *)&output[1];

    rtc_time->sec = bcd_to_byte(recv_buf[0]);
    rtc_time->min = bcd_to_byte(recv_buf[1]);
    rtc_time->hour = bcd_to_byte(recv_buf[2] - 0x80);
    rtc_time->day = bcd_to_byte(recv_buf[3]);
    rtc_time->week_day = bcd_to_byte(recv_buf[4]);
    rtc_time->month = bcd_to_byte(recv_buf[5]) - 1;
    rtc_time->year = bcd_to_byte(recv_buf[6]);
    rtc_time->year += (uint16_t)bcd_to_byte(recv_buf[7]) * 100;
    rtc_time->year += 1900;

    return recv_buf[8];
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
 * @param[in]  control
 *             The control block data
 * 
 * @return the status of the real-time clock
 */
uint8_t joybus_rtc_write_control( uint16_t control )
{
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

    uint8_t * send_buf = (uint8_t *)&input[1];

    send_buf[0] = control >> 8;
    send_buf[1] = control;

    controller_exec_pif( input, output );

    uint8_t * recv_buf = (uint8_t *)&output[2];

    return recv_buf[0];
}

/**
 * @brief Write a new date/time to the real-time clock.
 * 
 * This will fail silently if block 2 of the RTC is write-protected.
 * The RTC control block should be set to #JOYBUS_RTC_CONTROL_MODE_SET
 * before calling this function.
 * 
 * It may take up to 20 milliseconds after this function returns for the
 * write to actually complete on real hardware. It is recommended to wait
 * using #JOYBUS_RTC_WRITE_DELAY before setting the RTC control block back
 * to #JOYBUS_RTC_CONTROL_MODE_RUN after setting the time. 
 *
 * @param[in]  rtc_time
 *             Structure containing the updated time
 * 
 * @return the status of the real-time clock
 */
uint8_t joybus_rtc_write_time( const rtc_time_t * rtc_time )
{
    static uint64_t input[8] =
    {
        0x000000000A010802,
        0xffffffffffffffff,
        0xfffe000000000000,
        0,
        0,
        0,
        0,
        1
    };
    static uint64_t output[8];

    uint8_t * send_buf = (uint8_t *)&input[1];

    int year = rtc_time->year - 1900;
    send_buf[0] = byte_to_bcd(rtc_time->sec);
    send_buf[1] = byte_to_bcd(rtc_time->min);
    send_buf[2] = byte_to_bcd(rtc_time->hour) + 0x80;
    send_buf[3] = byte_to_bcd(rtc_time->day);
    send_buf[4] = byte_to_bcd(rtc_time->week_day);
    send_buf[5] = byte_to_bcd(rtc_time->month + 1);
    send_buf[6] = byte_to_bcd(year);
    send_buf[7] = byte_to_bcd(year / 100);

    controller_exec_pif( input, output );

    uint8_t * recv_buf = (uint8_t *)&output[2];

    return recv_buf[0];
}

/**
 * @brief High-level convenience helper to set the RTC date/time.
 * 
 * Prepares the RTC for writing, sets the new time, and resumes the clock.
 * 
 * This process will take approximately 70 milliseconds to complete.
 * 
 * @param[in,out]  rtc_time
 *                 Structure containing the updated time
 * 
 * @return the final status of the real-time clock 
 */
uint8_t joybus_rtc_set( const rtc_time_t * rtc_time )
{
    uint8_t status;
    joybus_rtc_write_control( JOYBUS_RTC_CONTROL_MODE_SET );
    wait_ms( JOYBUS_RTC_WRITE_DELAY );
    joybus_rtc_write_time( rtc_time );
    wait_ms( JOYBUS_RTC_WRITE_DELAY );
    joybus_rtc_write_control( JOYBUS_RTC_CONTROL_MODE_RUN );
    wait_ms( JOYBUS_RTC_WRITE_DELAY );
    joybus_rtc_status( NULL, &status );
    return status;
}

/** @} */ /* rtc */
