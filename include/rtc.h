/**
 * @file rtc.h
 * @brief Real-Time Clock Subsystem
 * @ingroup rtc
 */
#ifndef __LIBDRAGON_RTC_H
#define __LIBDRAGON_RTC_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @addtogroup rtc
 * @{
 */

/**
 * @brief Real-time clock identifier byte.
 * 
 * A magic value in the RTC status response that indicates the RTC is present.
 */
#define JOYBUS_RTC_IDENTIFIER 0x10

/**
 * @brief The RTC is running; time should update every second.
 * 
 * Some emulators incorrectly always respond with this status.
 */
#define JOYBUS_RTC_STATUS_RUNNING 0x00
/**
 * @brief The RTC is paused; time should not update.
 * 
 * Some emulators do not support pausing the RTC.
 */
#define JOYBUS_RTC_STATUS_PAUSED 0x80

/**
 * @brief Control bit for if the RTC should pause time updates.
 * 
 * This should be on when setting the date/time.
 */
#define JOYBUS_RTC_CONTROL_CLOCK_PAUSED          0x0004
/**
 * @brief Control bit for if RTC block 1 is write-protected.
 * 
 * RTC block 1 has an unknown purpose, but this is included for completeness.
 */
#define JOYBUS_RTC_CONTROL_BLOCK1_WRITE_PROTECTED 0x0100
/**
 * @brief Control bit for if RTC block 2 is write-protected.
 * 
 * RTC block 2 contains the date and time.
 * This should be on unless setting the date/time.
 */
#define JOYBUS_RTC_CONTROL_BLOCK2_WRITE_PROTECTED 0x0200

/**
 * @brief Control mode for setting the RTC date/time.
 * 
 * Pauses the clock and clears the write-protection bits.
 */
#define JOYBUS_RTC_CONTROL_MODE_SET (JOYBUS_RTC_CONTROL_CLOCK_PAUSED)
/**
 * @brief Control mode for normal operation of the RTC.
 * 
 * Enables the clock and sets the date/time write-protection bit.
 */
#define JOYBUS_RTC_CONTROL_MODE_RUN (JOYBUS_RTC_CONTROL_BLOCK2_WRITE_PROTECTED)

/**
 * @brief Structure for storing RTC time data.
 */
typedef struct rtc_time_t
{
    /** @brief Year. [1900-20XX] */
    uint16_t year;
    /** @brief Month. [0-11] */
    uint8_t month;
    /** @brief Day of month. [1-31] */
    uint8_t day;
    /** @brief Hours. [0-23] */
    uint8_t hour;
    /** @brief Minutes. [0-59] */
    uint8_t min;
    /** @brief Seconds. [0-59] */
    uint8_t sec;
    /** @brief Day of week. [0-6] */
    uint8_t week_day;
} rtc_time_t;

#ifdef __cplusplus
extern "C" {
#endif

bool joybus_rtc_present( void );
bool joybus_rtc_paused( void );
void joybus_rtc_status( uint8_t * identifier, uint8_t * status );
uint16_t joybus_rtc_read_control( void );
uint8_t joybus_rtc_write_control( uint16_t control );
uint8_t joybus_rtc_read_time( rtc_time_t * rtc_time );
uint8_t joybus_rtc_write_time( const rtc_time_t * rtc_time );

#ifdef __cplusplus
}
#endif

/** @} */ /* rtc */

#endif
