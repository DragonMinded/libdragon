/**
 * @file bb_rtc.c
 * @author Giovanni Bajo (giovannibajo@gmail.com)
 * @brief iQue Player (BB) Real-Time Clock (RTC) driver.
 */

#include <string.h>
#include <time.h>
#include "bb_rtc.h"
#include "debug.h"
#include "dma.h"
#include "n64sys.h"
#include "rtc_internal.h"

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

/**
 * @brief Read the internal state of the BBPlayer RTC chip
 *
 * @param state pointer to BBPlayer RTC state struct or NULL
 * @param raw pointer to a uint64 to store the raw state or NULL
 *
 * @retval RTC_ESUCCESS if the operation was successful.
 * @retval RTC_EBADCLOCK if an I/O error occurred.
 **/
int bb_rtc_get_state( bb_rtc_state_t *state, uint64_t *raw )
{
    if( !sys_bbplayer() )
    {
        debugf("bb_rtc_get_state: BBPlayer not detected\n");
        return RTC_ENOCLOCK;
    }

    uint64_t dword;
    uint8_t *bytes = (uint8_t *)&dword;

    if ( !i2c_read_data( RTC_SLAVE_ADDR, 0, sizeof(dword), bytes ) )
    {
        debugf("bb_rtc_get_state: failed to read over i2c\n");
        return RTC_EBADCLOCK;
    }

    if( state != NULL )
    {
        memset( state, 0, sizeof(bb_rtc_state_t) );
        state->secs  = bcd_decode( bytes[0] & 0x7F );
        state->mins  = bcd_decode( bytes[1] & 0x7F );
        state->hours = bcd_decode( bytes[2] & 0x3F );
        state->dow   = bcd_decode( bytes[3] & 0x07 );
        state->day   = bcd_decode( bytes[4] & 0x3F );
        state->month = bcd_decode( bytes[5] & 0x1F );
        state->year  = bcd_decode( bytes[6] & 0xFF );

        state->stop            = (bytes[0] & 0x80) ? true : false;
        state->oscillator_fail = (bytes[1] & 0x80) ? true : false;
        state->century         = (bytes[2] & 0x40) ? true : false;
        state->century_enable  = (bytes[2] & 0x80) ? true : false;
        state->output_level    = (bytes[7] & 0x80) ? true : false;
    }

    if( raw != NULL )
    {
        *raw = dword;
    }

    return RTC_ESUCCESS;
}

/**
 * @brief Write the internal state of the BBPlayer RTC chip
 *
 * @param state pointer to BBPlayer RTC state struct
 *
 * @retval RTC_ESUCCESS if the operation was successful.
 * @retval RTC_EBADCLOCK if an I/O error occurred.
 **/
int bb_rtc_set_state( bb_rtc_state_t *state )
{
    if( !sys_bbplayer() )
    {
        debugf("bb_rtc_set_state: BBPlayer not detected\n");
        return RTC_ENOCLOCK;
    }

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

    if( i2c_write_data( RTC_SLAVE_ADDR, 0, sizeof(bytes), bytes ) )
    {
        debugf("bb_rtc_set_state: success");
        return RTC_ESUCCESS;
    }
    else
    {
        debugf("bb_rtc_set_state: failed to write over i2c\n");
        return RTC_EBADCLOCK;
    }
}

/**
 * @brief Enable or disable the BBPlayer RTC century bit
 *
 * @param enabled true to enable the century bit, false to disable
 *
 * @return RTC_ESUCCESS if the operation was successful,
 *         or RTC_EBADCLOCK if an I/O error occurred.
 */
int bb_rtc_set_century_enable( bool enabled )
{
    bb_rtc_state_t state;
    int error = bb_rtc_get_state( &state, NULL );
    if ( error != RTC_ESUCCESS )
    {
        debugf("bb_rtc_set_century_enable: failed to read state\n");
        return error;
    }
    state.century_enable = enabled;
    return bb_rtc_set_state(&state);
}

int bb_rtc_get_time( time_t *out )
{
    bb_rtc_state_t state;
    int error = bb_rtc_get_state( &state, NULL );
    if( error != RTC_ESUCCESS )
    {
        debugf("bb_rtc_get_time: failed to read state\n");
        return error;
    }

    if( state.oscillator_fail )
    {
        debugf("bb_rtc_get_time: oscillator failure reported\n");
        // Oscillator failures are not necessarily fatal.
        // It could mean that the battery is dead, or has been replaced.
        // TODO: Message this up to RTC subsystem so it can be reported/handled.
    }

    // Extremely basic sanity-check on the date and time
    if(
        state.month == 0 || state.month > 12 ||
        state.day == 0 || state.day > 31 ||
        state.hours >= 24 || state.mins >= 60 || state.secs >= 60
    )
    {
        debugf("bb_rtc_get_time: invalid date/time\n");
        return RTC_EBADTIME;
    }

    // NOTE: Official iQue menu disables the century bit!
    bool century = state.century_enable && state.century;

    struct tm rtc_tm;
    rtc_tm.tm_sec   = state.secs;
    rtc_tm.tm_min   = state.mins;
    rtc_tm.tm_hour  = state.hours;
    rtc_tm.tm_wday  = state.dow;
    rtc_tm.tm_mday  = state.day;
    rtc_tm.tm_mon   = state.month - 1;
    // BBPlayer was released in 2003; does not support 19XX year
    rtc_tm.tm_year  = state.year + (century ? 200 : 100);

    debugf("bb_rtc_get_time: parsed time: %04d-%02d-%02d %02d:%02d:%02d\n",
        rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday,
        rtc_tm.tm_hour, rtc_tm.tm_min, rtc_tm.tm_sec);

    *out = mktime( &rtc_tm );
    return RTC_ESUCCESS;
}

int bb_rtc_set_time( time_t new_time )
{
    bb_rtc_state_t state;
    int error = bb_rtc_get_state( &state, NULL );
    if( error != RTC_ESUCCESS )
    {
        debugf("bb_rtc_set_time: failed to read state\n");
        return error;
    }

    if( state.oscillator_fail )
    {
        debugf("bb_rtc_set_time: resetting oscillator fail bit\n");
        // Oscillator failures are not necessarily fatal.
        // It could mean that the battery is dead, or has been replaced.
        // If the bit is set again on the next read, it's a real problem.
        state.oscillator_fail = false;
    }

    // NOTE: Official iQue menu disables the century bit!
    time_t max_time = state.century_enable ? BB_RTC_CENTURY_TIMESTAMP_MAX : BB_RTC_TIMESTAMP_MAX;
    if( new_time < BB_RTC_TIMESTAMP_MIN || new_time > max_time )
    {
        debugf("bb_rtc_set_time: time out of range\n");
        return RTC_EBADTIME;
    }

    struct tm * new_tm = gmtime( &new_time );

    debugf("bb_rtc_set_time: parsed time: %04d-%02d-%02d %02d:%02d:%02d\n",
        new_tm->tm_year + 1900, new_tm->tm_mon + 1, new_tm->tm_mday,
        new_tm->tm_hour, new_tm->tm_min, new_tm->tm_sec);

    state.stop = false;
    state.secs = new_tm->tm_sec;
    state.mins = new_tm->tm_min;
    state.hours = new_tm->tm_hour;
    state.dow = new_tm->tm_wday;
    state.day = new_tm->tm_mday;
    state.month = new_tm->tm_mon + 1;
    // BBPlayer was released in 2003; does not support 19XX year
    state.year = new_tm->tm_year % 100;
    // NOTE: Official iQue menu disables the century bit!
    if( state.century_enable )
    {
        state.century = new_tm->tm_year >= 200;
    }

    return bb_rtc_set_state( &state );
}
