#ifndef LIBDRAGON_DD_H
#define LIBDRAGON_DD_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

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

time_t dd_get_time( void );

bool dd_set_time( time_t new_time );

inline bool sys_dd(void) {
    return dd_found;
}

#ifdef __cplusplus
}
#endif

#endif // LIBDRAGON_DD_H

