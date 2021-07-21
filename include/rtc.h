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
    /** @brief Day of week. [0-6] (Sun-Sat) */
    uint8_t week_day;
} rtc_time_t;

#ifdef __cplusplus
extern "C" {
#endif

bool rtc_init( void );
void rtc_close( void );
bool rtc_is_writable( void );
bool rtc_get( rtc_time_t * rtc_time );
bool rtc_set( rtc_time_t * rtc_time );
void rtc_normalize_time( rtc_time_t * rtc_time );

#ifdef __cplusplus
}
#endif

/** @} */ /* rtc */

#endif
