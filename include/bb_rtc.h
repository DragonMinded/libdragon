#ifndef LIBDRAGON_BB_RTC_H
#define LIBDRAGON_BB_RTC_H

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

#ifdef __cplusplus
}
#endif

#endif // LIBDRAGON_BB_RTC_H
