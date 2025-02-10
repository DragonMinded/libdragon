/**
 * @file rtc.c
 * @brief Real-Time Clock Subsystem
 * @ingroup rtc
 */

#include "bb_rtc.h"
#include "dd.h"
#include "debug.h"
#include "joybus_rtc.h"
#include "libcart/cart.h"
#include "n64sys.h"
#include "rtc.h"
#include "rtc_internal.h"
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

/** @brief RTC initial timestamp (1996-01-01 00:00:00) */
#define RTC_CACHE_TIME_INIT 820454400

// MARK: Static variables

static volatile rtc_state_t rtc_state = RTC_STATE_INIT;
static rtc_source_t rtc_source = RTC_SOURCE_NONE;
static int rtc_sync_result = RTC_ESUCCESS;

static int rtc_source_joybus_persistent = -1;
static int rtc_source_dd_persistent = -1;
static int rtc_source_bb_persistent = -1;

/** @brief Tick counter state when rtc_cache_time was last updated. */
static int64_t rtc_cache_ticks = 0;
/** @brief Internal cache of the time from the last sync with the hardware RTC. */
static time_t rtc_cache_time = RTC_CACHE_TIME_INIT;

// MARK: Internal functions

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
static int rtc_get_time( time_t *out )
{
    // Act like a dumb software clock if not initialized
    if( rtc_state != RTC_STATE_INIT ) WAIT_FOR_RTC_READY();

    if( rtc_sync_result != RTC_ESUCCESS )
    {
        return rtc_sync_result;
    }

    long long now_ticks = get_ticks();
    long long seconds_since = (now_ticks - rtc_cache_ticks) / TICKS_PER_SECOND;
    time_t new_time = rtc_cache_time + seconds_since;
    rtc_range_t range = rtc_get_supported_range();
    // Wrap the time around if it goes out of bounds
    while( new_time < range.min ) new_time += range.max - range.min;
    while( new_time > range.max ) new_time -= range.max - range.min;

    *out = new_time;
    return RTC_ESUCCESS;
}

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
static int rtc_set_time( time_t new_time )
{
    // Act like a dumb software clock if not initialized
    if( rtc_state != RTC_STATE_INIT ) WAIT_FOR_RTC_READY();

    rtc_range_t range = rtc_get_supported_range();
    if( new_time < range.min || new_time > range.max )
    {
        return RTC_EBADTIME;
    }

    int result = RTC_ESUCCESS;
    if( rtc_source == RTC_SOURCE_JOYBUS )
    {
        result = joybus_rtc_set_time( new_time );
    }
    else if( rtc_source == RTC_SOURCE_DD )
    {
        result = dd_rtc_set_time( new_time );
    }
    else if( rtc_source == RTC_SOURCE_BB )
    {
        result = bb_rtc_set_time( new_time );
    }

    if( result == RTC_ESUCCESS )
    {
        rtc_cache_time = new_time;
        rtc_cache_ticks = get_ticks();
    }
    return rtc_sync_result = result;
}

/**
 * @brief Resynchronize the subsystem's time with the source clock.
 */
static int rtc_resync_time( void )
{
    time_t new_time = rtc_cache_time;
    int result = RTC_ESUCCESS;
    if( rtc_source == RTC_SOURCE_JOYBUS )
    {
        result = joybus_rtc_get_time( &new_time );
    }
    else if( rtc_source == RTC_SOURCE_DD )
    {
        result = dd_rtc_get_time( &new_time );
    }
    else if( rtc_source == RTC_SOURCE_BB )
    {
        result = bb_rtc_get_time( &new_time );
    }

    if( result == RTC_ESUCCESS )
    {
        rtc_cache_time = new_time;
        rtc_cache_ticks = get_ticks();
    }
    return rtc_sync_result = result;
}

// MARK: Public functions

static void rtc_init_joybus_read_time_callback( int error, time_t rtc_time )
{
    // If the state has changed, abort the callback
    if( rtc_state != RTC_STATE_JOYBUS_READING ) return;

    if( error == RTC_ESUCCESS )
    {
        // Set the RTC subsystem to Joybus RTC time
        rtc_cache_time = rtc_time;
        rtc_cache_ticks = get_ticks();
        rtc_sync_result = RTC_ESUCCESS;
    }
    else
    {
        // Don't initialize into a broken state; try to fall-back to DD!
        // DD RTC should already be synced during rtc_init_async
        if( sys_dd() && rtc_sync_result == RTC_ESUCCESS )
        {
            rtc_source = RTC_SOURCE_DD;
        }
        else
        {
            // If DD is unavailable or also broken, fall-back to software RTC
            rtc_source = RTC_SOURCE_NONE;
            rtc_sync_result = RTC_ESUCCESS;
        }
    }

    rtc_state = RTC_STATE_READY;
}

static void rtc_init_joybus_starting_callback( void )
{
    // If the state has changed, abort the callback
    if( rtc_state != RTC_STATE_JOYBUS_STARTING ) return;

    rtc_state = RTC_STATE_JOYBUS_READING;
    joybus_rtc_get_time_async( rtc_init_joybus_read_time_callback );
}

static void rtc_init_joybus_detecting_callback( bool detected, joybus_rtc_status_t status )
{
    // If the state has changed, abort the callback
    if( rtc_state != RTC_STATE_JOYBUS_DETECTING ) return;

    rtc_source_joybus_persistent = detected && !status.battery_bad;
    if( detected && !status.crystal_bad && !status.battery_bad )
    {
        // Always prefer Joybus RTC if it is available and operational
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
    rtc_cache_time = RTC_CACHE_TIME_INIT;
    rtc_cache_ticks = 0;
    rtc_sync_result = RTC_ESUCCESS;

    /* Enable newlib time integration */
    rtc_hooks_t hooks = { &rtc_get_time, &rtc_set_time };
    hook_rtc_calls( &hooks );

    // BBPlayer RTC will never co-exist with other RTCs
    if( sys_bbplayer() )
    {
        rtc_source = RTC_SOURCE_BB;
        if( rtc_resync_time() != RTC_ESUCCESS )
        {
            // If BB RTC is broken, fall-back to software RTC
            rtc_source = RTC_SOURCE_NONE;
            rtc_sync_result = RTC_ESUCCESS;
        }
        rtc_state = RTC_STATE_READY;
    }
    else
    {
        // 64DD RTC may co-exist with the Joybus RTC
        if( sys_dd() )
        {
            rtc_source = RTC_SOURCE_DD;
            rtc_resync_time();
        }

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

    /* Enable newlib time integration */
    rtc_hooks_t hooks = { &rtc_get_time, &rtc_set_time };
    unhook_rtc_calls( &hooks );

    /* Reset state and cancel any pending async callbacks */
    rtc_state = RTC_STATE_INIT;
    rtc_source = RTC_SOURCE_NONE;
    rtc_cache_time = RTC_CACHE_TIME_INIT;
    rtc_cache_ticks = 0;
    rtc_sync_result = RTC_ESUCCESS;
}

rtc_source_t rtc_get_source( void )
{
    assert( rtc_state != RTC_STATE_INIT );
    WAIT_FOR_RTC_READY();

    return rtc_source;
}

int rtc_set_source( rtc_source_t source )
{
    assert( rtc_state != RTC_STATE_INIT );
    WAIT_FOR_RTC_READY();

    if( rtc_is_source_available( source ))
    {
        rtc_source = source;
        return rtc_resync_time();
    }
    return RTC_ENOCLOCK;
}

bool rtc_is_source_available( rtc_source_t source )
{
    assert( rtc_state != RTC_STATE_INIT );
    WAIT_FOR_RTC_READY();

    switch( source )
    {
        case RTC_SOURCE_NONE: return true;
        case RTC_SOURCE_JOYBUS: return joybus_rtc_detect();
        case RTC_SOURCE_DD: return sys_dd();
        case RTC_SOURCE_BB: return sys_bbplayer();
        default: return false;
    }
}

bool rtc_is_source_persistent( rtc_source_t source )
{
    assert( rtc_state != RTC_STATE_INIT );
    WAIT_FOR_RTC_READY();

    if( !rtc_is_source_available( source ))
    {
        return false;
    }

    time_t check_time;
    int result;
    if( source == RTC_SOURCE_JOYBUS )
    {
        // Only perform the persistent test once and cache the result
        if( rtc_source_joybus_persistent == -1 )
        {
            // Test writability by attempting to perform a read/write
            result = joybus_rtc_get_time( &check_time );
            if( result != RTC_ESUCCESS )
            {
                rtc_source_joybus_persistent = false;
            }
            else
            {
                result = joybus_rtc_set_time( check_time );
                rtc_source_joybus_persistent = (result == RTC_ESUCCESS);
            }
        }
        return rtc_source_joybus_persistent;
    }
    if( source == RTC_SOURCE_DD )
    {
        // Only perform the persistent test once and cache the result
        if( rtc_source_dd_persistent == -1 )
        {
            // Test writability by attempting to perform a write
            result = dd_rtc_get_time( &check_time );
            if( result != RTC_ESUCCESS )
            {
                rtc_source_dd_persistent = false;
            }
            else
            {
                result = dd_rtc_set_time( check_time );
                rtc_source_dd_persistent = (result == RTC_ESUCCESS);
            }
        }
        return rtc_source_dd_persistent;
    }
    if( source == RTC_SOURCE_BB )
    {
            // Only perform the persistent test once and cache the result
        if( rtc_source_bb_persistent == -1 )
        {
            // Test writability by attempting to perform a write
            result = bb_rtc_get_time( &check_time );
            if( result != RTC_ESUCCESS )
            {
                rtc_source_bb_persistent = false;
            }
            else
            {
                result = bb_rtc_set_time( check_time );
                rtc_source_bb_persistent = (result == RTC_ESUCCESS);
            }
        }
        return rtc_source_bb_persistent;
    }
    // Software RTC does not persist across reset/power-off
    return false;
}

bool rtc_is_persistent( void )
{
    return rtc_is_source_persistent( rtc_source );
}

rtc_range_t rtc_get_source_supported_range( rtc_source_t source )
{
    if( source == RTC_SOURCE_JOYBUS )
    {
        return (rtc_range_t){
            .min = JOYBUS_RTC_TIMESTAMP_MIN,
            .max = JOYBUS_RTC_TIMESTAMP_MAX,
        };
    }
    if( source == RTC_SOURCE_DD )
    {
        return (rtc_range_t){
            .min = DD_RTC_TIMESTAMP_MIN,
            .max = DD_RTC_TIMESTAMP_MAX,
        };
    }
    if( source == RTC_SOURCE_BB )
    {
        return (rtc_range_t){
            .min = BB_RTC_TIMESTAMP_MIN,
            .max = BB_RTC_TIMESTAMP_MAX,
        };
    }
    return (rtc_range_t){
        .min = RTC_SOFT_TIMESTAMP_MIN,
        .max = RTC_SOFT_TIMESTAMP_MAX,
    };
}

rtc_range_t rtc_get_supported_range( void )
{
    return rtc_get_source_supported_range( rtc_source );
}

const char *rtc_error_str( int error )
{
    switch( error )
    {
        case RTC_ESUCCESS: return "Success";
        case RTC_ENOCLOCK: return "Clock not available";
        case RTC_EBADCLOCK: return "Clock not operational";
        case RTC_EBADTIME: return "Invalid clock time";
        default: return "Unknown clock error";
    }
}

/** @deprecated Use #rtc_get_time instead. */
bool rtc_get( rtc_time_t * rtc_time )
{
    time_t current_time;
    int result = rtc_get_time( &current_time );
    if( result != RTC_ESUCCESS ) return false;
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
    return rtc_set_time( new_time ) == RTC_ESUCCESS;
}

/** @deprecated Use #rtc_is_persistent instead. */
bool rtc_is_writable( void )
{
    return rtc_is_persistent();
}

/** @} */ /* rtc */
