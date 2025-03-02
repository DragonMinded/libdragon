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

 /**
  * @defgroup ed64x EverDrive 64 X-series
  * @ingroup peripherals
  * @brief EverDrive 64 X-series utilities.
  *
  * @{
  */

 #ifdef __cplusplus
 extern "C" {
 #endif

 int ed64x_rtc_write( time_t new_time );

 #ifdef __cplusplus
 }
 #endif

 /** @} */ /* ed64x */

 #endif
