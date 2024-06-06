/**
 * @file dd_rtc.c
 * @brief 64DD Real-Time Clock Utilities
 * @ingroup rtc
 */

#include "dd_rtc.h"
#include "debug.h"
#include "dma.h"
#include "n64sys.h"
#include "rtc_utils.h"

/**
 * @addtogroup rtc
 * @{
 */

// MARK: Constants

#define DD_REGS_BASE_ADDR 0x05000500

#define UNMAPPED_ADDR(addr) ((addr & 0xFFFF) | ((addr & 0xFFFF) << 16))

typedef enum
{
    DD_ASIC_DATA = 0,
    DD_ASIC_MISC_REG = 1,
    DD_ASIC_CMD_STATUS = 2,
} dd_reg_t;

#define DD_REG_ADDR(reg) (DD_REGS_BASE_ADDR + (reg << 2))

#define DD_READ(reg) (io_read( DD_REG_ADDR(reg) ))
#define DD_WRITE(reg, data) (io_write( DD_REG_ADDR(reg), data ))

typedef enum
{
    DD_CMD_NOOP              = 0x00,
    DD_CMD_SEEK_READ         = 0x01,
    DD_CMD_SEEK_WRITE        = 0x02,
    DD_CMD_RECALIBRATE       = 0x03,
    DD_CMD_SLEEP             = 0x04,
    DD_CMD_START             = 0x05,
    DD_CMD_SET_STANDBY       = 0x06,
    DD_CMD_SET_SLEEP         = 0x07,
    DD_CMD_CLR_CHANGE        = 0x08,
    DD_CMD_CLR_RESET         = 0x09,
    DD_CMD_READ_ASIC_VERSION = 0x0a,
    DD_CMD_SET_DISK_TYPE     = 0x0b,
    DD_CMD_REQUEST_STATUS    = 0x0c,
    DD_CMD_STANDBY           = 0x0d,
    DD_CMD_IDX_LOCK_RETRY    = 0x0e,
    DD_CMD_SET_YEAR_MONTH    = 0x0f,
    DD_CMD_SET_DAY_HOUR      = 0x10,
    DD_CMD_SET_MIN_SEC       = 0x11,
    DD_CMD_GET_YEAR_MONTH    = 0x12,
    DD_CMD_GET_DAY_HOUR      = 0x13,
    DD_CMD_GET_MIN_SEC       = 0x14,
    DD_CMD_SET_LED_BLINK     = 0x15,
    DD_CMD_READ_PGM_VERSION  = 0x1b,
} dd_cmd_t;

#define DD_CMD(cmd) (DD_WRITE( DD_ASIC_CMD_STATUS, cmd << 16 ))

static void dd_rtc_init_sc64( void )
{
    uint32_t identifier, status;
    io_write( 0x1FFF0010, 0x5F554E4C );
    io_write( 0x1FFF0010, 0x4F434B5F );
    identifier = io_read( 0x1FFF000C );
    if( identifier == 0x53437632 )
    {
        debugf("Detected SC64!\n");
        io_write( 0x1FFF0004, 3 );
        io_write( 0x1FFF0008, 1 );
        io_write( 0x1FFF0000, 'C' );
        do
        {
            status = io_read( 0x1FFF0000 );
        }
        while( status & 0x80000000 );
        debugf("Enabled SC64 DD registers!\n");
    }
}

// MARK: Public functions

bool dd_rtc_detect( void )
{
    dd_rtc_init_sc64();
    uint32_t unmapped = UNMAPPED_ADDR( DD_REG_ADDR( DD_ASIC_DATA ) );
    if( DD_READ( DD_ASIC_DATA ) == unmapped)
    {
        debugf("DD memory is not mapped!\n");
        return false;
    }
    // Assumption: If DD memory is mapped, a DD is attached.
    // TODO: We probably want to make sure it's actually a DD with
    //       with a working RTC, though...
    return true;
}

time_t dd_rtc_get_time( void )
{
    uint32_t data;
    unsigned int year, month, day, hour, min, sec;

    DD_CMD( DD_CMD_GET_MIN_SEC );
    data = DD_READ( DD_ASIC_DATA );
    debugf("DD read minute/second: %08lX\n", data);

    min = bcd_to_byte((data >> 24) & 0xFF);
    sec = bcd_to_byte((data >> 16) & 0xFF);

    DD_CMD( DD_CMD_GET_DAY_HOUR );
    data = DD_READ( DD_ASIC_DATA );
    debugf("DD read day/hour: %08lX\n", data);

    day = bcd_to_byte((data >> 24) & 0xFF);
    hour = bcd_to_byte((data >> 16) & 0xFF);

    DD_CMD( DD_CMD_GET_YEAR_MONTH );
    data = DD_READ( DD_ASIC_DATA );
    debugf("DD read year/month: %08lX\n", data);

    year = bcd_to_byte((data >> 24) & 0xFF);
    month = bcd_to_byte((data >> 16) & 0xFF);

    struct tm t = {
        .tm_year = year + (year >= 96 ? 0 : 100),
        .tm_mon = month,
        .tm_mday = day,
        .tm_hour = hour,
        .tm_min = min,
        .tm_sec = sec,
    };

    debugf("\n");
    debugf("Read DD RTC time:\n");
    debugf("  Year: %d\n", t.tm_year);
    debugf("  Month: %d\n", t.tm_mon);
    debugf("  Day: %d\n", t.tm_mday);
    debugf("  Hour: %d\n", t.tm_hour);
    debugf("  Minute: %d\n", t.tm_min);
    debugf("  Second: %d\n", t.tm_sec);
    debugf("\n");

    return mktime( &t );
}

void dd_rtc_set_time( time_t time )
{
    struct tm * t = gmtime( &time );
    uint32_t data;

    debugf("\n");
    debugf("Writing DD RTC time:\n");
    debugf("  Year: %d\n", t->tm_year);
    debugf("  Month: %d\n", t->tm_mon);
    debugf("  Day: %d\n", t->tm_mday);
    debugf("  Hour: %d\n", t->tm_hour);
    debugf("  Minute: %d\n", t->tm_min);
    debugf("  Second: %d\n", t->tm_sec);
    debugf("\n");

    data = byte_to_bcd(t->tm_year % 100) << 24;
    data |= byte_to_bcd(t->tm_mon) << 16;

    DD_WRITE( DD_ASIC_DATA, data );
    DD_CMD( DD_CMD_SET_YEAR_MONTH );
    debugf("DD write year/month: %08lX\n", data);

    data = byte_to_bcd(t->tm_mday) << 24;
    data |= byte_to_bcd(t->tm_hour) << 16;

    DD_WRITE( DD_ASIC_DATA, data );
    DD_CMD( DD_CMD_SET_DAY_HOUR );
    debugf("DD write day/hour: %08lX\n", data);

    data = byte_to_bcd(t->tm_min) << 24;
    data |= byte_to_bcd(t->tm_sec) << 16;

    DD_WRITE( DD_ASIC_DATA, data );
    DD_CMD( DD_CMD_SET_MIN_SEC );
    debugf("DD write min/sec: %08lX\n", data);
}
