/**
 * @file dd.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @author Christopher Bonhage <me@christopherbonhage.com>
 */
#ifndef LIBDRAGON_DD_H
#define LIBDRAGON_DD_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "preview.h"

#ifdef __cplusplus
extern "C" {
#endif

///@cond
extern bool dd_found;
///@endcond

/** @brief Base address of the 64DD ASIC */
#define DD_ASIC_BASE    0x05000500
/** @brief Data register address for 64DD ASIC */
#define DD_ASIC_DATA	(DD_ASIC_BASE + 0x0)
/** @brief Status register address for 64DD ASIC */
#define DD_ASIC_STATUS  (DD_ASIC_BASE + 0x8)
/** @brief Write command register address for 64DD ASIC */
#define DD_ASIC_WCMD    (DD_ASIC_BASE + 0x8)
/** @brief Write control register address for 64DD ASIC */
#define DD_ASIC_WCTRL   (DD_ASIC_BASE + 0x10)

/** @brief Writes a 16-bit value to the 64DD ASIC
 * @preview
 */
LIBDRAGON_PREVIEW_API
void dd_write(uint32_t address, uint16_t value);

/** @brief Reads a 16-bit value from the 64DD ASIC
 * @preview
 */
LIBDRAGON_PREVIEW_API
uint16_t dd_read(uint32_t address);

/** @brief 64DD command codes */
typedef enum {
	DD_CMD_CLEAR_RESET_FLAG  = 0x09, ///< Clear reset flag command
	DD_CMD_RTC_SET_YEARMONTH = 0x0f, ///< Set RTC year/month command
	DD_CMD_RTC_SET_DAYHOUR   = 0x10, ///< Set RTC day/hour command
	DD_CMD_RTC_SET_MINSEC    = 0x11, ///< Set RTC minute/second command
	DD_CMD_RTC_GET_YEARMONTH = 0x12, ///< Get RTC year/month command
	DD_CMD_RTC_GET_DAYHOUR   = 0x13, ///< Get RTC day/hour command
	DD_CMD_RTC_GET_MINSEC    = 0x14, ///< Get RTC minute/second command
} dd_cmd_t;

/** @brief Sends a command to the 64DD ASIC
 * @preview
 */
LIBDRAGON_PREVIEW_API
uint16_t dd_command(dd_cmd_t cmd);

/** @brief 64DD RTC minimum timestamp (1996-01-01 00:00:00) */
#define DD_RTC_TIMESTAMP_MIN 820454400
/** @brief 64DD RTC maximum timestamp (2095-12-31 23:59:59) */
#define DD_RTC_TIMESTAMP_MAX 3976214399

/**
 * @brief Read the time from the 64DD real-time clock as a UNIX timestamp
 * @preview
 *
 * @param[out] out pointer to the output time_t
 *
 * @retval RTC_ESUCCESS if the operation was successful
 * @retval RTC_ENOCLOCK if the RTC is not available
 * @retval RTC_EBADCLOCK if the RTC is not operational
 * @retval RTC_EBADTIME if the RTC time is not representable
 */
LIBDRAGON_PREVIEW_API
int dd_rtc_get_time( time_t *out );

/**
 * @brief Set the date/time on the 64DD real-time clock.
 * @preview
 *
 * @param new_time the new RTC time as a UNIX timestamp
 *
 * @retval RTC_ESUCCESS if the operation was successful
 * @retval RTC_ENOCLOCK if the RTC is not available
 * @retval RTC_EBADCLOCK if the RTC is not operational
 * @retval RTC_EBADTIME if the RTC cannot represent the new time
 */
LIBDRAGON_PREVIEW_API
int dd_rtc_set_time( time_t new_time );

/** @brief Checks if 64DD hardware is present
 * @preview
 */
LIBDRAGON_PREVIEW_API
inline bool sys_dd(void) {
    return dd_found;
}

#ifdef __cplusplus
}
#endif

#endif // LIBDRAGON_DD_H

