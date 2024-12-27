/**
 * @file rtc.c
 * @brief Real-Time Clock Subsystem
 * @ingroup rtc
 */

#include "joybus_rtc.h"
#include "libcart/cart.h"
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

/**
 * @brief Tick counter state when #rtc_get cache was last updated.
 */
static int64_t rtc_cache_ticks = 0;

/**
 * @brief Internal cache of the most-recent RTC time read.
 *
 * Initial value will be overwritten unless there is no RTC available.
 * Initial value is 1996-01-01 00:00:00 UTC because the RTC subsystem
 * only supports dates between 1996-2095.
 */
static time_t rtc_cache_time = 820454400;

// MARK: Public functions

bool rtc_init( void )
{
    // Just increment the refcount if already initialized
	if (rtc_init_refcount++ > 0) return rtc_source != RTC_SOURCE_NONE;

    // Initialize libcart (if necessary) to detect common flashcart types
    // TODO: the iQue check can be removed once libcart is updated to include it
    if (cart_type <= CART_NULL && !sys_bbplayer())
    {
        cart_init();
    }

    /* Reset subsystem state */
    rtc_source = RTC_SOURCE_NONE;
    rtc_cache_ticks = get_ticks();
    rtc_cache_time = 0;

    /* For now, only Joybus RTC is supported */
    if( rtc_is_source_available( RTC_SOURCE_JOYBUS ))
    {
        rtc_source = RTC_SOURCE_JOYBUS;
        joybus_rtc_set_stopped( false );
        rtc_resync_time();
    }
    // TODO: 64DD RTC is not yet implemented.
    // TODO: BBPlayer RTC is not yet implemented.

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
}

rtc_source_t rtc_get_source( void )
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
    if( source == RTC_SOURCE_NONE )
    {
        return true;
    }
    if( source == RTC_SOURCE_JOYBUS )
    {
        return joybus_rtc_detect();
    }
    // TODO: 64DD RTC is not yet implemented.
    // TODO: BBPlayer RTC is not yet implemented.
    return false;
}

bool rtc_resync_time( void )
{
    if( rtc_source == RTC_SOURCE_JOYBUS )
    {
        rtc_cache_time = joybus_rtc_read_time();
        rtc_cache_ticks = get_ticks();
        return true;
    }
    return false;
}

time_t rtc_get_time( void )
{
    long long now_ticks = get_ticks();
    long long seconds_since = (now_ticks - rtc_cache_ticks) / TICKS_PER_SECOND;
    return (time_t)(rtc_cache_time + seconds_since);
}

bool rtc_set_time( time_t new_time )
{
    if( new_time < RTC_TIMESTAMP_MIN || new_time > RTC_TIMESTAMP_MAX )
    {
        return false;
    }

    bool written = false;
    if( rtc_source == RTC_SOURCE_JOYBUS )
    {
        written = joybus_rtc_set_time( new_time );
    }
    /* Update cache state */
    rtc_cache_time = new_time;
    rtc_cache_ticks = get_ticks();
    return written;
}

bool rtc_is_persistent( void )
{
    if( rtc_source == RTC_SOURCE_JOYBUS )
    {
        // Test writability by performing a write
        return rtc_set_time( rtc_get_time() );
    }
    // TODO: 64DD RTC is not yet implemented.
    // TODO: BBPlayer RTC is not yet implemented.
    // Software RTC cannot be persistent.
    return false;
}

/** @deprecated Use #rtc_get_time instead. */
bool rtc_get( rtc_time_t * rtc_time )
{
    time_t current_time = rtc_get_time();
    struct tm * timeinfo = gmtime( &current_time );
    if( timeinfo == NULL ) return false;
    *rtc_time = rtc_time_from_tm( timeinfo );
    return true;
}

/** @deprecated Use #rtc_set_time instead. */
bool rtc_set( rtc_time_t * write_time )
{
    struct tm timeinfo = rtc_time_to_tm( write_time );
    time_t new_time = mktime( &timeinfo );
    return rtc_set_time( new_time );
}

/** @deprecated Use #rtc_is_persistent instead. */
bool rtc_is_writable( void )
{
    return rtc_is_persistent();
}

/** @} */ /* rtc */
