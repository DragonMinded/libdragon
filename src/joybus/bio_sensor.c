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
#include "timer.h"

/**
 * @addtogroup bio_sensor
 * @{
 */

 /** @brief One minute in microseconds */
 #define MICROS_PER_MINUTE 60000000LL
 /** @brief Minimum number of beats required before calculating BPM */
#define BIO_SENSOR_BEATS_MIN 3
/** @brief Maximum number of heartbeat timestamps stored in the circular buffer */
#define BIO_SENSOR_BEATS_MAX 9

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
 * Uses a circular buffer to track individual beat timestamps for IBI calculation.
 */
typedef struct
{
    /** @brief Flag indicating an async read operation is in progress */
    bool read_pending;
    /** @brief Current heartbeat detection state */
    bio_sensor_state_t state;
    /** @brief Circular buffer storing timestamps of detected heartbeats */
    int64_t beat_timestamps[BIO_SENSOR_BEATS_MAX];
    /** @brief Current write position in the circular buffer */
    unsigned beat_cursor;
    /** @brief Total number of heartbeats detected since reading started */
    unsigned beat_count;
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

    uint8_t sensor_data = cmd->recv.data[0];
    bio_sensor_state_t next_state = BIO_SENSOR_STATE_STOPPED;
    if (sensor_data == 0x00) next_state = BIO_SENSOR_STATE_PULSING;
    if (sensor_data == 0x03) next_state = BIO_SENSOR_STATE_RESTING;

    if (
        reader->state == BIO_SENSOR_STATE_PULSING &&
        next_state == BIO_SENSOR_STATE_RESTING
    ) {
        // Store the timestamp of this beat
        int64_t now_ticks = timer_ticks();
        reader->beat_timestamps[reader->beat_cursor] = now_ticks;
        reader->beat_cursor = (reader->beat_cursor + 1) % BIO_SENSOR_BEATS_MAX;
        reader->beat_count++;
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

    // Need at least 3 beats to calculate 2 intervals
    if (reader->beat_count < BIO_SENSOR_BEATS_MIN) { return 0; }

    // Determine how many beats to use (up to buffer size)
    unsigned num_beats = reader->beat_count;
    if (num_beats > BIO_SENSOR_BEATS_MAX) {
        num_beats = BIO_SENSOR_BEATS_MAX;
    }
    int num_intervals = num_beats - 1;

    // Calculate oldest and newest beat indices
    unsigned oldest_idx, newest_idx;
    if (reader->beat_count <= BIO_SENSOR_BEATS_MAX) {
        // Buffer hasn't wrapped yet
        oldest_idx = 0;
        newest_idx = num_beats - 1;
    } else {
        // Buffer has wrapped
        oldest_idx = reader->beat_cursor;
        newest_idx = (reader->beat_cursor + BIO_SENSOR_BEATS_MAX - 1) % BIO_SENSOR_BEATS_MAX;
    }

    // Calculate total time span between first and last beat
    int64_t time_span_ticks = reader->beat_timestamps[newest_idx] - reader->beat_timestamps[oldest_idx];
    int64_t time_span_us = TIMER_MICROS_LL(time_span_ticks);

    // Avoid division by zero and handle invalid time spans
    if (time_span_us <= 0) { return 0; }

    // Calculate BPM with rounding
    return (MICROS_PER_MINUTE * num_intervals + time_span_us / 2) / time_span_us;
}

/** @} */ /* bio_sensor */
