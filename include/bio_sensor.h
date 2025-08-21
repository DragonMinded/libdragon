/**
 * @file bio_sensor.h
 * @author Christopher Bonhage <me@christopherbonhage.com>
 * @brief Bio Sensor subsystem
 * @ingroup bio_sensor
 */

#ifndef LIBDRAGON_BIO_SENSOR_H
#define LIBDRAGON_BIO_SENSOR_H

#include <stdbool.h>

#include "joypad.h"

/**
 * @defgroup bio_sensor Bio Sensor Subsystem
 * @ingroup peripherals
 * @brief Bio Sensor accessory interface.
 * @author Christopher Bonhage <me@christopherbonhage.com>
 *
 * Bio Sensor (NUS-A-BIO-JPN) is an N64 controller accessory to read a player's
 * heartbeat using an infrared sensor. It was only released in Japan, and was
 * only supported by a single retail game, "Tetris 64". The Bio Sensor is a
 * video game accessory. It is NOT a medical device and is for entertainment
 * purposes only.
 *
 * The Bio Sensor subsystem provides an interface for reading heartbeat data
 * from multiple Bio Sensor accessories connected to different joypad ports.
 * Each port can be independently started and stopped for reading heartbeat data.
 *
 * Bio Sensor reads are performed at 60Hz (NTSC) or 50Hz (PAL) using the
 * vertical interrupt (VI) handler. The subsystem maintains a rolling window of
 * heartbeat counts to calculate the current heart rate in beats per minute (BPM).
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Bio Sensor subsystem
 *
 * Must be called before starting Bio Sensor reads on any port.
 *
 * Registers the VI interrupt handler for periodic sensor reads.
 * Uses reference counting to support multiple init/close pairs.
 */
void bio_sensor_init(void);

/**
 * @brief Close the Bio Sensor subsystem
 *
 * Should be called when Bio Sensor functionality is no longer needed.
 *
 * Decrements the reference count and unregisters the VI interrupt handler
 * when the count reaches zero.
 */
void bio_sensor_close(void);

/**
 * @brief Start reading heartbeat data from a Bio Sensor
 *
 * Initializes the reader context for the specified port and begins
 * continuous heartbeat monitoring. The sensor will be read at 60Hz (NTSC)
 * or 50Hz (PAL) via the VI interrupt handler. After starting, it will
 * take 4-5 seconds to accumulate enough data for a heart rate reading.
 *
 * @param port The joypad port with the Bio Sensor accessory attached
 */
void bio_sensor_read_start(joypad_port_t port);

/**
 * @brief Stop reading heartbeat data from a Bio Sensor
 *
 * Clears the reader context for the specified port and stops all
 * heartbeat monitoring. Also called automatically if the Bio Sensor
 * is disconnected during reading.
 *
 * @param port The joypad port to stop reading from
 */
void bio_sensor_read_stop(joypad_port_t port);

/**
 * @brief Check if Bio Sensor is actively reading heartbeat data
 *
 * @param port The joypad port to check
 * @return true if heartbeat monitoring is active, false if stopped
 */
bool bio_sensor_get_active(joypad_port_t port);

/**
 * @brief Check if a heartbeat pulse is currently detected
 *
 * @param port The joypad port to check
 * @return true if the heart is currently beating (pulsing state), false otherwise
 */
bool bio_sensor_get_pulsing(joypad_port_t port);

/**
 * @brief Calculate the current heart rate in beats per minute (BPM)
 *
 * Calculates the average BPM based on heartbeat counts from multiple
 * measurement periods. Requires at least 8 measurement periods (4 seconds)
 * of data before returning a valid BPM. Uses up to 16 periods (8 seconds)
 * for the rolling average calculation.
 *
 * @param port The joypad port to read BPM from
 * @return Heart rate in beats per minute, or 0 if insufficient data
 */
int bio_sensor_get_bpm(joypad_port_t port);

#ifdef __cplusplus
}
#endif

/** @} */ /* bio_sensor */

#endif /* LIBDRAGON_BIO_SENSOR_H */
