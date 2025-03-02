/**
 * @file ed64.h
 * @brief EverDrive 64 V-series utilities.
 * @ingroup peripherals
 */

 #ifndef __LIBDRAGON_ED64_H
 #define __LIBDRAGON_ED64_H

 #include <stdbool.h>
 #include <stdint.h>
 #include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encode a time into the raw RTC format.
 * 
 * This function encodes a time into the raw format used by the Analog Devices
 * DS1337 RTC, which is used by the EverDrive 64. See the datasheet for
 * more information.
 * 
 * @param time      The time to encode.
 * @param buf       The buffer to store the encoded time.
 */
void ed64_rtc_encode( time_t time, uint8_t buf[7] );

/**
 * @brief Write an encoded time to the EverDrive 64 V3 RTC.
 * 
 * This function writes an encoded time to the EverDrive 64 V3 RTC, using
 * direct I2C access via hardware registers. It can be used to change the time,
 * as ED64 V3 does not support using the joybus RTC write commands.
 * 
 * @param buf        The encoded time to write (see #ed64_rtc_encode).
 * @return int       0 on success, -1 on failure.
 * 
 * @see #ed64_rtc_encode
 * @see #ed64x_rtc_write
 */
int ed64_rtc_write( uint8_t buf[7] );

#ifdef __cplusplus
}
#endif

#endif
