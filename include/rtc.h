/**
 * @file rtc.h
 * @author Christopher Bonhage <me@christopherbonhage.com>
 * @brief Real-Time Clock Subsystem
 * @ingroup rtc
 */
#ifndef __LIBDRAGON_RTC_H
#define __LIBDRAGON_RTC_H


#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "preview.h"

/**
 * @defgroup rtc Real-Time Clock Subsystem
 * @ingroup peripherals
 * @brief Real-time clock interface.
 * @author Christopher Bonhage
 *
 * The RTC subsystem integrates several hardware clocks with newlib time
 * functions. Supported sources are identified by #rtc_source_t:
 *
 * - **Joybus** (#RTC_SOURCE_JOYBUS): PIF/controller-port RTC on retail
 *   hardware and most flash carts. This is preferred when present and
 *   reporting a good crystal and battery.
 * - **64DD** (#RTC_SOURCE_DD): RTC in the 64DD ASIC when a disk drive is
 *   attached. It can coexist with the Joybus RTC; initialization may read
 *   the 64DD first, then switch to Joybus if that RTC is detected and healthy.
 * - **BBPlayer / iQue** (#RTC_SOURCE_BB): RTC on iQue Player hardware only;
 *   it does not coexist with the other sources.
 *
 * Call #rtc_init_async to start detection without blocking, or #rtc_init to
 * block until the subsystem is ready (typically within a few milliseconds).
 * #rtc_init returns `true` if any **hardware** source was selected, `false`
 * if the subsystem fell back to the software clock (#RTC_SOURCE_NONE).
 *
 * If no hardware clock is usable, the subsystem keeps time in software for
 * the current session only; that value does not survive reset or power loss.
 *
 * Initialization installs hooks so POSIX `gettimeofday` / `settimeofday` and
 * ISO C `time` work as usual (for example `time(NULL)` for seconds since the
 * UNIX epoch). Use `struct tm` with `gmtime`, `localtime`, and `mktime` as on
 * any hosted C environment.
 *
 * Use #rtc_is_source_available, #rtc_get_source, and #rtc_set_source to query
 * or change the active source. #rtc_set_source resynchronizes cached time from
 * the newly selected hardware.
 *
 * Each source accepts a different range of `time_t` values. Query
 * #rtc_get_supported_range or #rtc_get_source_supported_range instead of
 * hard-coding limits. Roughly: Joybus is represented from 1900 through 2099
 * (some real chips, e.g. UltraPIF, effectively only support from 2000); 64DD
 * uses two-digit years interpreted as 1996–1999 and 2000–2095; BBPlayer spans
 * 2000–2099 by default (a century bit can extend the hardware range; LibDragon
 * does not enable it automatically). The software fallback allows 1970 through
 * 2099 (#RTC_SOFT_TIMESTAMP_MIN / #RTC_SOFT_TIMESTAMP_MAX). When the cached
 * time would leave the active source’s range, reads wrap inside that range.
 *
 * Writes outside the supported range fail with #RTC_EBADTIME. Prefer storing
 * game-specific “fiction time” as an offset from the wall clock rather than
 * programming impossible dates into hardware.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief RTC source values. */
typedef enum {
    /** @brief Software RTC source */
    RTC_SOURCE_NONE = 0,
    /** @brief Joybus RTC source */
    RTC_SOURCE_JOYBUS = 1,
    /** @brief 64DD RTC source */
    RTC_SOURCE_DD = 2,
    /** @brief iQue/BBPlayer RTC source */
    RTC_SOURCE_BB = 3,
} rtc_source_t;

/**
 * @name RTC error codes
 * @{
 */
/** @brief RTC Operation successful */
#define RTC_ESUCCESS   0
/** @brief RTC source in unavailable. */
#define RTC_ENOCLOCK  -1
/** @brief RTC source is not operational. */
#define RTC_EBADCLOCK -2
/** @brief RTC clock time is not representable.  */
#define RTC_EBADTIME  -3
/** @} */

/** @brief Get the string representation of an RTC error code.
 * @preview
 */
LIBDRAGON_PREVIEW_API
const char *rtc_error_str( int error );

/** @brief Software RTC minimum timestamp (1970-01-01 00:00:00) */
#define RTC_SOFT_TIMESTAMP_MIN 0
/** @brief Software RTC maximum timestamp (2099-12-31 23:59:59) */
#define RTC_SOFT_TIMESTAMP_MAX 4102444799

/** @brief Structure representing an RTC timestamp range. */
typedef struct {
    time_t min; ///< Minimum timestamp
    time_t max; ///< Maximum timestamp
} rtc_range_t;

/**
 * @brief Initialize the RTC subsystem asynchronously.
 * @preview
 *
 * Some flash carts require the RTC to be explicitly enabled before loading
 * the ROM file. Some emulators and flash carts do not support RTC at all.
 *
 * This function will detect if the RTC is available and if so, will
 * prepare the RTC so that the current time can be read from it.
 *
 * This will also hook the RTC into the newlib gettimeofday and settimeofday
 * functions, so you will be able to use the ISO C time functions.
 *
 * This operation may take up to 5 milliseconds to complete, but does not
 * block the CPU while detecting and initializing the RTC hardware.
 *
 * Use #rtc_get_source to determine if a hardware RTC source was detected.
 */
LIBDRAGON_PREVIEW_API
void rtc_init_async( void );

/**
 * @brief Initialize the RTC subsystem.
 *
 * Some flash carts require the RTC to be explicitly enabled before loading
 * the ROM file. Some emulators and flash carts do not support RTC at all.
 *
 * This function will detect if the RTC is available and if so, will
 * prepare the RTC so that the current time can be read from it.
 *
 * This will also hook the RTC into the newlib gettimeofday and settimeofday
 * functions, so you will be able to use the ISO C time functions.
 *
 * This operation may take up to 5 milliseconds to complete.
 *
 * @return whether any supported hardware RTC source was initialized
 */
bool rtc_init( void );

/**
 * @brief Close the RTC subsystem, disabling system hooks.
 */
void rtc_close( void );

/**
 * @brief Check if the specified RTC source is usable by the subsystem.
 * @preview
 */
LIBDRAGON_PREVIEW_API
bool rtc_is_source_available( rtc_source_t source );

/**
 * @brief Get the current source clock for the subsystem.
 * @preview
 */
LIBDRAGON_PREVIEW_API
rtc_source_t rtc_get_source( void );

/**
 * @brief Switch the preferred source clock for the subsystem.
 * @preview
 *
 * By default, the subsytem will use to the first available source,
 * but some games may wish to specify the preferred RTC source.
 *
 * This function will automatically resynchronize the time with the new clock.
 *
 * @retval RTC_ESUCCESS if the source was successfully set
 * @retval RTC_ENOCLOCK if the source is not available
 * @retval RTC_EBADCLOCK if the source is not operational
 */
LIBDRAGON_PREVIEW_API
int rtc_set_source( rtc_source_t source );

/**
 * @brief Get the supported timestamp range for the given RTC source.
 * @preview
 *
 * @param source the RTC source to check
 *
 * @return the supported timestamp range for the source
 */
LIBDRAGON_PREVIEW_API
rtc_range_t rtc_get_source_supported_range( rtc_source_t source );

/**
 * @brief Get the supported timestamp range for the current RTC source.
 * @preview
 *
 * @return the supported timestamp range for the current source clock
 */
LIBDRAGON_PREVIEW_API
rtc_range_t rtc_get_supported_range( void );

/**************************************
 *  DEPRECATED
 **************************************/

/// @cond

/**
 * @brief Structure for storing RTC time data.
 * @deprecated Use `struct tm` and `time_t` from <time.h> instead.
 */
typedef struct rtc_time_t
{
    /** @brief Year. [1900-20XX] */
    uint16_t year;
    /** @brief Month. [0-11] */
    uint8_t month;
    /** @brief Day of month. [1-31] */
    uint8_t day;
    /** @brief Hours. [0-23] */
    uint8_t hour;
    /** @brief Minutes. [0-59] */
    uint8_t min;
    /** @brief Seconds. [0-59] */
    uint8_t sec;
    /** @brief Day of week. [0-6] (Sun-Sat) */
    uint8_t week_day;
} rtc_time_t;

__attribute__((deprecated("just assume it's always writable")))
static inline bool rtc_is_writable( void ) { return true; }

__attribute__((deprecated("use time(NULL) instead")))
bool rtc_get( rtc_time_t *rtc_time );

__attribute__((deprecated("use settimeofday instead")))
bool rtc_set( rtc_time_t *rtc_time );

/// @endcond

#ifdef __cplusplus
}
#endif

/** @} */ /* rtc */

#endif
