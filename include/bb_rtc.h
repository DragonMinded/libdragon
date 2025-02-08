#ifndef LIBDRAGON_BB_RTC_H
#define LIBDRAGON_BB_RTC_H

#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief BBPlayer RTC minimum timestamp (2000-01-01 00:00:00) */
#define BB_RTC_TIMESTAMP_MIN 946684800
/**
 * @brief BBPlayer RTC maximum timestamp (2099-12-31 23:59:59)
 *
 * The RTC has a "century" bit that could be used to extend the
 * date range, but the iQue Menu disables it, so LibDragon currently
 * makes no attempt to use it.
 **/
#define BB_RTC_TIMESTAMP_MAX 4102444799

/**
 * @brief Read the time from the BBPlayer RTC as a UNIX timestamp
 *
 * @param[out] out pointer to the output time_t
 *
 * @retval RTC_ESUCCESS if the operation was successful
 * @retval RTC_ENOCLOCK if the RTC is not available
 * @retval RTC_EBADCLOCK if the RTC is not operational
 * @retval RTC_EBADTIME if the RTC time is not representable
**/
int bb_rtc_get_time( time_t *out );

/**
 * @brief Read the time from the BBPlayer RTC as a UNIX timestamp
 *
 * @param new_time the new time to set the BBPlayer RTC to as a UNIX timestamp
 *
 * @retval RTC_ESUCCESS if the operation was successful
 * @retval RTC_ENOCLOCK if the RTC is not available
 * @retval RTC_EBADCLOCK if the RTC is not operational
 * @retval RTC_EBADTIME if the RTC cannot represent the new time
**/
int bb_rtc_set_time( time_t new_time );

#ifdef __cplusplus
}
#endif

#endif // LIBDRAGON_BB_RTC_H
