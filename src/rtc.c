/**
 * @file rtc.c
 * @brief Real-Time Clock Subsystem
 * @ingroup rtc
 */

#include "joybus_rtc.h"
#include "n64sys.h"
#include "rtc.h"
#include "rtc_utils.h"
#include "system.h"
#include "timer.h"

/**
 * @addtogroup rtc
 * @{
 */

// MARK: Static variables

/** @brief Refcount of #rtc_init vs #rtc_close calls. */
static int rtc_init_refcount = 0;

/** @brief Preferred Real-time Clock source */
static rtc_source_t rtc_source = RTC_SOURCE_NONE;

/** @brief Joybus RTC detection state */
static bool rtc_detected_joybus = false;

/**
 * @brief Tick counter state when #rtc_get cache was last updated.
 */
static int64_t rtc_cache_ticks = 0;

/**
 * @brief Internal cache of the most-recent RTC time read.
 * 
 * Initial value is not significant and will be overwritten
 * unless there is no RTC available.
 */
static time_t rtc_cache_time = 0;

// MARK: Public functions

bool rtc_init( void )
{
    // Just increment the refcount if already initialized
	if (rtc_init_refcount++ > 0) return rtc_source != RTC_SOURCE_NONE;

    // Initialize the timer subsystem (or increment its refcount)
    timer_init();

    /* Reset subsystem state */
    rtc_source = RTC_SOURCE_NONE;
    rtc_detected_joybus = false;
    rtc_cache_ticks = timer_ticks();
    rtc_cache_time = 0;

    /* Detect any available real-time clocks */
    if ( joybus_rtc_detect() )
    {
        rtc_detected_joybus = true;
        rtc_source = RTC_SOURCE_JOYBUS;
        joybus_rtc_init();
        rtc_resync_time();
    }

    /* Enable newlib time integration */
    time_hooks_t hooks = { &rtc_get_time, &rtc_set_time };
    hook_time_calls( &hooks );
    
    return rtc_source != RTC_SOURCE_NONE;
}

void rtc_close( void )
{
    // Do nothing if there are still dangling references.
	if (--rtc_init_refcount > 0) { return; }

    /* Disable newlib time integration */
    time_hooks_t hooks = { &rtc_get_time, &rtc_set_time };
    unhook_time_calls( &hooks );

    // Decrement the timer subsystem refcount (possibly closing it)
    timer_close();
}

bool rtc_get_source( void )
{ 
    return rtc_source;
}

bool rtc_set_source( rtc_source_t source )
{
    if( rtc_is_source_available( source ))
    {
        rtc_source = source;
        return true;
    }
    return false;
}

bool rtc_is_source_available( rtc_source_t source )
{
    if( source == RTC_SOURCE_JOYBUS ) return rtc_detected_joybus;
    return false;
}

bool rtc_resync_time( void )
{
    if( rtc_source == RTC_SOURCE_JOYBUS )
    {
        rtc_cache_time = joybus_rtc_read_time();
        rtc_cache_ticks = timer_ticks();
        return true;
    }
    return false;
}

time_t rtc_get_time( void )
{
    int seconds_since = TICKS_SINCE( rtc_cache_ticks ) / TICKS_PER_SECOND;
    return rtc_cache_time + seconds_since;
}

bool rtc_set_time( time_t new_time )
{
    bool written = false;
    if( rtc_source == RTC_SOURCE_JOYBUS )
    {
        written = joybus_rtc_set_time( new_time );
    }
    /* Update cache state */
    rtc_cache_time = new_time;
    rtc_cache_ticks = timer_ticks();
    return written;
}

bool rtc_is_persistent( void )
{
    time_t restore_time = rtc_cache_time;
    /* write_time could be any time except the present time */
    time_t write_time = restore_time + 60;
    
    bool written = rtc_set_time( write_time );
    if( !written ) return false;

    if( rtc_source == RTC_SOURCE_JOYBUS )
    {
        joybus_rtc_wait_for_write_finished();
    }

    if ( !rtc_resync_time() ) return false;
    time_t verify_time = rtc_cache_time;

    if( write_time == verify_time || write_time + 1 == verify_time )
    {
        rtc_set_time( restore_time );
        return true;
    }
    return false;
}

bool rtc_get( rtc_time_t * rtc_time )
{
    time_t current_time = rtc_get_time();
    struct tm * timeinfo = gmtime( &current_time );
    if( timeinfo == NULL ) return false;
    *rtc_time = rtc_time_from_tm( timeinfo );
    return true;
}

bool rtc_set( rtc_time_t * write_time )
{
    struct tm timeinfo = rtc_time_to_tm( write_time );
    time_t new_time = mktime( &timeinfo );
    return rtc_set_time( new_time );
}

bool rtc_is_writable( void ) 
{
    return rtc_is_persistent(); 
}
