/**
 * @file joypad_accessory.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joypad accessory helpers
 * @ingroup joypad
 */

#ifndef __LIBDRAGON_JOYPAD_ACCESSORY_H
#define __LIBDRAGON_JOYPAD_ACCESSORY_H

#include <stddef.h>
#include <stdint.h>

#include "joybus_accessory_internal.h"
#include "joypad.h"
#include "timer.h"

/**
 * @addtogroup joypad
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Number of times to retry accessory commands. */
#define JOYPAD_ACCESSORY_RETRY_LIMIT 2

/** @brief Joypad accessory states enumeration */
typedef enum
{
    JOYPAD_ACCESSORY_STATE_IDLE = 0,
    // Accessory detection routine states
    JOYPAD_ACCESSORY_STATE_DETECT_INIT,
    JOYPAD_ACCESSORY_STATE_DETECT_LABEL_WRITE,
    JOYPAD_ACCESSORY_STATE_DETECT_LABEL_READ,
    JOYPAD_ACCESSORY_STATE_DETECT_RUMBLE_PROBE_WRITE,
    JOYPAD_ACCESSORY_STATE_DETECT_RUMBLE_PROBE_READ,
    JOYPAD_ACCESSORY_STATE_DETECT_TRANSFER_PROBE_ON,
    JOYPAD_ACCESSORY_STATE_DETECT_TRANSFER_PROBE_READ,
    JOYPAD_ACCESSORY_STATE_DETECT_TRANSFER_PROBE_OFF,
    JOYPAD_ACCESSORY_STATE_DETECT_SNAP_PROBE_WRITE,
    JOYPAD_ACCESSORY_STATE_DETECT_SNAP_PROBE_READ,
    // Rumble Pak motor control states
    JOYPAD_ACCESSORY_STATE_RUMBLE_WRITE,
    // Transfer Pak power control states
    JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_PROBE_WRITE,
    JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_PROBE_WAIT,
    JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_STATUS_WRITE,
    JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_STATUS_WAIT,
    JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_STATUS_READ,
    // Transfer Pak cartridge read states
    JOYPAD_ACCESSORY_STATE_TRANSFER_LOAD_STATUS_READ,
    JOYPAD_ACCESSORY_STATE_TRANSFER_LOAD_BANK_WRITE,
    JOYPAD_ACCESSORY_STATE_TRANSFER_LOAD_DATA_READ,
    // Transfer Pak cartridge write states
    JOYPAD_ACCESSORY_STATE_TRANSFER_STORE_STATUS_READ,
    JOYPAD_ACCESSORY_STATE_TRANSFER_STORE_BANK_WRITE,
    JOYPAD_ACCESSORY_STATE_TRANSFER_STORE_DATA_WRITE,
} joypad_accessory_state_t;

/** @brief Is Joypad accessory currently in detection state? */
#define joypad_accessory_state_is_detecting(state) \
    ((state) >= JOYPAD_ACCESSORY_STATE_DETECT_INIT && \
     (state) <= JOYPAD_ACCESSORY_STATE_DETECT_SNAP_PROBE_READ)

/** @brief Is Joypad accessory currently in Transfer Pak enabling state? */
#define joypad_accessory_state_is_transfer_enabling(state) \
    ((state) >= JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_PROBE_WRITE && \
     (state) <= JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_STATUS_READ)

/** @brief Is Joypad accessory currently in Transfer Pak loading state? */
#define joypad_accessory_state_is_transfer_loading(state) \
    ((state) >= JOYPAD_ACCESSORY_STATE_TRANSFER_LOAD_STATUS_READ && \
     (state) <= JOYPAD_ACCESSORY_STATE_TRANSFER_LOAD_DATA_READ)

/** @brief Is Joypad accessory currently in Transfer Pak storing state? */
#define joypad_accessory_state_is_transfer_storing(state) \
    ((state) >= JOYPAD_ACCESSORY_STATE_TRANSFER_STORE_STATUS_READ && \
     (state) <= JOYPAD_ACCESSORY_STATE_TRANSFER_STORE_DATA_WRITE)

/** @brief Joypad accessory errors enumeration */
typedef enum
{
    JOYPAD_ACCESSORY_ERROR_PENDING = -1,
    JOYPAD_ACCESSORY_ERROR_NONE = 0,
    JOYPAD_ACCESSORY_ERROR_ABSENT,
    JOYPAD_ACCESSORY_ERROR_CHECKSUM,
    JOYPAD_ACCESSORY_ERROR_TRANSFER_PAK_STATUS_CHANGE,
    JOYPAD_ACCESSORY_ERROR_UNKNOWN,
} joypad_accessory_error_t;

/** @brief Joypad N64 Transfer Pak I/O operation state */
typedef struct
{
    uint8_t *start;
    uint8_t *end;
    uint8_t *cursor;
    uint8_t bank;
    uint16_t cart_addr;
    uint16_t tpak_addr;
} joypad_transfer_pak_io_t;

/** @brief Joypad accessory structure */
typedef struct joypad_accessory_s
{
    uint8_t status;
    joypad_accessory_type_t type;
    joypad_accessory_state_t state;
    joypad_accessory_error_t error;
    unsigned retries;
    timer_link_t *transfer_pak_wait_timer;
    joybus_transfer_pak_status_t transfer_pak_status;
    joypad_transfer_pak_io_t transfer_pak_io;
} joypad_accessory_t;

/**
 * @brief Detect which accessory is inserted in an N64 controller.
 *
 * * Step 1: Ensure Transfer Pak is turned off
 * * Step 2A: Overwrite "label" area to detect Controller Pak
 * * Step 2B: Read back the "label" area to detect Controller Pak
 * * Step 3A: Write probe value to detect Rumble Pak
 * * Step 3B: Read probe value to detect Rumble Pak
 * * Step 4A: Write probe value to detect Transfer Pak
 * * Step 4B: Read probe value to detect Transfer Pak
 * * Step 4C: Write probe value to turn off Transfer Pak
 * * Step 5A: Write probe value to detect Snap Station
 * * Step 5B: Read probe value to detect Snap Station
 *
 * @param port Joypad port to detect the accessory on (#joypad_port_t)
 */
void joypad_accessory_detect_async(joypad_port_t port);

/**
 * @brief Turn the Rumble Pak motor on or off for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * @param active Whether the motor should be on (true) or off (false)
 */
void joypad_rumble_pak_toggle_async(joypad_port_t port, bool active);

/**
 * @brief Get the Transfer Pak status byte for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Transfer Pak status byte (#joybus_transfer_pak_status_t)
 */
uint8_t joypad_get_transfer_pak_status(joypad_port_t port);

/**
 * @brief Enable or disable the Transfer Pak for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * @param enabled Whether the Transfer Pak should be enabled (true) or disabled (false)
 */
void joypad_transfer_pak_enable_async(joypad_port_t port, bool enabled);

/**
 * @brief Load data from the GB cartridge inserted in a Transfer Pak.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * @param cart_addr Starting address in the GB cartridge to load from.
 * @param[out] dst Destination buffer to load cartridge data into. 
 * @param len Number of bytes to load (must be a multiple of 32).
 */
void joypad_transfer_pak_load_async(joypad_port_t port, uint16_t cart_addr, void *dst, size_t len);

/**
 * @brief Store data on the GB cartridge inserted in a Transfer Pak.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * @param cart_addr Starting address in the GB cartridge to store into.
 * @param[in] src Source buffer of data to store on GB cartridge. 
 * @param len Number of bytes to store (must be a multiple of 32).
 */
void joypad_transfer_pak_store_async(joypad_port_t port, uint16_t cart_addr, void *src, size_t len);

#ifdef __cplusplus
}
#endif

/** @} */ /* joypad */

#endif
