/**
 * @file rtc.h
 * @brief Real-Time Clock Subsystem
 * @ingroup rtc
 */
#ifndef __LIBDRAGON_RTC_H
#define __LIBDRAGON_RTC_H

#include <stdint.h>
#include <stdbool.h>

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
 * To check if the real-time clock supports writes, call #rtc_is_writable.
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
 * (As of July 2021) Joybus RTC does not work in combination with any EEPROM
 * save type on EverDrive64 3.0 or X7. To have the best compatibility and player
 * experience, it is not recommended to use the EEPROM + RTC ROM configuration.
 * This is a bug in the EverDrive64 firmware and not a system limitation imposed
 * by the Joybus protocol or Serial Interface.
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
 * whether writes are supported using #rtc_is_writable so that you can
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
 * RTC will not be detected if the EEPROM save type is used; RTC writes are
 * not supported through the SI, so changing the time must be done in the OS.
 *
 * UltraPIF fully implements an emulated Joybus RTC that can be accessed even
 * when the cartridge does not include the real-time clock circuitry.
 *
 * Special thanks to marshallh and jago85 for their hard work and research
 * reverse-engineering and documenting the inner-workings of the Joybus RTC.
 *
 * @{
 */

/**
 * @brief Structure for storing RTC time data.
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
 * 
 * Unhooks the RTC from the newlib gettimeofday function.
 * This will cause subsequent calls to gettimeofday to error with ENOSYS.
 *
 * The only reason you should ever need to call this is if you need to
 * stop the Timer Subsystem, which the RTC Subsystem depends on. If you
 * do need to do this, make sure to call #rtc_close BEFORE #timer_close
 * and then call #rtc_init again after you call #timer_init to restart
 * the Timer Subsystem!
 */
void rtc_close( void );

/**
 * @brief Determine whether the RTC supports writing the time.
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
 * @return whether RTC writes appear to be supported
 */
bool rtc_is_writable( void );

/**
 * @brief Read the current date/time from the real-time clock.
 *
 * If the RTC is not detected or supported, this function will
 * not modify the destination rtc_time parameter.
 *
 * Your code should call this once per frame to update the #rtc_time_t
 * data structure. The RTC Subsystem maintains a cache of the
 * most-recent RTC time that was read and will only perform an
 * actual RTC read command if the cache is invalidated. The
 * destination rtc_time parameter will be updated regardless of
 * the cache validity.
 *
 * Cache will invalidate every #RTC_GET_CACHE_INVALIDATE_TICKS.
 * Calling #rtc_set will also invalidate the cache.
 *
 * If an actual RTC read command is needed, this function can take
 * a few milliseconds to complete.
 *
 * @param[out]  rtc_time
 *              Destination pointer for the RTC time data structure
 *
 * @return whether the rtc_time destination pointer data was modified
 */
bool rtc_get( rtc_time_t * rtc_time );

/**
 * @brief High-level convenience helper to set the RTC date/time.
 *
 * Prepares the RTC for writing, sets the new time, and resumes the clock.
 *
 * This function will take approximately 570 milliseconds to complete.
 *
 * Unfortunately, the best way to ensure that writes to the RTC have
 * actually finished is by waiting for a fixed duration. Emulators may not
 * accurately reflect this, but this delay is necessary on real hardware.
 *
 * @param[in]   rtc_time
 *              Source pointer for the RTC time data structure
 *
 * @return false if the RTC does not support being set
 */
bool rtc_set( rtc_time_t * rtc_time );

/**
 * @brief Calculate sane values for arbitrary time inputs.
 *
 * If your time inputs are already sane, nothing should change.
 * This function will clamp date/time values within the expected ranges,
 * including the correct day-of-month based on year/month. It will also
 * recalculate the day-of-week based on the clamped year/month/day.
 *
 * This is useful to call while the player is adjusting the time after each
 * input to ensure that the date being set always makes sense before they
 * actually confirm and commit the updated date/time. The rtctest example
 * demonstrates a user-interface for setting the time with live validation.
 *
 * Internally, RTC cannot represent dates before 1990-01-01, although some
 * RTC implementations (like UltraPIF) only support dates after 2000-01-01.
 *
 * For highest compatibility, it is not recommended to set the date past
 * 2038-01-19 03:14:07 UTC, which is the UNIX timestamp Epochalypse.
 * 
 * Special thanks to networkfusion for providing the algorithm to
 * calculate day-of-week from an arbitrary date.
 *
 * @param[in,out] rtc_time
 *                Pointer to the RTC time data structure
 */
void rtc_normalize_time( rtc_time_t * rtc_time );

#ifdef __cplusplus
}
#endif

/** @} */ /* rtc */

#endif
