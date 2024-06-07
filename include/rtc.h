/**
 * @file rtc.h
 * @brief Real-Time Clock Subsystem
 * @ingroup rtc
 */
#ifndef __LIBDRAGON_RTC_H
#define __LIBDRAGON_RTC_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/**
 * @defgroup rtc Real-Time Clock Subsystem
 * @ingroup peripherals
 * @brief Real-time clock interface.
 * @author Christopher Bonhage
 *
 * The Joybus real-time clock is a cartridge peripheral that uses a battery
 * to power a clock that tracks the date, time, and day of the week. The
 * real-time clock keeps running even when the N64 is powered-off. The
 * Joybus RTC is accessed through the serial interface (SI) similar to EEPROM
 * and controllers. The Joybus RTC was only ever available on one official
 * cartridge that was only available in Japan: Dōbutsu no Mori (Animal Forest).
 * Many emulators and flash carts include support for the Animal Forest RTC,
 * which makes it possible to include real-time clock functionality in homebrew!
 * There is also a real-time clock included in the N64DD hardware, which uses
 * a different interface and is not currently supported by libdragon.
 *
 * To check if the real-time clock is available, call #rtc_init.
 * To read the current time from the real-time clock, call #rtc_get.
 * Once the RTC subsystem is initialized, you can also use ISO C Time functions
 * to get the current time, for example: `time(NULL)` will return the number of
 * seconds elapsed since the UNIX epoch (January 1, 1970 at 00:00:00).
 * To check if the real-time clock supports writes, call #rtc_is_persistent.
 * To write a new time to the real-time clock, call #rtc_set.
 *
 * This subsystem handles decoding and encoding the date/time from its internal
 * format into a struct called #rtc_time_t, which contains integer values for
 * year, month, day-of-month, day-of-week, hour, minute, and second.
 *
 * The Joybus RTC contains 3 "blocks" (or zones) which contain 8 bytes of data:
 * Block 0 contains a half-word control register and opaque calibration data.
 * Block 1 is unused and unsupported. See notes below.
 * Block 2 contains the current date/time as packed binary-coded decimal.
 *
 * Animal Forest did not use block 1 at all, so most emulators do not bother to
 * implement it. Theoretically, block 1 could be used as 8-bytes of SRAM-backed
 * storage, but this is not supported by libdragon's Real-Time Clock Subsystem.
 * If you need storage, consider using a standard cartridge save type or saving
 * to a Controller Pak.
 *
 * Unfortunately, since only one game ever used Joybus RTC (and that game was
 * later re-released on the GameCube in English), real-time clock support in
 * emulators and flash carts can be incomplete, inaccurate, or non-existent.
 * Many emulators do not actually implement the Joybus RTC write command and
 * always respond with the host device's current local time. Some emulators
 * and flash carts support writing to RTC but will not persist the date/time
 * after resetting or powering-off. You can run the `rtctest` example ROM on
 * your preferred emulator or flash cart to what RTC support is available.
 *
 * The only reliable way to check if writes are actually supported is to write
 * a time to the RTC and read the time back out. Many emulators that do
 * support RTC reads will silently ignore RTC writes. You should detect
 * whether writes are supported using #rtc_is_persistent so that you can
 * conditionally show the option to change the time if it's supported. If the
 * RTC supports writes, it is safe to call #rtc_set to set the date and time.
 *
 * Due to the inaccurate and inconsistent behavior of RTC reproductions that
 * currently exist, this subsystem trades-off complete accuracy with the actual
 * Animal Forest RTC in favor of broader compatibility with the various quirks
 * and bugs that exist in real-world scenarios like emulators and flash carts.
 *
 * Some notable examples of RTC support in the ecosystem (as of July 2021):
 *
 * 64drive hw2 fully implements Joybus RTC including writes, but requires
 * delays after setting the time (see #JOYBUS_RTC_WRITE_FINISHED_DELAY).
 *
 * EverDrive64 3.0 and X7 partially support Joybus RTC, with caveats: The RTC
 * must be explicitly enabled in the OS or with a ROM header configuration;
 * RTC writes are not supported -- changing the time must be done in the ED menu.
 *
 * UltraPIF fully implements an emulated Joybus RTC that can be accessed even
 * when the cartridge does not include the real-time clock circuitry.
 *
 * Special thanks to marshallh and jago85 for their hard work and research
 * reverse-engineering and documenting the inner-workings of the Joybus RTC.
 *
 * @{
 */

/** @brief RTC source values. */
typedef enum {
    /** @brief Software RTC source */
    RTC_SOURCE_NONE = 0,
    /** @brief Joybus RTC source */
    RTC_SOURCE_JOYBUS = 1,
    /** @brief 64DD RTC source (Not implemented yet) */
    RTC_SOURCE_DD = 2
} rtc_source_t;

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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief High-level convenience helper to initialize the RTC subsystem.
 * 
 * The RTC Subsystem depends on the libdragon Timer Subsystem, so make sure
 * to call #timer_init before calling #rtc_init!
 *
 * Some flash carts require the RTC to be explicitly enabled before loading
 * the ROM file. Some emulators and flash carts do not support RTC at all.
 *
 * This function will detect if the RTC is available and if so, will
 * prepare the RTC so that the current time can be read from it.
 *
 * This operation may take up to 50 milliseconds to complete.
 *
 * This will also hook the RTC into the newlib gettimeofday function, so
 * you will be able to use the ISO C time functions if RTC is available.
 *
 * @return whether the RTC is present and supported by the RTC Subsystem.
 */
bool rtc_init( void );

/**
 * @brief Close the RTC Subsystem, disabling system hooks.
 */
void rtc_close( void );

bool rtc_is_source_available( rtc_source_t source );

bool rtc_get_source( void );

/**
 * @brief Switch the preferred source clock for the subsystem.
 * 
 * By default, the subsytem will use to the first available source,
 * but some games may wish to specify the preferred RTC source.
 * 
 * Make sure you call #rtc_resync_time after switching sources!
*/
bool rtc_set_source( rtc_source_t source );

/**
 * @brief Resynchronize the subsystem's time with the source clock.
 * 
 * You should only need to do this after switching sources.
 */
bool rtc_resync_time( void );

/**
 * @brief Read the current date/time from the real-time clock.
 *
 * @return the current RTC time as a UNIX timestamp
 */
time_t rtc_get_time( void );

/** 
 * @brief Set a new date/time for the real-time clock.
 * 
 * Internally, Joybus RTC cannot represent dates before 1990-01-01, although some
 * RTC implementations (like UltraPIF) only support dates after 2000-01-01.
 * 
 * 64DD RTC only stores two digits for the year, so conventionally 96-99 are
 * treated as 1996-1999 and 00-95 are treated as 2000-2095.
 *
 * For highest compatibility, it is not recommended to set the date past
 * 2095-12-31 23:59:59 UTC.
 * 
 * @param new_time the new time to set the RTC to
 * 
 * @return whether the time was written to the RTC
 */
bool rtc_set_time( time_t new_time );

/**
 * @brief Determine whether the RTC actually supports writing the time.
 *
 * Some emulators and flash carts do not support writing to the RTC, so
 * this function makes an attempt to detect silent write failures and will
 * return `false` if it is unable to change the time on the RTC.
 *
 * This function is useful if your program wants to conditionally offer the
 * ability to set the time based on hardware/emulator support.
 *
 * Unfortunately this operation may introduce a slight drift in the clock,
 * but it is the only way to determine if the RTC supports the write command.
 *
 * This operation will take approximately 1 second to complete.
 *
 * @return whether RTC write persistence appears to be supported
 */
bool rtc_is_persistent( void );

__attribute__((deprecated("use rtc_is_persistent instead")))
bool rtc_is_writable( void );

__attribute__((deprecated("use rtc_get_time instead")))
bool rtc_get( rtc_time_t * rtc_time );

__attribute__((deprecated("use rtc_set_time instead")))
bool rtc_set( rtc_time_t * rtc_time );

#ifdef __cplusplus
}
#endif

/** @} */ /* rtc */

#endif
