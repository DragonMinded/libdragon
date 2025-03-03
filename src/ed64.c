/**
 * @file ed64.c
 * @brief EverDrive 64 V-series utilities
 * @ingroup peripherals
 */

#include "debug.h"
#include "dma.h"
#include "ed64.h"
#include "rtc_internal.h"
#include "utils.h"
 
/** @brief ED64 register to perform I2C bitbanging */
#define ED64_REG_I2C     0x08040030

/** @brief Init I2C communication (callback from i2c_internal.h) */
#define I2C_INIT()              ({ })
/** @brief Write I2C clock and data line (callback from i2c_internal.h) */
#define I2C_WRITE(clock, data)  ({ io_write(ED64_REG_I2C, ((clock)<<2) | ((data))); })
/** @brief Read I2C data line, while setting the clock (callback from i2c_internal.h) */
#define I2C_READ(clock)         ({ io_write(ED64_REG_I2C, ((clock)<<2) | 1); io_read(ED64_REG_I2C) & 1; })

#include "i2c_internal.h"

#define DS1337_DEVICE_ADDR      0x68            ///< DS1337 I2C device address

void ed64_rtc_encode(time_t new_time, uint8_t buf[7])
{
    struct tm * rtc_time = gmtime(&new_time);

    // Encoding according to DS1337 datasheet
    buf[0] = bcd_encode(rtc_time->tm_sec);
    buf[1] = bcd_encode(rtc_time->tm_min);
    buf[2] = bcd_encode(rtc_time->tm_hour);  // bit 6 toggles 12/24 hour mode
    buf[3] = bcd_encode(rtc_time->tm_wday);  // NOTE: this should be changed to 1-based, but ED64 OS has a bug, so we mimic it
    buf[4] = bcd_encode(rtc_time->tm_mday);
    buf[5] = bcd_encode(rtc_time->tm_mon + 1); // bit 7 is the century bit
    buf[6] = bcd_encode(rtc_time->tm_year % 100);
}

int ed64_rtc_write( uint8_t buf[7] )
{
    if (!i2c_write_data(DS1337_DEVICE_ADDR, 0, 7, buf))
        return -1;
    return 0;
}
