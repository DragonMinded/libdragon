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
 * @addtogroup rtc
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

void joybus_rtc_init( void );

bool joybus_rtc_detect( void );

/**
 * @brief Read the status of the Joybus real-time clock.
 *
 * The RTC should be stopped when the control registers are in
 * #JOYBUS_RTC_CONTROL_MODE_SET and the time is ready to be written to.
 *
 * @return whether the Joybus RTC status is stopped
 */
bool joybus_rtc_is_stopped( void );

/**
 * @brief Read the current date/time from the Joybus real-time clock.
 *
 * The result of calling this function when the Joybus RTC is not
 * present is undefined and may not be safe. #rtc_get will not
 * call this function if the Joybus RTC was not detected.
 *
 * @param[out]  rtc_time
 *              Destination pointer for the RTC time data structure
 */
time_t joybus_rtc_read_time( void );

/**
 * @brief Write a new date/time to the Joybus real-time clock.
 *
 * If writes are not supported by the emulator or flash cart, this function
 * will fail silently. It is recommended to call #rtc_is_writable early
 * in your program to detect whether the RTC actually supports writes.
 *
 * This will also fail silently if block 2 of the RTC is write-protected.
 *
 * The RTC control registers should be set to #JOYBUS_RTC_CONTROL_MODE_SET
 * using #joybus_rtc_write_control before calling this function.
 *
 * The RTC control registers should be set to #JOYBUS_RTC_CONTROL_MODE_RUN
 * using #joybus_rtc_write_control after calling this function.
 *
 * It may take up to 20 milliseconds after this function returns for the
 * write to actually complete on real hardware. It is recommended to wait
 * using #JOYBUS_RTC_WRITE_BLOCK_DELAY before setting the RTC control registers
 * back to #JOYBUS_RTC_CONTROL_MODE_RUN after setting the time. On 64drive
 * it is recommended to wait using #JOYBUS_RTC_WRITE_FINISHED_DELAY before
 * reading the time to ensure the internal clock has ticked and copied
 * the new time data into the "SI shadow interface".
 *
 * This is a low-level function that needs to be used in proper sequence.
 * For normal use, call the the high-level #rtc_set function which handles
 * the RTC control block writes, delays, and status checks.
 *
 * @param[in]  rtc_time
 *             Source pointer for the RTC time data structure
 */
void joybus_rtc_write_time( time_t new_time );

/**
 * @brief High-level convenience helper to set the Joybus RTC date/time.
 *
 * Prepares the RTC for writing, sets the new time, and resumes the clock.
 *
 * This function will take approximately 570 milliseconds to complete.
 *
 * Unfortunately, the best way to ensure that writes to the RTC have
 * actually finished is by waiting for a fixed duration. Emulators may not
 * accurately reflect this, but this delay is necessary on real hardware.
 *
 * @param[in]   new_time
 *              Source pointer for the RTC time data structure
 *
 * @return false if the RTC does not support being set
 */
bool joybus_rtc_set_time( time_t new_time );

void joybus_rtc_wait_for_write_finished( void );

#ifdef __cplusplus
}
#endif

/** @} */ /* rtc */

#endif
