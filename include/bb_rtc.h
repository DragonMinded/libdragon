#ifndef LIBDRAGON_BB_RTC_H
#define LIBDRAGON_BB_RTC_H

#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read the time from the BBPlayer RTC as a UNIX timestamp
 *
 * @return the current BBPlayer RTC time as a UNIX timestamp (or 0 if error)
**/
time_t bb_rtc_get_time( void );

/**
 * @brief Read the time from the BBPlayer RTC as a UNIX timestamp
 *
 * @param new_time the new time to set the BBPlayer RTC to as a UNIX timestamp
 *
 * @return whether the BBPlayer RTC write was successful
**/
bool bb_rtc_set_time( time_t new_time );

#ifdef __cplusplus
}
#endif

#endif // LIBDRAGON_BB_RTC_H
