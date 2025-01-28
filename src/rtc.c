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

/** @brief RTC initialization state enumeration */
typedef enum {
    RTC_STATE_INIT = 0,         ///< Init has not been called yet
    RTC_STATE_JOYBUS_DETECTING, ///< Detecting Joybus RTC
    RTC_STATE_JOYBUS_STARTING,  ///< Starting Joybus RTC
    RTC_STATE_JOYBUS_READING,   ///< Reading Joybus RTC
    RTC_STATE_READY,            ///< RTC subsystem is ready
} rtc_state_t;

/** @brief Convenience macro to spinlock while waiting for RTC subsystem to initialize. */
#define WAIT_FOR_RTC_READY() while( rtc_state != RTC_STATE_READY ) { /* Spinlock! */ }

// MARK: Static variables

static volatile rtc_state_t rtc_state = RTC_STATE_INIT;

/** @brief Preferred Real-time Clock source */
static rtc_source_t rtc_source = RTC_SOURCE_NONE;

/**
 * @brief Tick counter state when #rtc_get cache was last updated.
 */
static int64_t rtc_cache_ticks = 0;

/** @brief RTC initial timestamp (2000-01-01 00:00:00) */
#define RTC_CACHE_TIME_INIT 946684800

/**
 * @brief Internal cache of the most-recent RTC time read.
 *
 * Initial value will be overwritten unless there is no RTC available.
 * Initial value is 2000-01-01 00:00:00 UTC because the RTC subsystem
 * only supports dates between 1996-2095.
 */
static time_t rtc_cache_time = RTC_CACHE_TIME_INIT;

// MARK: Internal functions

/**
 * @brief Read the current date/time from the real-time clock.
 *
 * @return the current RTC time as a UNIX timestamp
 */
static time_t rtc_get_time( void )
{
    WAIT_FOR_RTC_READY();

    long long now_ticks = get_ticks();
    long long seconds_since = (now_ticks - rtc_cache_ticks) / TICKS_PER_SECOND;
    return (time_t)(rtc_cache_time + seconds_since);
}

/**
 * @brief Set a new date/time for the real-time clock.
 *
 * @param new_time the new time to set the RTC to
 *
 * @return whether the time was written to the RTC
 */
static bool rtc_set_time( time_t new_time )
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

static void rtc_resync_time_joybus_callback( time_t rtc_time )
{
    if( rtc_source == RTC_SOURCE_JOYBUS )
    {
        rtc_cache_time = rtc_time;
        rtc_cache_ticks = get_ticks();
    }
}

/**
 * @brief Resynchronize the subsystem's time with the source clock.
 */
static void rtc_resync_time( void )
{
    if( rtc_source == RTC_SOURCE_JOYBUS )
    {
        joybus_rtc_read_time_async( rtc_resync_time_joybus_callback );
    }
    // TODO: 64DD RTC is not yet implemented.
    // TODO: BBPlayer RTC is not yet implemented.
}

// MARK: Public functions

static void rtc_init_joybus_read_time_callback( time_t rtc_time )
{
    if( rtc_state != RTC_STATE_JOYBUS_READING ) return;

    rtc_cache_time = rtc_time;
    rtc_cache_ticks = get_ticks();
    rtc_state = RTC_STATE_READY;
}

static void rtc_init_joybus_starting_callback( void )
{
    if( rtc_state != RTC_STATE_JOYBUS_STARTING ) return;

    rtc_state = RTC_STATE_JOYBUS_READING;
    joybus_rtc_read_time_async( rtc_init_joybus_read_time_callback );
}

static void rtc_init_joybus_detecting_callback( bool detected )
{
    if( rtc_state != RTC_STATE_JOYBUS_DETECTING ) return;

    if( detected )
    {
        // Always prefer Joybus RTC if it is available
        rtc_source = RTC_SOURCE_JOYBUS;
        rtc_state = RTC_STATE_JOYBUS_STARTING;
        joybus_rtc_set_stopped_async( false, rtc_init_joybus_starting_callback );
    }
    else
    {
        rtc_state = RTC_STATE_READY;
    }
}

void rtc_init_async( void )
{
    assert( rtc_state == RTC_STATE_INIT );

    /* Reset subsystem state */
    rtc_source = RTC_SOURCE_NONE;
    rtc_cache_ticks = get_ticks();
    rtc_cache_time = RTC_CACHE_TIME_INIT;

    /* Enable newlib time integration */
    time_hooks_t hooks = { &rtc_get_time, &rtc_set_time };
    hook_time_calls( &hooks );

    if( sys_bbplayer() )
    {
        // TODO: BBPlayer RTC is not yet implemented.
        rtc_state = RTC_STATE_READY;
    }
    else
    {
        // TODO: 64DD RTC is not yet implemented.

        // Initialize libcart (if necessary) to detect common flashcart types
        if( cart_type <= CART_NULL ) cart_init();

        // Attempt to detect and start the Joybus RTC asynchronously
        rtc_state = RTC_STATE_JOYBUS_DETECTING;
        joybus_rtc_detect_async( rtc_init_joybus_detecting_callback );
    }
}

bool rtc_init( void )
{
    if( rtc_state == RTC_STATE_INIT ) rtc_init_async();
    while( rtc_state != RTC_STATE_READY ) { /* Spinlock! */ }
    return rtc_source != RTC_SOURCE_NONE;
}

void rtc_close( void )
{
    if( rtc_state == RTC_STATE_INIT ) return;

    /* Disable newlib time integration */
    time_hooks_t hooks = { &rtc_get_time, &rtc_set_time };
    unhook_time_calls( &hooks );

    /* Reset state and cancel any pending async callbacks */
    rtc_state = RTC_STATE_INIT;
}

rtc_source_t rtc_get_source( void )
{
    WAIT_FOR_RTC_READY();

    return rtc_source;
}

bool rtc_set_source( rtc_source_t source )
{
    WAIT_FOR_RTC_READY();

    if( source == RTC_SOURCE_NONE )
    {
        rtc_source = RTC_SOURCE_NONE;
        return true;
    }
    if( rtc_is_source_available( source ))
    {
        rtc_source = source;
        rtc_resync_time();
        return true;
    }
    return false;
}

bool rtc_is_source_available( rtc_source_t source )
{
    WAIT_FOR_RTC_READY();

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

bool rtc_is_persistent( void )
{
    WAIT_FOR_RTC_READY();

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
