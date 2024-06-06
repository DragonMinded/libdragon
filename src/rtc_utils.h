/**
 * @file rtc_utils.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Real-Time Clock Subsystem Utilities
 * @ingroup rtc
 */

#ifndef __LIBDRAGON_RTC_UTILS_H
#define __LIBDRAGON_RTC_UTILS_H

#include "rtc.h"

/**
 * @addtogroup rtc
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Decode a packed binary-coded decimal number.
 *
 * @param[in]   bcd
 *              packed binary-coded decimal number
 *
 * @return the decoded integer
 */
uint8_t bcd_to_byte(uint8_t bcd);

/**
 * @brief Encode a packed binary-coded decimal number.
 *
 * @param[in]   byte
 *              integer to encode
 *
 * @return the encoded packed binary-coded decimal number
 */
uint8_t byte_to_bcd(uint8_t byte);


int rtc_time_yday( const rtc_time_t * rtc_time );

int rtc_time_wday( const rtc_time_t * rtc_time );

/**
 * @brief Convert rtc_time_t into struct tm.
 *
 * @param[in]   rtc_time
 *              rtc_time_t to convert
 *
 * @return the converted struct tm
 */
struct tm rtc_time_to_tm( const rtc_time_t * rtc_time );

/**
 * @brief Convert rtc_time_t into time_t.
 *
 * @param[in]   time
 *              rtc_time_t to convert
 *
 * @return the converted time_t
 */
time_t rtc_time_to_time( const rtc_time_t * rtc_time );

/**
 * @brief Convert a struct tm into rtc_time_t.
 *
 * @param[in]   time
 *              struct tm to convert
 *
 * @return the converted rtc_time_t
 */
rtc_time_t rtc_time_from_tm( const struct tm * time );

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
 * actually confirm and commit the updated date/time. The rtctest example
 * demonstrates a user-interface for setting the time with live validation.
 *
 * Internally, RTC cannot represent dates before 1990-01-01, although some
 * RTC implementations (like UltraPIF) only support dates after 2000-01-01.
 *
 * For highest compatibility, it is not recommended to set the date past
 * 2038-01-19 03:14:07 UTC, which is the UNIX timestamp Epochalypse.
 * 
 * Special thanks to networkfusion for providing the algorithm to
 * calculate day-of-week from an arbitrary date.
 *
 * @param[in,out] rtc_time
 *                Pointer to the RTC time data structure
 */
void rtc_normalize_time( rtc_time_t * rtc_time );


#ifdef __cplusplus
}
#endif

/** @} */ /* rtc */

#endif
