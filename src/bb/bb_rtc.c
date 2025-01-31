/**
 * @file bb_rtc.c
 * @author Giovanni Bajo (giovannibajo@gmail.com)
 * @brief iQue Player (BB) Real-Time Clock (RTC) driver.
 */
#include "dma.h"
#include <string.h>

#define PI_BB_GPIO      ((volatile uint32_t*)0xA4600060)            ///< BB GPIO register

#define I2C_DATA_OUT     (1<<7)     ///< Data input/output
#define I2C_CLOCK_OUT    (1<<6)     ///< Clock input/output
#define I2C_DATA_BIT     (1<<4)     ///< Data bit
#define I2C_CLOCK_BIT    (1<<3)     ///< Clock bit

/** @brief Write SCL/SDA I2C lines */
#define I2C_WRITE(clock, data)  ({ \
    *PI_BB_GPIO = I2C_DATA_OUT | I2C_CLOCK_OUT | ((data) ? I2C_DATA_BIT : 0) | ((clock) ? I2C_CLOCK_BIT : 0); \
})

/** @brief Read SDA I2C line */
#define I2C_READ(clock) ({ \
    *PI_BB_GPIO = I2C_CLOCK_OUT | ((clock) ? I2C_CLOCK_BIT : 0); \
    (*PI_BB_GPIO & I2C_DATA_BIT) ? 1 : 0; \
})

/** @brief Delay to wait the RTC chip to reply */
#define I2C_READ_DELAY   1000

#include "i2c_internal.h"

/** @brief RTC slave address on the I2C bus */
#define RTC_SLAVE_ADDR   0x68

/**
 * @brief RTC state
 * 
 * This structure contains a dump of the internal state of the RTC chip.
 */
typedef struct {
    uint8_t secs;           ///< Seconds [0-59]
    uint8_t mins;           ///< Minutes [0-59]
    uint8_t hours;          ///< Hours [0-23]
    uint8_t dow;            ///< Day of week [1-7] (Sun-Sat)
    uint8_t day;            ///< Day of month [1-31]
    uint8_t month;          ///< Month [1-12]
    uint8_t year;           ///< Year [0-99]

    bool stop;              ///< RTC is stopped
    bool oscillator_fail;   ///< Oscillator has failed
    bool century;           ///< Century bit
    bool century_enable;    ///< Century enable: automatic toggle of century bit
    bool output_level;      ///< Level of the OUT pin
} bb_rtc_state_t;

static int bcd_decode(uint8_t bcd)
{
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

/** @brief Read the internal state of the RTC chip */
bool bbrtc_get_state(bb_rtc_state_t *state)
{
    uint8_t data[8];
    if (!i2c_read_data(RTC_SLAVE_ADDR, 0, sizeof(data), data))
        return false;

    memset(state, 0, sizeof(bb_rtc_state_t));
    state->secs  = bcd_decode(data[0] & 0x7F);
    state->mins  = bcd_decode(data[1] & 0x7F);
    state->hours = bcd_decode(data[2] & 0x3F);
    state->dow   = bcd_decode(data[3] & 0x07);
    state->day   = bcd_decode(data[4] & 0x3F);
    state->month = bcd_decode(data[5] & 0x1F);
    state->year  = bcd_decode(data[6] & 0xFF);

    state->stop            = (data[0] & 0x80) ? true : false;
    state->oscillator_fail = (data[1] & 0x80) ? true : false;
    state->century         = (data[2] & 0x40) ? true : false;
    state->century_enable  = (data[2] & 0x80) ? true : false;
    state->output_level    = (data[7] & 0x80) ? true : false;
    return true;
}
