/**
 * @file bio_sensor.c
 * @author Christopher Bonhage <me@christopherbonhage.com>
 * @brief Bio Sensor Subsystem
 * @ingroup bio_sensor
 */

#include <string.h>
#include <libdragon.h>

#include "bio_sensor.h"
#include "joybus_commands.h"
#include "joybus_accessory_internal.h"

/**
 * @addtogroup bio_sensor
 * @{
 */

/** @brief Minimum number of measurement periods required before calculating BPM */
#define BIO_SENSOR_PERIODS_MINIMUM 8
/** @brief Maximum number of measurement periods stored in the rolling window */
#define BIO_SENSOR_PERIODS_MAXIMUM 16
/** @brief Timer ticks per measurement period (500ms or half-second intervals) */
#define BIO_SENSOR_PERIOD_INTERVAL_TICKS (TICKS_PER_SECOND / 2)
/** @brief Number of measurement periods in one minute (120 half-second periods) */
#define BIO_SENSOR_PERIODS_PER_MINUTE (60 * 2)
/** @brief Convenience macro for joypad accessory type comparison */
#define BIO_SENSOR_CONNECTED(port) \
    (joypad_get_accessory_type(port) == JOYPAD_ACCESSORY_TYPE_BIO_SENSOR)

/**
 * @brief Bio Sensor reading states
 *
 * Tracks the current state of heartbeat detection from the Bio Sensor.
 */
typedef enum
{
    /** @brief Sensor reading is stopped or not initialized */
    BIO_SENSOR_STATE_STOPPED = 0,
    /** @brief Heart is between beats (sensor value: 0x03) */
    BIO_SENSOR_STATE_RESTING,
    /** @brief Heart is actively beating (sensor value: 0x00) */
    BIO_SENSOR_STATE_PULSING,
} bio_sensor_state_t;

/**
 * @brief Bio Sensor reader context structure
 *
 * Maintains state and statistics for heartbeat detection on a single port.
 * Uses a rolling window to track beats across multiple measurement periods.
 */
typedef struct
{
    /** @brief Flag indicating an async read operation is in progress */
    bool read_pending;
    /** @brief Current heartbeat detection state */
    bio_sensor_state_t state;
    /** @brief Timer tick value when the current measurement period started */
    int64_t period_start_ticks;
    /** @brief Number of heartbeats detected in the current measurement period */
    unsigned period_beats;
    /** @brief Current write position in the circular buffer of beat counts */
    unsigned period_cursor;
    /** @brief Total number of completed measurement periods since reading started */
    unsigned period_counter;
    /** @brief Circular buffer storing heartbeat counts for each measurement period */
    unsigned beats_per_period[BIO_SENSOR_PERIODS_MAXIMUM];
} bio_sensor_reader_t;

/** @brief Reference count tracking #bio_sensor_init vs #bio_sensor_close calls */
static int bio_sensor_init_refcount = 0;

/** @brief Array of Bio Sensor reader contexts, one per controller port */
static volatile bio_sensor_reader_t bio_sensor_readers[JOYPAD_PORT_COUNT] = {0};

/**
 * @brief Callback function for asynchronous Bio Sensor data reads
 *
 * Processes heartbeat data from the Bio Sensor accessory. Detects state transitions
 * from PULSING to RESTING to count heartbeats. Maintains a rolling window of beat
 * counts across multiple measurement periods for BPM calculation.
 *
 * @param out_dwords Pointer to the Joybus response data containing sensor readings
 * @param ctx        Context pointer containing the joypad port number
 */
static void bio_sensor_read_callback(uint64_t *out_dwords, void *ctx)
{
    const uint8_t *out_bytes = (void *)out_dwords;
    joypad_port_t port = (joypad_port_t)ctx;

    // Extract the "N64 Accessory Read" command struct from the Joybus response
    const joybus_cmd_n64_accessory_read_port_t *cmd =
        (void *)&out_bytes[port + JOYBUS_COMMAND_METADATA_SIZE];
    assert(cmd->send.command == JOYBUS_COMMAND_ID_N64_ACCESSORY_READ);

    volatile bio_sensor_reader_t *reader = &bio_sensor_readers[port];
    // Ignore this read if this sensor has been stopped
    if (reader->state == BIO_SENSOR_STATE_STOPPED) { return; }

    int crc_status = joybus_accessory_compare_data_crc(cmd->recv.data, cmd->recv.data_crc);
    if (crc_status != JOYBUS_ACCESSORY_IO_STATUS_OK)
    {
        // Stop reading if the Bio Sensor has been disconnected
        if (crc_status == JOYBUS_ACCESSORY_IO_STATUS_NO_PAK)
        {
            bio_sensor_read_stop(port);
        }
        // Skip this read if it fails the CRC check
        reader->read_pending = false;
        return;
    }

    int64_t now_ticks = timer_ticks();
    if (reader->period_start_ticks + BIO_SENSOR_PERIOD_INTERVAL_TICKS < now_ticks)
    {
        unsigned cursor = reader->period_cursor;
        reader->beats_per_period[cursor++] = reader->period_beats;
        if (cursor >= BIO_SENSOR_PERIODS_MAXIMUM) cursor = 0;
        reader->period_cursor = cursor;
        reader->period_beats = 0;
        reader->period_counter++;
        reader->period_start_ticks = now_ticks;
    }

    uint8_t sensor_data = cmd->recv.data[0];
    bio_sensor_state_t next_state = BIO_SENSOR_STATE_STOPPED;
    if (sensor_data == 0x00) next_state = BIO_SENSOR_STATE_PULSING;
    if (sensor_data == 0x03) next_state = BIO_SENSOR_STATE_RESTING;

    if (
        reader->state == BIO_SENSOR_STATE_PULSING &&
        next_state == BIO_SENSOR_STATE_RESTING
    ) {
        reader->period_beats += 1;
    }
    reader->state = next_state;
    reader->read_pending = false;
}

/**
 * @brief VI interrupt handler for periodic Bio Sensor reads
 *
 * Called on every vertical interrupt (60Hz NTSC / 50Hz PAL) to initiate
 * asynchronous reads from all active Bio Sensor accessories. Ensures
 * continuous monitoring of heartbeat data across all controller ports.
 */
static void bio_sensor_vi_interrupt_callback(void)
{
    JOYPAD_PORT_FOREACH (port)
    {
        if (
            bio_sensor_readers[port].read_pending == false &&
            bio_sensor_readers[port].state != BIO_SENSOR_STATE_STOPPED
        )
        {
            if (!BIO_SENSOR_CONNECTED(port))
            {
                // Stop reading if the Bio Sensor has been disconnected
                bio_sensor_read_stop(port);
                continue;
            }
            bio_sensor_readers[port].read_pending = true;
            joybus_accessory_read_async(
                port, JOYBUS_ACCESSORY_ADDR_BIO_PULSE,
                bio_sensor_read_callback, (void *)port
            );
        }
    }
}

void bio_sensor_init(void)
{
    // Just increment the refcount if already initialized
	if (bio_sensor_init_refcount++ > 0) { return; }

    register_VI_handler(bio_sensor_vi_interrupt_callback);
}

void bio_sensor_close(void)
{
    // Do nothing if there are still dangling references.
	if (--bio_sensor_init_refcount > 0) { return; }

    unregister_VI_handler(bio_sensor_vi_interrupt_callback);
    JOYPAD_PORT_FOREACH (port) { bio_sensor_read_stop(port); }
}

void bio_sensor_read_start(joypad_port_t port)
{
    if (!BIO_SENSOR_CONNECTED(port)) { return; }
    if (bio_sensor_readers[port].state != BIO_SENSOR_STATE_STOPPED) { return; }
    volatile bio_sensor_reader_t *reader = &bio_sensor_readers[port];
    memset((void *)reader, 0, sizeof(*reader));
    reader->state = BIO_SENSOR_STATE_RESTING;
    reader->period_start_ticks = timer_ticks();
}

void bio_sensor_read_stop(joypad_port_t port)
{
    volatile bio_sensor_reader_t *reader = &bio_sensor_readers[port];
    memset((void *)reader, 0, sizeof(*reader));
}

bool bio_sensor_get_active(joypad_port_t port)
{
    if (!BIO_SENSOR_CONNECTED(port)) { return false; }
    return bio_sensor_readers[port].state != BIO_SENSOR_STATE_STOPPED;
}

bool bio_sensor_get_pulsing(joypad_port_t port)
{
    if (!BIO_SENSOR_CONNECTED(port)) { return false; }
    return bio_sensor_readers[port].state == BIO_SENSOR_STATE_PULSING;
}

int bio_sensor_get_bpm(joypad_port_t port)
{
    if (!BIO_SENSOR_CONNECTED(port)) { return 0; }
    volatile bio_sensor_reader_t *reader = &bio_sensor_readers[port];
    int num_periods = reader->period_counter;
    if (num_periods < BIO_SENSOR_PERIODS_MINIMUM)
    {
        // Insufficient data to calculate BPM
        return 0;
    }
    if (num_periods > BIO_SENSOR_PERIODS_MAXIMUM)
    {
        num_periods = BIO_SENSOR_PERIODS_MAXIMUM;
    }
    float sum = 0.0;
    // Read from the circular buffer in chronological order
    // When buffer is full, oldest data starts at period_cursor
    unsigned start_index = 0;
    if (reader->period_counter >= BIO_SENSOR_PERIODS_MAXIMUM)
    {
        // Buffer has wrapped around, start from oldest entry
        start_index = reader->period_cursor;
    }
    for (size_t i = 0; i < num_periods; i++)
    {
        unsigned index = (start_index + i) % BIO_SENSOR_PERIODS_MAXIMUM;
        sum += reader->beats_per_period[index];
    }
    return (sum / (float)num_periods) * BIO_SENSOR_PERIODS_PER_MINUTE;
}

/** @} */ /* bio_sensor */
