#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "dd.h"
#include "debug.h"
#include "interrupt.h"
#include "dma.h"
#include "n64sys.h"
#include "rtc_utils.h"

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

time_t dd_get_time( void )
{
    // NOTE: the order of these commands is important, because reading MINSEC
    // is what actually triggers the handshake with the RTC. The other two
    // reads just fetch cached values.
    uint16_t ms = dd_command(DD_CMD_RTC_GET_MINSEC);
    uint16_t dh = dd_command(DD_CMD_RTC_GET_DAYHOUR);
    uint16_t ym = dd_command(DD_CMD_RTC_GET_YEARMONTH);

    debugf("dd_get_time: raw time: %02x-%02x-%02x %02x:%02x:%02x\n",
        ym >> 8, ym & 0xFF, dh >> 8, dh & 0xFF, ms >> 8, ms & 0xFF);

    // By convention, 2-digit year is interpreted as 20YY if it's 96 or later
    int year = bcd_decode( ym >> 8 );

    struct tm rtc_time = (struct tm){
        .tm_sec   = bcd_decode( ms & 0xFF ),
        .tm_min   = bcd_decode( ms >> 8 ),
        .tm_hour  = bcd_decode( dh & 0xFF ),
        .tm_mday  = bcd_decode( dh >> 8 ),
        .tm_mon   = bcd_decode (ym & 0xFF ) - 1,
        .tm_year  = year + (year >= 96 ? 0 : 100),
    };

    char buff[20];
    strftime(buff, 20, "%Y-%m-%d %H:%M:%S", &rtc_time);
    debugf("dd_get_time: parsed time: %s\n", buff);

    return mktime( &rtc_time );
}

bool dd_set_time( time_t new_time )
{
    struct tm * rtc_time = gmtime( &new_time );

    char buff[20];
    strftime(buff, 20, "%Y-%m-%d %H:%M:%S", rtc_time);
    debugf("dd_set_time: parsed time: %s\n", buff);

    uint8_t year = bcd_encode( rtc_time->tm_year % 100 );
    uint8_t month = bcd_encode( rtc_time->tm_mon + 1 );
    uint8_t day = bcd_encode( rtc_time->tm_mday );
    uint8_t hour = bcd_encode( rtc_time->tm_hour );
    uint8_t min = bcd_encode( rtc_time->tm_min );
    uint8_t sec = bcd_encode( rtc_time->tm_sec );

    debugf("dd_set_time raw time: %02x-%02x-%02x %02x:%02x:%02x\n",
        year, month, day, hour, min, sec);

    uint16_t ym_write = year << 8 | month;
    uint16_t dh_write = day << 8 | hour;
    uint16_t ms_write = min << 8 | sec;

    dd_write( DD_ASIC_DATA, ym_write );
    uint16_t ym_read = dd_command( DD_CMD_RTC_SET_YEARMONTH );

    dd_write( DD_ASIC_DATA, dh_write );
    uint16_t dh_read = dd_command( DD_CMD_RTC_SET_DAYHOUR );

    dd_write( DD_ASIC_DATA, ms_write );
    uint16_t ms_read = dd_command( DD_CMD_RTC_SET_MINSEC );

    debugf("dd_set_time: result: %04x %04x %04x\n", ym_read, dh_read, ms_read);

    return ym_write == ym_read && dh_write == dh_read && ms_write == ms_read;
}
