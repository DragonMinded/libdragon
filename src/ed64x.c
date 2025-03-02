/**
 * @file ed64x.c
 * @brief EverDrive 64 X-series utilities
 * @ingroup peripherals
 */

#include "debug.h"
#include "dma.h"
#include "ed64x.h"
#include "rtc_internal.h"
#include "utils.h"

#define DS1337_DEVICE_ADDR      0x68            ///< DS1337 I2C device address

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

__attribute__((used))
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

__attribute__((used))
static void ed64x_i2c_setwr(void)
{
    io_write(ED64X_REG_I2C_CMD, io_read(ED64X_REG_I2C_CMD) | 0x11);
}

__attribute__((used))
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

int ed64x_rtc_write(uint8_t buf[7])
{
    return ed64x_i2c_write((DS1337_DEVICE_ADDR << 9) + 0, buf, 7);
}
