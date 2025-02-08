/**
 * @file rtc_internal.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Real-Time Clock subsystem internal API
 * @ingroup rtc
 */

#ifndef __LIBDRAGON_RTC_INTERNAL_H
#define __LIBDRAGON_RTC_INTERNAL_H

#include "rtc.h"

/**
 * @addtogroup rtc
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name RTC error codes
 * @{
 */
/** @brief RTC Operation successful */
#define RTC_ESUCCESS   0
/** @brief RTC source in unavailable. */
#define RTC_ENOCLOCK  -1
/** @brief RTC source is not operational. */
#define RTC_EBADCLOCK -2
/** @brief RTC clock time is not representable.  */
#define RTC_EBADTIME  -3
/** @} */

/**
 * @brief Read the current date/time from the real-time clock subsystem.
 *
 * @param[out] out pointer to the output time_t
 *
 * @retval RTC_ESUCCESS if the operation was successful
 * @retval RTC_ENOCLOCK if the RTC is not available
 * @retval RTC_EBADCLOCK if the RTC is not operational
 * @retval RTC_EBADTIME if the RTC clock time is not representable
 */
int rtc_get_time( time_t *out );

/**
 * @brief Set a new date/time for the real-time clock subsystem.
 *
 * @param new_time the new time to set the RTC to
 *
 * @retval RTC_ESUCCESS if the operation was successful
 * @retval RTC_ENOCLOCK if the RTC is not available
 * @retval RTC_EBADCLOCK if the RTC is not operational
 * @retval RTC_EBADTIME if the RTC cannot represent the new time
 */
int rtc_set_time( time_t new_time );

/**
 * @brief Decode a packed binary-coded decimal number.
 *
 * @param[in]   bcd
 *              packed binary-coded decimal number
 *
 * @return the decoded integer
 */
int bcd_decode( uint8_t bcd );

/**
 * @brief Encode a packed binary-coded decimal number.
 *
 * @param[in]   value
 *              integer to encode
 *
 * @return the encoded packed binary-coded decimal number
 */
uint8_t bcd_encode( int value );

/**
 * @brief Convert rtc_time_t into struct tm.
 *
 * @deprecated use `struct tm` from <time.h> instead of #rtc_time_t
 *
 * @param[in]   rtc_time
 *              rtc_time_t to convert
 *
 * @return the converted struct tm
 */
struct tm rtc_time_to_tm( const rtc_time_t * rtc_time );

/**
 * @brief Convert a struct tm into rtc_time_t.
 *
 * @deprecated use `struct tm` from <time.h> instead of #rtc_time_t
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
 * @deprecated use `struct tm` from <time.h> instead of #rtc_time_t
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
