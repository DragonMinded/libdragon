#include "libdragon.h"

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

void joybus_rtc_status( uint8_t * identifier, uint8_t * status )
{
    static const uint64_t SI_rtc_status_block[8] =
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

    controller_exec_pif( SI_rtc_status_block, output );

    uint8_t * recv_buf = (uint8_t *)&output[1];

    if( identifier != NULL ) *identifier = recv_buf[1];
    if( status != NULL ) *status = recv_buf[2];
}

/**
 * @brief Probe the RTC interface on the cartridge.
 *
 * @see #joybus_rtc_status
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
 * @see #joybus_rtc_status
 * 
 * @return whether the Joybus RTC is paused
 */
bool joybus_rtc_paused( void )
{
    uint8_t status;
    joybus_rtc_status( NULL, &status );
    return status == JOYBUS_RTC_STATUS_PAUSED;
}

/**
 * @brief Read the control block from the real-time clock.
 * 
 * The control block contains the bits for:
 * @see #JOYBUS_RTC_CONTROL_CLOCK_PAUSED
 * @see #JOYBUS_RTC_CONTROL_BLOCK1_WRITE_PROTECTED
 * @see #JOYBUS_RTC_CONTROL_BLOCK2_WRITE_PROTECTED
 * 
 * @return the RTC control block data
 */
uint16_t joybus_rtc_read_control( void )
{
    static const uint64_t SI_rtc_read_block[8] =
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

    controller_exec_pif( SI_rtc_read_block, output );

    uint8_t * recv_buf = (uint8_t *)&output[1];

    return (((uint16_t)recv_buf[1]) << 8) | recv_buf[0];
}

/**
 * @brief Read the current time from the real-time clock.
 *
 * @param[out]  rtc_time
 *              Structure to place the current time
 * 
 * @return the current status of the real-time clock
 */
uint8_t joybus_rtc_read_time( rtc_time_t * rtc_time )
{
    static const uint64_t SI_rtc_read_block[8] =
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

    controller_exec_pif( SI_rtc_read_block, output );

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
 * The control block is used to set the bits for:
 * @see #JOYBUS_RTC_CONTROL_CLOCK_PAUSED
 * @see #JOYBUS_RTC_CONTROL_BLOCK1_WRITE_PROTECTED
 * @see #JOYBUS_RTC_CONTROL_BLOCK2_WRITE_PROTECTED
 * 
 * Typically this would be set to one of two "modes":
 * @see #JOYBUS_RTC_CONTROL_MODE_SET
 * @see #JOYBUS_RTC_CONTROL_MODE_RUN
 *
 * @param[in]  control
 *             The control block data
 * 
 * @return the current status of the real-time clock
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

    send_buf[0] = control;
    send_buf[1] = control >> 8;

    controller_exec_pif( input, output );

    uint8_t * recv_buf = (uint8_t *)&output[2];

    return recv_buf[0];
}

/**
 * @brief Write a new time to the real-time clock.
 * 
 * This will fail if block 2 of the RTC is not writable.
 * @see #joybus_rtc_write_control
 * @see #JOYBUS_RTC_CONTROL_BLOCK2_WRITE_PROTECTED
 *
 * @param[in]  rtc_time
 *             Structure containing the updated time
 * 
 * @return the current status of the real-time clock
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
