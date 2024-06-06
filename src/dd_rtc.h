/**
 * @file dd_rtc.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief 64DD Real-Time Clock Utilities
 * @ingroup rtc
 */

#ifndef __LIBDRAGON_DD_RTC_H
#define __LIBDRAGON_DD_RTC_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/**
 * @addtogroup rtc
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

bool dd_rtc_detect( void );

time_t dd_rtc_get_time( void );

void dd_rtc_set_time( time_t new_time );

#ifdef __cplusplus
}
#endif

/** @} */ /* rtc */

#endif
