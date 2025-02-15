#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "dd.h"
#include "debug.h"
#include "interrupt.h"
#include "dma.h"
#include "n64sys.h"
#include "rtc_internal.h"

#define DD_ASIC_STATUS_MECHA_IRQ_LINE    (1<<9)
#define DD_ASIC_STATUS_BM_IRQ_LINE       (1<<10)

#define DD_ASIC_WCTRL_MECHA_IRQ_CLEAR    (1<<8)

bool dd_found = false;

static volatile int mecha_irq_count = 0;
static volatile int bm_irq_count = 0;

void dd_write(uint32_t address, uint16_t value) {
    io_write(address, value << 16);
}

uint16_t dd_read(uint32_t address) {
    return io_read(address) >> 16;
}

static void dd_handler(void)
{
    uint16_t status = dd_read(DD_ASIC_STATUS);

    if (status & DD_ASIC_STATUS_MECHA_IRQ_LINE) {
        // Acknowledge interrupt and record it was generated
        mecha_irq_count++;
        dd_write(DD_ASIC_WCTRL, DD_ASIC_WCTRL_MECHA_IRQ_CLEAR);
    }

    if (status & DD_ASIC_STATUS_BM_IRQ_LINE) {
        // This interrupt is auto-acknowledged when we read the ASIC status,
        // so just record it was generated
        bm_irq_count++;
    }
}

uint16_t dd_command(dd_cmd_t cmd) {
    int irq_count = mecha_irq_count;
    dd_write(DD_ASIC_WCMD, cmd);

    while (mecha_irq_count == irq_count) {}
    return dd_read(DD_ASIC_DATA);
}

__attribute__((constructor))
void dd_init(void)
{
    // iQue doesn't like PI accesses outside of ROM
    if (sys_bbplayer()) return;

    uint32_t magic = 0x36344444; // "64DD"
    dd_found = io_read(0x06000020) == magic;
    if (!dd_found) return;

    // Install the cart interrupt handler immediately, because the DD will generate
    // interrupts as soon as you interact with it, and we need to acknowledge
    // them to avoid stalling the CPU.
    set_CART_interrupt(1);
    register_CART_handler(dd_handler);
}

int dd_rtc_get_time( time_t *out )
{
    if( !dd_found )
    {
        return RTC_ENOCLOCK;
    }

    // NOTE: the order of these commands is important, because reading MINSEC
    // is what actually triggers the handshake with the RTC. The other two
    // reads just fetch cached values.
    uint16_t ms = dd_command(DD_CMD_RTC_GET_MINSEC);
    uint16_t dh = dd_command(DD_CMD_RTC_GET_DAYHOUR);
    uint16_t ym = dd_command(DD_CMD_RTC_GET_YEARMONTH);

    bool clock_error = ms >> 15;
    uint8_t sec_enc = ms & 0xFF;
    uint8_t min_enc = (ms >> 8) & 0x7F;
    uint8_t hour_enc = dh & 0xFF;
    uint8_t day_enc = dh >> 8;
    uint8_t month_enc = ym & 0xFF;
    uint8_t year_enc = ym >> 8;

    debugf("dd_rtc_get_time: raw ms:%04x dh:%04x ym:%04x\n", ms, dh, ym);

    if( clock_error )
    {
        debugf("dd_rtc_get_time: clock error\n");
        return RTC_EBADCLOCK;
    }

    int sec = bcd_decode( sec_enc );
    int min = bcd_decode( min_enc );
    int hour = bcd_decode( hour_enc );
    int day = bcd_decode( day_enc );
    int month = bcd_decode( month_enc );
    int year = bcd_decode( year_enc );

    // Extremely basic sanity-check on the date and time
    if(
        month == 0 || month > 12 ||
        day == 0 || day > 31 ||
        hour >= 24 || min >= 60 || sec >= 60
    )
    {
        debugf("dd_rtc_get_time: invalid date/time\n");
        return RTC_EBADTIME;
    }

    struct tm rtc_time = (struct tm){
        .tm_sec   = sec,
        .tm_min   = min,
        .tm_hour  = hour,
        .tm_mday  = day,
        .tm_mon   = month - 1,
        // By convention, 2-digit year is interpreted as 20YY if it's 96 or later
        .tm_year  = year + (year >= 96 ? 0 : 100),
    };

    debugf("dd_rtc_get_time: parsed time: %04d-%02d-%02d %02d:%02d:%02d\n",
        rtc_time.tm_year + 1900, rtc_time.tm_mon + 1, rtc_time.tm_mday,
        rtc_time.tm_hour, rtc_time.tm_min, rtc_time.tm_sec);

    *out = mktime( &rtc_time );
    return RTC_ESUCCESS;
}

int dd_rtc_set_time( time_t new_time )
{
    if( !dd_found )
    {
        return RTC_ENOCLOCK;
    }

    if( new_time < DD_RTC_TIMESTAMP_MIN || new_time > DD_RTC_TIMESTAMP_MAX )
    {
        debugf("dd_rtc_set_time: time out of range\n");
        return RTC_EBADTIME;
    }

    struct tm * rtc_time = gmtime( &new_time );

    debugf("dd_rtc_set_time: parsed time: %04d-%02d-%02d %02d:%02d:%02d\n",
        rtc_time->tm_year + 1900, rtc_time->tm_mon + 1, rtc_time->tm_mday,
        rtc_time->tm_hour, rtc_time->tm_min, rtc_time->tm_sec);

    uint8_t year = bcd_encode( rtc_time->tm_year % 100 );
    uint8_t month = bcd_encode( rtc_time->tm_mon + 1 );
    uint8_t day = bcd_encode( rtc_time->tm_mday );
    uint8_t hour = bcd_encode( rtc_time->tm_hour );
    uint8_t min = bcd_encode( rtc_time->tm_min );
    uint8_t sec = bcd_encode( rtc_time->tm_sec );

    uint16_t ym_write = year << 8 | month;
    uint16_t dh_write = day << 8 | hour;
    uint16_t ms_write = min << 8 | sec;

    debugf("dd_rtc_set_time: raw ms:%04x dh:%04x ym:%04x\n", ms_write, dh_write, ym_write);

    dd_write( DD_ASIC_DATA, ym_write );
    uint16_t ym_read = dd_command( DD_CMD_RTC_SET_YEARMONTH );

    dd_write( DD_ASIC_DATA, dh_write );
    uint16_t dh_read = dd_command( DD_CMD_RTC_SET_DAYHOUR );

    dd_write( DD_ASIC_DATA, ms_write );
    uint16_t ms_read = dd_command( DD_CMD_RTC_SET_MINSEC );

    debugf("dd_rtc_set_time: result ms:%04x dh:%04x ym:%04x\n", ms_read, dh_read, ym_read);

    bool clock_error = ms_read >> 15;
    if( clock_error )
    {
        return RTC_EBADCLOCK;
    }

    if( ym_write != ym_read || dh_write != dh_read || ms_write != ms_read )
    {
        return RTC_EBADTIME;
    }

    return RTC_ESUCCESS;
}
