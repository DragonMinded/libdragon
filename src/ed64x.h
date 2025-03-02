/**
 * @file ed64x.h
 * @brief EverDrive 64 X-Series utilities.
 * @ingroup peripherals
 */

#ifndef __LIBDRAGON_ED64X_H
#define __LIBDRAGON_ED64X_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Write an encoded time to the EverDrive 64 X7 RTC.
 * 
 * This function writes an encoded time to the EverDrive 64 X7 RTC, using
 * direct I2C access via hardware registers. It can be used to change the time,
 * as ED64 X7 does not support using the joybus RTC write commands.
 * 
 * @param buf        The encoded time to write (see #ed64_rtc_encode).
 * @return int       0 on success, -1 on failure.
 * 
 * @see #ed64_rtc_encode
 * @see #ed64_rtc_write
 */
int ed64x_rtc_write( uint8_t buf[7] );

#ifdef __cplusplus
}
#endif

#endif
