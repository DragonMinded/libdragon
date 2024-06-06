/**
 * @file joybus_rtc.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joybus Real-Time Clock Utilities
 * @ingroup rtc
 */

#ifndef __LIBDRAGON_JOYBUS_RTC_H
#define __LIBDRAGON_JOYBUS_RTC_H

#include "rtc.h"

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
 * @brief Read a block of data from the Joybus real-time clock.
 *
 * This is a low-level utility function that is used by
 * #joybus_rtc_read_control and #rtc_get.
 *
 * @param[in]   block
 *              Which RTC block to read from (0-2)
 *
 * @param[out]  data
 *              Destination pointer for the RTC block data
 *
 * @return the status byte from the Joybus real-time clock
 */
uint8_t joybus_rtc_read( uint8_t block, uint64_t * data );

/**
 * @brief Write a block of data to the Joybus real-time clock.
 *
 * This is a low-level utility function that is used by
 * #joybus_rtc_write_control and #joybus_rtc_write_time.
 *
 * @param[in]   block
 *              Which RTC block to write to (0-2)
 *
 * @param[in]   data
 *              RTC block data to write
 *
 * @return the status byte from the Joybus real-time clock
 */
uint8_t joybus_rtc_write( uint8_t block, const uint64_t * data );

/**
 * @brief Read the control block from the Joybus real-time clock.
 *
 * The control registers should generally be one of the pre-defined modes:
 * #JOYBUS_RTC_CONTROL_MODE_SET when writing to the RTC.
 * #JOYBUS_RTC_CONTROL_MODE_RUN when not writing to the RTC.
 *
 * The calibration data is an opaque value that should be propagated when
 * calling #joybus_rtc_write_control. Emulators and flash carts may return
 * zeroes for this data, which is also fine. In the interest of compatibility
 * and accuracy, this data should be preserved, but its intended shape and
 * purpose is unknown.
 *
 * This is a low-level function that needs to be used in proper sequence.
 * For normal use, call the the high-level #rtc_init and #rtc_set functions
 * which handle the RTC control block writes, delays, and status checks.
 *
 * @param[out]  control
 *              Destination pointer for the RTC control register data.
 *
 * @param[out]  calibration
 *              Destination pointer for the RTC calibration data.
 */
void joybus_rtc_read_control( uint16_t * control, uint32_t * calibration );

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
 * @brief Write the control block to the Joybus real-time clock.
 *
 * Typically this would be set to one of two "modes":
 * Use #JOYBUS_RTC_CONTROL_MODE_SET before writing to block 2 to stop the RTC.
 * Use #JOYBUS_RTC_CONTROL_MODE_RUN after writing to block 2 to resume the RTC.
 *
 * It may take some time after the SI command responds for the write operation
 * to actually complete. It is recommended to add a delay after this function
 * using #JOYBUS_RTC_WRITE_BLOCK_DELAY to ensure the control block data was fully
 * written. After the delay, check the RTC status byte to confirm that it is
 * in the expected state.
 *
 * It is strongly recommended to propagate the calibration data returned from
 * #joybus_rtc_read_control into the calibration parameter of this function.
 * If you have any reason to modify or omit the calibration data, please
 * update this documentation with an explanation of why and how you would
 * want to do this.
 *
 * Generally, you should not need to call this function directly. Prefer
 * calling the high-level #rtc_init and #rtc_set functions which handle
 * the necessary delays, status checks, and calibration propagation.
 *
 * This is a low-level function that needs to be used in proper sequence.
 * For normal use, call the the high-level #rtc_init and #rtc_set functions
 * which handle the RTC control block writes, delays, and status checks.
 *
 * @param[in]  control
 *             The control register data
 *
 * @param[in]  calibration
 *             The opaque calibration data from #joybus_rtc_read_control
 */
void joybus_rtc_write_control( uint16_t control, uint32_t calibration );

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

bool joybus_rtc_set_time( time_t new_time );

#ifdef __cplusplus
}
#endif

/** @} */ /* rtc */

#endif
