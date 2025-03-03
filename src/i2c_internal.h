#ifndef LIBDRAGON_I2C_INTERNAL_H
#define LIBDRAGON_I2C_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include "debug.h"
#include "n64sys.h"

// Simple implementation of 2-wire I2C protocol
// Before including this file, define the following macros:
// - I2C_INIT()           : Initialize a I2C transaction
// - I2C_WRITE(scl, sda)  : Write values to the SCL (clock) and SDA (data) lines
// - I2C_READ(scl)        : Read the value of the SDA (data) line, while setting the SCL (clock) line

#if !defined(I2C_INIT) || !defined(I2C_WRITE) || !defined(I2C_READ)
#error "I2C_WRITE, I2C_READ, and I2C_INIT must be defined before including i2c_internal.h"
#endif

static inline void i2c_write_start(void)
{
    I2C_WRITE(0, 1);
    I2C_WRITE(1, 1);
    I2C_WRITE(1, 0);
}

static inline void i2c_write_stop(void)
{
    I2C_WRITE(1, 0);
    I2C_WRITE(1, 1);
    I2C_WRITE(0, 1);
}

static inline void i2c_write_bit(bool data)
{
    I2C_WRITE(0, data);
    I2C_WRITE(1, data);
    I2C_WRITE(0, data);
}

static inline bool i2c_read_bit(void)
{
    I2C_WRITE(0, 1);
    I2C_WRITE(1, 1);
    bool data = I2C_READ(1);
    I2C_WRITE(0, 1);
    return data;
}

static inline bool i2c_read_ack(int ackid)
{
    I2C_WRITE(1, 1);
    bool ack = !I2C_READ(1);
    I2C_WRITE(0, 1);
    assertf(ack, "I2C ACK%d failed: expected 0, got 1", ackid);
    return ack;
}

static inline void i2c_write_ack(void)
{
    i2c_write_bit(0);
}

static inline void i2c_write_nack(void)
{
    i2c_write_bit(1);
}

static inline void i2c_write_byte(uint8_t data)
{
    for (int i=7; i>=0; i--) {
        i2c_write_bit((data >> i) & 1);
    }
}

static inline uint8_t i2c_read_byte(void)
{
    uint8_t data = 0;
    for (int i=7; i>=0; i--) {
        data |= (i2c_read_bit() << i);
    }
    return data;
}

__attribute__((used))
static bool i2c_read_data(uint8_t slave_addr, uint8_t addr, int len, uint8_t *data)
{
    bool ok = true;
    I2C_INIT();
    i2c_write_start();
    i2c_write_byte(slave_addr << 1);
    if (!i2c_read_ack(0)) { ok = false; goto finish; }
    i2c_write_byte(addr);
    if (!i2c_read_ack(1)) { ok = false; goto finish; }
    i2c_write_start();
    i2c_write_byte((slave_addr << 1) | 1);  // read mode
    if (!i2c_read_ack(2)) { ok = false; goto finish; }
    for (int i=0; i<len; i++) {
        data[i] = i2c_read_byte();
        if (i < len-1)
            i2c_write_ack();
        else
            i2c_write_nack();
    }
finish:
    i2c_write_stop();
    I2C_WRITE(1,1);
    return ok;
}

__attribute__((used))
static bool i2c_write_data(uint8_t slave_addr, uint8_t addr, int len, uint8_t *data)
{
    bool ok = true;
    I2C_INIT();
    i2c_write_start();
    i2c_write_byte(slave_addr << 1);
    if (!i2c_read_ack(0)) { ok = false; goto finish; }
    i2c_write_byte(addr);
    if (!i2c_read_ack(1)) { ok = false; goto finish; }
    for (int i=0; i<len; i++) {
        i2c_write_byte(data[i]);
        if (!i2c_read_ack(2)) { ok = false; goto finish; }
    }
finish:
    i2c_write_stop();
    I2C_WRITE(1,1);
    return ok;
}

#endif
