/**
 * @file rtc_utils.c
 * @brief Real-Time Clock Subsystem Utilities
 * @ingroup rtc
 */

#include "rtc.h"

/**
 * @addtogroup rtc
 * @{
 */

/**
 * @brief Lookup table for number of days in each month.
 *
 * Used by #rtc_normalize_time to clamp the day-of-month value.
 *
 * Does not account for leap years in the month of February.
 */
static const uint8_t DAYS_IN_MONTH[] =
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

uint8_t bcd_to_byte(uint8_t bcd)
{
    uint8_t hi = (bcd & 0xF0) >> 4;
    uint8_t lo = bcd & 0x0F;
    return (hi * 10) + lo;
}

uint8_t byte_to_bcd(uint8_t byte)
{
    byte %= 100;
    return ((byte / 10) << 4) | (byte % 10);
}

int rtc_time_yday( const rtc_time_t * rtc_time )
{
    int yday = 0;
    for( int i = 0; i < rtc_time->month; i++ )
    {
        yday += DAYS_IN_MONTH[i];
    }
    yday += rtc_time->day;
    return yday;
}

int rtc_time_wday( const rtc_time_t * rtc_time )
{
    int month = rtc_time->month;
    int year = rtc_time->year;
    int day = rtc_time->day;

    month = month + 1; /* Adjust month to 1-indexed */
    if( month < 3 )
    {
        month = month + 12;
        year = year - 1;
    }
    return (
        day
        + (2 * month)
        + (6 * (month + 1) / 10)
        + year
        + (year / 4)
        - (year / 100)
        + (year / 400)
        + 1
    ) % 7;
}

struct tm rtc_time_to_tm( const rtc_time_t * rtc_time )
{
    return (struct tm){
        .tm_sec = rtc_time->sec,
        .tm_min = rtc_time->min,
        .tm_hour = rtc_time->hour,
        .tm_mday = rtc_time->day,
        .tm_mon = rtc_time->month,
        .tm_year = rtc_time->year - 1900,
        .tm_wday = rtc_time->week_day,
        .tm_yday = rtc_time_yday(rtc_time),
        .tm_isdst = -1, /* Auto-detect Daylight Saving Time */
    };
}

time_t rtc_time_to_time( const rtc_time_t * rtc_time )
{
    struct tm time = rtc_time_to_tm(rtc_time);
    return mktime( &time );
}

rtc_time_t rtc_time_from_tm( const struct tm * time )
{
    return (rtc_time_t){
        .year = time->tm_year + 1900,
        .month = time->tm_mon,
        .day = time->tm_mday,
        .hour = time->tm_hour,
        .min = time->tm_min,
        .sec = time->tm_sec,
        .week_day = time->tm_wday,
    };
}

void rtc_normalize_time( rtc_time_t * rtc_time )
{
    /* Clamp date/time values that have static limits */
    if( rtc_time->year < 1900 ) rtc_time->year = 1900;
    if( rtc_time->month > 11 ) rtc_time->month = 11;
    if( rtc_time->hour > 23 ) rtc_time->hour = 23;
    if( rtc_time->min > 59 ) rtc_time->min = 59;
    if( rtc_time->sec > 59 ) rtc_time->sec = 59;

    /* Clamp the day of the month based on the year and month */
    int year = rtc_time->year;
    int month = rtc_time->month;
    int days_in_month = DAYS_IN_MONTH[month];
    bool leap_year = year % 400 == 0 || (year % 100 != 0 && year % 4 == 0);
    if( month == 1 && leap_year ) days_in_month = 29;
    if( rtc_time->day > days_in_month ) rtc_time->day = days_in_month;

    rtc_time->week_day = rtc_time_wday(rtc_time);
}

/** @} */ /* rtc_utils */
