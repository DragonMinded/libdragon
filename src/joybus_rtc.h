/**
 * @file joybus_rtc.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joybus Real-Time Clock Utilities
 * @ingroup rtc
 */

#ifndef __LIBDRAGON_JOYBUS_RTC_H
#define __LIBDRAGON_JOYBUS_RTC_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/**
 * @defgroup joybus_rtc Joybus Real-Time Clock
 * @ingroup rtc
 * @brief Joybus Real-Time Clock interface.
 * @author Christopher Bonhage
 *
 * The Joybus real-time clock is a cartridge peripheral that uses a battery
 * to power a clock that tracks the date, time, and day of the week. The
 * real-time clock keeps running even when the N64 is powered-off. The
 * Joybus RTC is accessed through the serial interface (SI) similar to EEPROM
 * and controllers. The Joybus RTC was only ever available on one official
 * cartridge that was only available in Japan: Dōbutsu no Mori (Animal Forest).
 * Many emulators and flash carts include support for the Animal Forest RTC,
 * which makes it possible to include real-time clock functionality in N64
 * homebrew!
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
 * a 500ms delay after setting the time in order to read it back correctly.
 *
 * EverDrive64 3.0 and X7 partially support Joybus RTC, with caveats: The RTC
 * must be explicitly enabled in the OS or with a ROM header configuration;
 * RTC writes are not supported over Joybus -- changing the time must be done
 * in the EverDrive OS menu.
 *
 * UltraPIF fully implements an emulated Joybus RTC that can be accessed even
 * when the cartridge does not include the real-time clock circuitry.
 *
 * Internally, Joybus RTC cannot represent dates before 1900-01-01, although
 * some RTC implementations (like UltraPIF) only support dates after
 * 2000-01-01. Joybus RTC cannot represent dates after 2099-12-31. If the
 * clock goes beyond this range, it will wrap around to 1900-01-01.
 *
 * Special thanks to korgeaux, marshallh and jago85 for their hard work
 * and research reverse-engineering and documenting the inner-workings
 * of the Joybus RTC.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Joybus RTC minimum timestamp (1900-01-01 00:00:00) */
#define JOYBUS_RTC_TIMESTAMP_MIN -2208988800
/** @brief Joybus RTC maximum timestamp (2099-12-31 23:59:59) */
#define JOYBUS_RTC_TIMESTAMP_MAX 4102444799

/** @brief Joybus RTC status byte */
typedef union
{
    /** @brief raw byte value */
    uint8_t byte;
    /// @cond
    struct __attribute__((packed))
    {
    /// @endcond
        /** @brief Set if the RTC is stopped. */
        unsigned stopped     : 1;
        unsigned             : 5;
        /** @brief Set if RTC crystal is not working. */
        unsigned crystal_bad : 1;
        /** @brief Set if the RTC battery voltage is too low. */
        unsigned battery_bad : 1;
    /// @cond
    };
    /// @endcond
} joybus_rtc_status_t;

/** @brief Callback function signature for #joybus_rtc_detect_async */
typedef void (*joybus_rtc_detect_callback_t)(bool detected, joybus_rtc_status_t status);

/** @brief Callback function signature for #joybus_rtc_set_stopped_async */
typedef void (*joybus_rtc_set_stopped_callback_t)(void);

/** @brief Callback function signature for #joybus_rtc_get_time_async */
typedef void (*joybus_rtc_get_time_callback_t)(int error, time_t time);

/**
 * @brief Detect the presence of the Joybus real-time clock asynchronously.
 *
 * @param callback function to call when detection is complete
 */
void joybus_rtc_detect_async( joybus_rtc_detect_callback_t callback );

/**
 * @brief Detect the presence of the Joybus real-time clock.
 *
 * @return whether the Joybus RTC was detected and operational
 */
bool joybus_rtc_detect( void );

/**
 * @brief Check if the Joybus real-time clock is stopped.
 *
 * The clock must be stopped in order to set the time,
 * and should not be stopped during normal operation.
 */
bool joybus_rtc_is_stopped( void );

/**
 * @brief Set the stop state of the Joybus real-time clock asynchronously.
 *
 * The clock must be stopped in order to set the time,
 * and should not be stopped during normal operation.
 *
 * @param stop whether to stop the clock
 * @param callback function to call when the stop state has been set
 */
void joybus_rtc_set_stopped_async( bool stop, joybus_rtc_set_stopped_callback_t callback );

/**
 * @brief Change the stop state of the Joybus real-time clock.
 *
 * The clock must be stopped in order to set the time,
 * and should not be stopped during normal operation.
 *
 * @param stop whether to stop the clock
 */
void joybus_rtc_set_stopped( bool stop );

/**
 * @brief Read the date/time of the Joybus real-time clock asynchronously.
 *
 * @param callback function to call when the time has been read
 */
void joybus_rtc_get_time_async( joybus_rtc_get_time_callback_t callback );

/**
 * @brief Read the current date/time from the Joybus real-time clock.
 *
 * @param[out] out pointer to the output time_t
 *
 * @retval RTC_ESUCCESS if the operation was successful
 * @retval RTC_ENOCLOCK if the RTC is not available
 * @retval RTC_EBADCLOCK if the RTC crystal is bad
 * @retval RTC_EBADTIME if the RTC cannot represent the new time
 */
int joybus_rtc_get_time( time_t *out );

/**
 * @brief Set the date/time on the Joybus real-time clock.
 *
 * Prepares the RTC for writing, sets the new time, and resumes the clock.
 *
 * @param new_time the new RTC time as a UNIX timestamp
 *
 * @retval RTC_ESUCCESS if the operation was successful
 * @retval RTC_ENOCLOCK if the RTC is not available
 * @retval RTC_EBADCLOCK if the RTC crystal is bad
 * @retval RTC_EBADTIME if the RTC cannot represent the new time
 */
int joybus_rtc_set_time( time_t new_time );

#ifdef __cplusplus
}
#endif

/** @} */ /* rtc */

#endif
