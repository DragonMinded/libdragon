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
    JOYPAD_ACCESSORY_STATE_DETECT_CPAK_BANK_WRITE,
    JOYPAD_ACCESSORY_STATE_DETECT_CPAK_LABEL_BACKUP,
    JOYPAD_ACCESSORY_STATE_DETECT_CPAK_LABEL_WRITE,
    JOYPAD_ACCESSORY_STATE_DETECT_CPAK_LABEL_READ,
    JOYPAD_ACCESSORY_STATE_DETECT_CPAK_LABEL_RESTORE,
    JOYPAD_ACCESSORY_STATE_DETECT_RUMBLE_PROBE_WRITE,
    JOYPAD_ACCESSORY_STATE_DETECT_RUMBLE_PROBE_READ,
    JOYPAD_ACCESSORY_STATE_DETECT_TRANSFER_PROBE_ON,
    JOYPAD_ACCESSORY_STATE_DETECT_TRANSFER_PROBE_READ,
    JOYPAD_ACCESSORY_STATE_DETECT_TRANSFER_PROBE_OFF,
    JOYPAD_ACCESSORY_STATE_DETECT_SNAP_PROBE_WRITE,
    JOYPAD_ACCESSORY_STATE_DETECT_SNAP_PROBE_READ,
    // Accessory read states
    JOYPAD_ACCESSORY_STATE_READ,
    // Accessory write states
    JOYPAD_ACCESSORY_STATE_WRITE,
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

/** @brief Type of transfer performed by #joypad_accessory_xfer_async */
typedef enum
{
    JOYPAD_ACCESSORY_XFER_READ,
    JOYPAD_ACCESSORY_XFER_WRITE,
} joypad_accessory_xfer_t;

/** @brief Controller pak address to perform bank switching */
#define JOYPAD_CONTROLLER_PAK_BANK_SWITCH_ADDRESS       0x8000

/** @brief Callback function signature for #joypad_accessory_xfer_async */
typedef void (*joypad_accessory_io_callback_t)(joypad_accessory_error_t error, void *ctx);

/** @brief Joypad Accessory I/O operation state */
typedef struct
{
    uint8_t *start;
    uint8_t *end;
    uint8_t *cursor;
    uint16_t cart_addr;
    joypad_accessory_io_callback_t callback;
    void *ctx;
} joypad_accessory_io_t;

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
    uint8_t cpak_label_backup[JOYBUS_ACCESSORY_DATA_SIZE];
    joypad_accessory_io_t io;
    timer_link_t *transfer_pak_wait_timer;
    joybus_transfer_pak_status_t transfer_pak_status;
    joypad_transfer_pak_io_t transfer_pak_io;
} joypad_accessory_t;

/**
 * @brief Reset the accessory state for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 */
void joypad_accessory_reset(joypad_port_t port);

/**
 * @brief Detect which accessory is inserted in an N64 controller.
 *
 * * Step 1: Ensure Transfer Pak is turned off
 * * Step 2A: Set Controller Pak "linear paging bank" to 0
 * * Step 2B: Backup the Controller Pak "label" area
 * * Step 2C: Overwrite the Controller Pak "label" area
 * * Step 2D: Read back the "label" area to detect Controller Pak
 * * Step 2E: Restore the Controller Pak "label" area
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
 * @brief Read or write data from a Joypad accessory asynchronously.
 * 
 * This function can perform a bulk transfer of data from a Joypad accessory.
 * A bulk transfer can read or write any number of bytes from any starting
 * address in the accessory, including misaligned addresses.
 * 
 * It builds upon the lower level primitives #joybus_accessory_read and
 * #joybus_accessory_write, which are limited to 32-byte, aligned data blocks.
 * To perform misaligned writes, this function will perform read-modify-write
 * operations on the accessory when needed.
 * 
 * This function can operate on any Joypad accessory.
 * 
 * The joybus protocol for accessories includes a builtin checksum to detect
 * corruptions on the wire (that can indeed happen during normal operation).
 * This function will automatically retry the transfer in case of a checksum
 * error (up to some hardcoded number of times) and then eventually fail
 * with a #JOYPAD_ACCESSORY_ERROR_CHECKSUM error. If you get this error,
 * it might be useless to try again and you can just assume the connection
 * to the accessory is electrically faulty.
 * 
 * @note Multi-bank Controller Paks (with sizes > 32 KiB), require an explicit
 *       bank switch operation to access data beyond the first 32 KiB. See
 *       #joypad_controller_pak_set_bank; notice also that only 32 KiB at a time will be
 *       available so the valid address range for Controller Paks is 0x0000-0x7FFF.
 * 
 * @param port          Joypad port number (#joypad_port_t)
 * @param xfer          Transfer direction (#JOYPAD_ACCESSORY_XFER_READ or #JOYPAD_ACCESSORY_XFER_WRITE)
 * @param start_addr    Starting address in the accessory to read from, or write to.
 *                      There is no alignment requirement for this address.
 * @param dst           Destination buffer to read accessory data into.
 * @param len           Number of bytes to read. Any number of bytes can be read.
 * @param callback      Callback function to call when the read operation completes.
 * @param ctx           Opaque pointer to pass to the callback function.
 */
void joypad_accessory_xfer_async(
    joypad_port_t port,
    joypad_accessory_xfer_t xfer,
    uint16_t start_addr,
    void *dst,
    size_t len,
    joypad_accessory_io_callback_t callback,
    void *ctx
);

/**
 * @brief Read or write data from a joypad accessory.
 * 
 * This is the blocking version of #joypad_accessory_xfer_async. Like the
 * asynchronous version, this function can read or write any number of bytes
 * from any starting address in the accessory.
 * 
 * This builds upon the lower level primitives #joybus_accessory_read and
 * #joybus_accessory_write, which are limited to 32-byte, aligned data blocks.
 * 
 * @param port          Joypad port number (#joypad_port_t)
 * @param xfer          Transfer direction (#JOYPAD_ACCESSORY_XFER_READ or #JOYPAD_ACCESSORY_XFER_WRITE)
 * @param start_addr    Starting address in the accessory to read from.
 *                      There is no alignment requirement for this address.
 * @param dst           Destination buffer to read accessory data into.
 * @param len           Number of bytes to read. Any number of bytes can be read.
 * @return joypad_accessory_error_t     Error code indicating the result of the read operation.
 */
joypad_accessory_error_t joypad_accessory_xfer(
    joypad_port_t port,
    joypad_accessory_xfer_t xfer,
    uint16_t start_addr,
    void *dst,
    size_t len
);

/**
 * @brief Write data to a Joypad accessory asynchronously.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * @param start_addr Starting address in the accessory to write to.
 * @param src Source buffer of data to write to the accessory.
 * @param len Number of bytes to write.
 * @param callback Callback function to call when the write operation completes.
 * @param ctx Opaque pointer to pass to the callback function.
 */
void joypad_accessory_write_async(
    joypad_port_t port,
    uint16_t start_addr,
    void *src,
    size_t len,
    joypad_accessory_io_callback_t callback,
    void *ctx
);

/**
 * @brief Select the active bank for a Controller Pak.
 * 
 * Most controller paks (including all first-party ones) have a single bank
 * of 32 KiB of storage. However, some third-party controller paks have
 * multiple banks, and require an explicit bank switch operation to access
 * data beyond the first 32 KiB.
 * 
 * Generic transfer functions for accessories like #joypad_accessory_xfer_async
 * will only access the active bank.
 * 
 * There is no way to probe the number of banks in a Controller Pak at the
 * hardware level. In situation where probing is necessary (eg: formatting
 * functions), write tests can be performed to determine the number of banks.
 * 
 * @param port          Joypad port number (#joypad_port_t)
 * @param bank          Bank number to switch to.
 * @return joypad_accessory_error_t    Error code for the transfer operation. 
 */
joypad_accessory_error_t joypad_controller_pak_set_bank(joypad_port_t port, uint8_t bank);


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
