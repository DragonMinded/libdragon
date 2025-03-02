/**
 * @file ed64x.c
 * @brief EverDrive 64 X-series utilities
 */

#include "debug.h"
#include "dma.h"
#include "ed64x.h"
#include "rtc_internal.h"
#include "utils.h"

/**
 * @addtogroup ed64x
 * @{
 */

static const uint32_t ED64X_REG_I2C_CMD = 0x1F800018;
static const uint32_t ED64X_REG_I2C_DAT = 0x1F80001C;

static int ed64x_i2c_status(void)
{
    return io_read(ED64X_REG_I2C_CMD) & 1;
}

static int ed64x_i2c_cmd(uint8_t cmd)
{
    io_write(ED64X_REG_I2C_DAT, cmd);
    while (io_read(ED64X_REG_I2C_CMD) & 0x80) {}
    return ed64x_i2c_status();
}

static uint8_t ed64x_i2c_dat(uint8_t cmd)
{
    io_write(ED64X_REG_I2C_DAT, cmd);
    while (io_read(ED64X_REG_I2C_CMD) & 0x80) {}
    return io_read(ED64X_REG_I2C_DAT);
}

static void ed64x_i2c_start(void)
{
    uint8_t val = io_read(ED64X_REG_I2C_CMD);
    io_write(ED64X_REG_I2C_CMD, 0x20);
    io_write(ED64X_REG_I2C_DAT, 0xFF);
    while (io_read(ED64X_REG_I2C_CMD) & 0x80) {}
    io_write(ED64X_REG_I2C_CMD, val | 0x11); // set write mode
}

static void ed64x_i2c_end(void)
{
    io_write(ED64X_REG_I2C_CMD, 0x30);
    io_write(ED64X_REG_I2C_DAT, 0xFF);
    while (io_read(ED64X_REG_I2C_CMD) & 0x80) {}
}

static void ed64x_i2c_setwr(void)
{
    io_write(ED64X_REG_I2C_CMD, io_read(ED64X_REG_I2C_CMD) | 0x11);
}

static void ed64x_i2c_setrd(void)
{
    io_write(ED64X_REG_I2C_CMD, io_read(ED64X_REG_I2C_CMD) | 0x10);
}

static int ed64x_i2c_write(uint16_t addr, const uint8_t* data, int len)
{
    uint8_t bus_addr = addr >> 8;
    uint8_t dev_addr = addr & 0xFF;

    for (int i=0; i<len; i+=8) {
        int retry = 0;
        while (1) {
            ed64x_i2c_start();
            if (ed64x_i2c_cmd(bus_addr) == 0)
                break;
            ed64x_i2c_end();
            if (++retry == 16)
                return -1;
        }
        ed64x_i2c_cmd(dev_addr+i);
        for (int j=0; j<MIN(len, 8); j++) {
            io_write(ED64X_REG_I2C_DAT, data[i+j]);
            while (io_read(ED64X_REG_I2C_CMD) & 0x80) {}
        }
        ed64x_i2c_end();
    }
    return 0;
}

int ed64x_rtc_write( time_t new_time )
{
    struct tm * rtc_time = gmtime( &new_time );
    uint8_t buf[7];

    // The RTC is a DS1337. Encode time according to its datasheet
    enum { DS1337_BUS_ADDR = 0xD000 };

    buf[0] = bcd_encode( rtc_time->tm_sec );
    buf[1] = bcd_encode( rtc_time->tm_min );
    buf[2] = bcd_encode( rtc_time->tm_hour );  // bit 6 toggles 12/24 hour mode
    buf[3] = bcd_encode( rtc_time->tm_wday + 1 );
    buf[4] = bcd_encode( rtc_time->tm_mday );
    buf[5] = bcd_encode( rtc_time->tm_mon + 1 ); // bit 7 is the century bit
    buf[6] = bcd_encode( rtc_time->tm_year % 100 );
    return ed64x_i2c_write( DS1337_BUS_ADDR + 0, buf, sizeof(buf) );
}

 /** @} */ /* ed64x */
