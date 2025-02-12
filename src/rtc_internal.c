/**
 * @file rtc_internal.c
 * @brief Real-Time Clock subsystem internal API
 * @ingroup rtc
 */

#include <time.h>

#include "rtc.h"
#include "utils.h"

/**
 * @addtogroup rtc
 * @{
 */

int bcd_decode( uint8_t bcd )
{
    uint8_t hi = (bcd & 0xF0) >> 4;
    uint8_t lo = bcd & 0x0F;
    return (hi * 10) + lo;
}

uint8_t bcd_encode( int value )
{
    value %= 100;
    return ((value / 10) << 4) | (value % 10);
}

struct tm rtc_time_to_tm( const rtc_time_t * rtc_time )
{
    struct tm t = {
        .tm_sec = CLAMP( rtc_time->sec, 0, 59 ),
        .tm_min = CLAMP( rtc_time->min, 0, 59 ),
        .tm_hour = CLAMP( rtc_time->hour, 0, 23 ),
        .tm_mday = CLAMP( rtc_time->day, 1, 31 ),
        .tm_mon = CLAMP( rtc_time->month, 0, 11 ),
        .tm_year = CLAMP( rtc_time->year, 1900, 2099 ) - 1900,
    };
    time_t timestamp = mktime( &t );
    return *gmtime( &timestamp );
}

rtc_time_t rtc_time_from_tm( const struct tm * time )
{
    return (rtc_time_t){
        .year = CLAMP( time->tm_year + 1900, 1900, 2099 ),
        .month = CLAMP( time->tm_mon, 0, 11 ),
        .day = CLAMP( time->tm_mday, 1, 31 ),
        .hour = CLAMP( time->tm_hour, 0, 23 ),
        .min = CLAMP( time->tm_min, 0, 59 ),
        .sec = CLAMP( time->tm_sec, 0, 59 ),
        .week_day = CLAMP( time->tm_wday, 0, 6 ),
    };
}

void rtc_normalize_time( rtc_time_t * rtc_time )
{
    struct tm t = rtc_time_to_tm( rtc_time );
    *rtc_time = rtc_time_from_tm( &t );
}

/** @} */ /* rtc_internal */
