/**
 * @file bb_rtc.c
 * @author Giovanni Bajo (giovannibajo@gmail.com)
 * @brief iQue Player (BB) Real-Time Clock (RTC) driver.
 */

#include "debug.h"
#include "dma.h"
#include "n64sys.h"
#include <string.h>
#include <time.h>

#define PI_BB_GPIO      ((volatile uint32_t*)0xA4600060)            ///< BB GPIO register

#define I2C_DATA_OUT     (1<<7)     ///< Data input/output
#define I2C_CLOCK_OUT    (1<<6)     ///< Clock input/output
#define I2C_DATA_BIT     (1<<3)     ///< Data bit
#define I2C_CLOCK_BIT    (1<<2)     ///< Clock bit

static uint32_t gpio_cache;

/** @brief Convert nanoseconds to BBPlayer CPU ticks. */
#define BB_TICKS_FROM_NS(ns)  ((int64_t)(ns) * (144000000 / 2) / 1000000000)

static inline void wait_ns( unsigned long wait_ns )
{
    wait_ticks(BB_TICKS_FROM_NS(wait_ns));
}

/** @brief Initialize a I2C transaction */
#define I2C_INIT()  ({ \
    gpio_cache = *PI_BB_GPIO & ~(I2C_DATA_OUT | I2C_CLOCK_OUT | I2C_DATA_BIT | I2C_CLOCK_BIT); \
})

/** @brief Write SCL/SDA I2C lines */
#define I2C_WRITE(clock, data)  ({ \
    *PI_BB_GPIO = gpio_cache | (!(data) ? I2C_DATA_OUT : 0) | (!(clock) ? I2C_CLOCK_OUT : 0); \
    wait_ns((clock) ? 600 : 1300); \
})

/** @brief Read SDA I2C line */
#define I2C_READ(clock) ({ \
    *PI_BB_GPIO = gpio_cache | (!(clock) ? I2C_CLOCK_OUT : 0); \
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
typedef struct bb_rtc_state {
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

static int bcd_encode(int dec)
{
    return ((dec / 10) << 4) | (dec % 10);
}

/** @brief Read the internal state of the BBPlayer RTC chip */
uint64_t bb_rtc_get_state(bb_rtc_state_t *state)
{
    uint64_t dword;
    uint8_t *bytes = (uint8_t*)&dword;

    if (!i2c_read_data(RTC_SLAVE_ADDR, 0, sizeof(dword), bytes))
    {
        debugf("bb_rtc_get_state: failed to read over i2c\n");
        return 0;
    }

    debugf("bb_rtc_get_state: raw (0x%llx)\n", dword);

    if( state != NULL )
    {
        memset(state, 0, sizeof(bb_rtc_state_t));
        state->secs  = bcd_decode(bytes[0] & 0x7F);
        state->mins  = bcd_decode(bytes[1] & 0x7F);
        state->hours = bcd_decode(bytes[2] & 0x3F);
        state->dow   = bcd_decode(bytes[3] & 0x07);
        state->day   = bcd_decode(bytes[4] & 0x3F);
        state->month = bcd_decode(bytes[5] & 0x1F);
        state->year  = bcd_decode(bytes[6] & 0xFF);

        state->stop            = (bytes[0] & 0x80) ? true : false;
        state->oscillator_fail = (bytes[1] & 0x80) ? true : false;
        state->century         = (bytes[2] & 0x40) ? true : false;
        state->century_enable  = (bytes[2] & 0x80) ? true : false;
        state->output_level    = (bytes[7] & 0x80) ? true : false;
    }

    return dword;
}

/** @brief Write the internal state of the BBPlayer RTC chip */
bool bb_rtc_set_state(bb_rtc_state_t *state)
{
    uint8_t bytes[8];

    bytes[0] = bcd_encode(state->secs);
    bytes[1] = bcd_encode(state->mins);
    bytes[2] = bcd_encode(state->hours);
    bytes[3] = bcd_encode(state->dow);
    bytes[4] = bcd_encode(state->day);
    bytes[5] = bcd_encode(state->month);
    bytes[6] = bcd_encode(state->year);

    bytes[0] |= state->stop            ? 0x80 : 0x00;
    bytes[1] |= state->oscillator_fail ? 0x80 : 0x00;
    bytes[2] |= state->century         ? 0x40 : 0x00;
    bytes[2] |= state->century_enable  ? 0x80 : 0x00;
    bytes[7] |= state->output_level    ? 0x80 : 0x00;

    return i2c_write_data(RTC_SLAVE_ADDR, 0, sizeof(bytes), bytes);
}

time_t bb_rtc_get_time( void )
{
    bb_rtc_state_t state;
    if (!bb_rtc_get_state(&state))
    {
        debugf("bb_rtc_get_time: failed to read state\n");
        return 0;
    }

    struct tm rtc_tm;
    rtc_tm.tm_sec   = state.secs;
    rtc_tm.tm_min   = state.mins;
    rtc_tm.tm_hour  = state.hours;
    rtc_tm.tm_wday  = state.dow;
    rtc_tm.tm_mday  = state.day;
    rtc_tm.tm_mon   = state.month - 1;
    // TODO: Handle BBPlayer "century" flag
    rtc_tm.tm_year  = state.year + 100;

    char buff[20];
    strftime(buff, 20, "%Y-%m-%d %H:%M:%S", &rtc_tm);
    debugf("bb_rtc_get_time: parsed time: %s\n", buff);

    return mktime( &rtc_tm );
}

bool bb_rtc_set_time( time_t new_time )
{
    bb_rtc_state_t state;
    if (!bb_rtc_get_state(&state))
    {
        debugf("bb_rtc_set_time: failed to read state\n");
        return 0;
    }

    struct tm * new_tm = gmtime( &new_time );

    state.secs = new_tm->tm_sec;
    state.mins = new_tm->tm_min;
    state.hours = new_tm->tm_hour;
    state.dow = new_tm->tm_wday;
    state.day = new_tm->tm_mday;
    state.month = new_tm->tm_mon + 1;
    // TODO: Handle BBPlayer "century" flag
    state.year = new_tm->tm_year - 100;

    return bb_rtc_set_state( &state );
}
