#ifndef LIBDRAGON_DD_H
#define LIBDRAGON_DD_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

///@cond
extern bool dd_found;
///@endcond

#define DD_ASIC_BASE    0x05000500
#define DD_ASIC_DATA	(DD_ASIC_BASE + 0x0)
#define DD_ASIC_STATUS  (DD_ASIC_BASE + 0x8)
#define DD_ASIC_WCMD    (DD_ASIC_BASE + 0x8)
#define DD_ASIC_WCTRL   (DD_ASIC_BASE + 0x10)

void dd_write(uint32_t address, uint16_t value);
uint16_t dd_read(uint32_t address);

typedef enum {
	DD_CMD_CLEAR_RESET_FLAG  = 0x09,
	DD_CMD_RTC_SET_YEARMONTH = 0x0f,
	DD_CMD_RTC_SET_DAYHOUR   = 0x10,
	DD_CMD_RTC_SET_MINSEC    = 0x11,
	DD_CMD_RTC_GET_YEARMONTH = 0x12,
	DD_CMD_RTC_GET_DAYHOUR   = 0x13,
	DD_CMD_RTC_GET_MINSEC    = 0x14,
} dd_cmd_t;

uint16_t dd_command(dd_cmd_t cmd);

/** @brief 64DD RTC minimum timestamp (1996-01-01 00:00:00) */
#define DD_RTC_TIMESTAMP_MIN 820454400
/** @brief 64DD RTC maximum timestamp (2095-12-31 23:59:59) */
#define DD_RTC_TIMESTAMP_MAX 3976214399

/**
 * @brief Read the time from the 64DD real-time clock as a UNIX timestamp
 *
 * @param[out] out pointer to the output time_t
 *
 * @retval RTC_ESUCCESS if the operation was successful
 * @retval RTC_ENOCLOCK if the RTC is not available
 * @retval RTC_EBADCLOCK if the RTC is not operational
 * @retval RTC_EBADTIME if the RTC time is not representable
 */
int dd_rtc_get_time( time_t *out );

/**
 * @brief Set the date/time on the 64DD real-time clock.
 *
 * @param new_time the new RTC time as a UNIX timestamp
 *
 * @retval RTC_ESUCCESS if the operation was successful
 * @retval RTC_ENOCLOCK if the RTC is not available
 * @retval RTC_EBADCLOCK if the RTC is not operational
 * @retval RTC_EBADTIME if the RTC cannot represent the new time
 */
int dd_rtc_set_time( time_t new_time );

inline bool sys_dd(void) {
    return dd_found;
}

#ifdef __cplusplus
}
#endif

#endif // LIBDRAGON_DD_H

