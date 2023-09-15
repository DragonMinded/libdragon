
/**
 * @file joypad_accessory.c
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joypad accessory helpers
 * @ingroup joypad
 */

#include <string.h>

#include "debug.h"
#include "joypad_internal.h"

static void joypad_accessory_detect_read_callback(uint64_t *out_dwords, void *ctx);
static void joypad_accessory_detect_write_callback(uint64_t *out_dwords, void *ctx);
static void joypad_transfer_pak_enable_read_callback(uint64_t *out_dwords, void *ctx);
static void joypad_transfer_pak_enable_write_callback(uint64_t *out_dwords, void *ctx);
static void joypad_transfer_pak_load_read_callback(uint64_t *out_dwords, void *ctx);
static void joypad_transfer_pak_load_write_callback(uint64_t *out_dwords, void *ctx);
static void joypad_transfer_pak_store_read_callback(uint64_t *out_dwords, void *ctx);
static void joypad_transfer_pak_store_write_callback(uint64_t *out_dwords, void *ctx);
static void joypad_transfer_pak_wait_timer_callback(int ovfl, void *ctx);

/**
 * @addtogroup joypad
 * @{
 */

/**
 * @brief Determine whether the accessory read command was successful. Retry if necessary.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * @param[in] cmd Joybus "N64 Accessory Read" command output
 * @param[in] retry_callback Callback to use if the command needs to be retried.
 * @param[in,out] retry_ctx Context to pass to the retry callback.
 * 
 * @retval true The accessory read command failed
 * @retval false The accessory read command succeeded
 */
static bool joypad_accessory_check_read_crc_error(
    joypad_port_t port,
    const joybus_cmd_n64_accessory_read_port_t *cmd,
    joybus_callback_t retry_callback,
    void *retry_ctx
)
{
    volatile joypad_device_hot_t *device = &joypad_devices_hot[port];
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];
    int crc_status = joybus_accessory_compare_data_crc(cmd->recv.data, cmd->recv.data_crc);
    switch (crc_status)
    {
        case JOYBUS_ACCESSORY_IO_STATUS_OK:
        {
            // Read operation was successful!
            accessory->error = JOYPAD_ACCESSORY_ERROR_NONE;
            return false;
        }
        case JOYBUS_ACCESSORY_IO_STATUS_NO_PAK:
        {
            // Accessory is no longer connected!
            device->rumble_method = JOYPAD_RUMBLE_METHOD_NONE;
            device->rumble_active = false;
            accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
            accessory->type = JOYPAD_ACCESSORY_TYPE_NONE;
            accessory->status = JOYBUS_IDENTIFY_STATUS_ACCESSORY_ABSENT;
            accessory->error = JOYPAD_ACCESSORY_ERROR_ABSENT;
            return true;
        }
        case JOYBUS_ACCESSORY_IO_STATUS_BAD_CRC:
        {
            size_t retries = accessory->retries;
            if (retries < JOYPAD_ACCESSORY_RETRY_LIMIT)
            {
                // Retry: Bad communication with the accessory
                accessory->retries = retries + 1;
                accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
                uint16_t retry_addr = cmd->send.addr_checksum;
                retry_addr &= JOYBUS_ACCESSORY_ADDR_MASK_OFFSET;
                joybus_accessory_read_async(
                    port, retry_addr,
                    retry_callback, retry_ctx
                );
                return true;
            }
            else
            {
                // Retry limit exceeded; read failed
                accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
                accessory->error = JOYPAD_ACCESSORY_ERROR_CHECKSUM;
                return true;
            }
        }
        default:
        {
            // This should never happen!
            assertf(false, "Unknown joybus_accessory_io_status_t value: %d", crc_status);
            accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
            accessory->error = JOYPAD_ACCESSORY_ERROR_UNKNOWN;
            return true;
        }
    }
}

/**
 * @brief Determine whether the accessory write command was successful. Retry if necessary.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * @param[in] cmd Joybus "N64 Accessory Write" command output
 * @param[in] retry_callback Callback to use if the command needs to be retried.
 * @param[in,out] retry_ctx Context to pass to the retry callback.
 * 
 * @retval true The accessory write command failed
 * @retval false The accessory write command succeeded
 */
static bool joypad_accessory_write_crc_error_check(
    joypad_port_t port,
    const joybus_cmd_n64_accessory_write_port_t *cmd,
    joybus_callback_t retry_callback,
    void *retry_ctx
)
{
    volatile joypad_device_hot_t *device = &joypad_devices_hot[port];
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];
    int crc_status = joybus_accessory_compare_data_crc(cmd->send.data, cmd->recv.data_crc);
    switch (crc_status)
    {
        case JOYBUS_ACCESSORY_IO_STATUS_OK:
        {
            // Write operation was successful!
            // Intentionally preserve accessory status in this case
            accessory->error = JOYPAD_ACCESSORY_ERROR_NONE;
            return false;
        }
        case JOYBUS_ACCESSORY_IO_STATUS_NO_PAK:
        {
            // Accessory is no longer connected!
            device->rumble_method = JOYPAD_RUMBLE_METHOD_NONE;
            device->rumble_active = false;
            accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
            accessory->type = JOYPAD_ACCESSORY_TYPE_NONE;
            accessory->status = JOYBUS_IDENTIFY_STATUS_ACCESSORY_ABSENT;
            accessory->error = JOYPAD_ACCESSORY_ERROR_ABSENT;
            return true;
        }
        case JOYBUS_ACCESSORY_IO_STATUS_BAD_CRC:
        {
            size_t retries = accessory->retries;
            if (retries < JOYPAD_ACCESSORY_RETRY_LIMIT)
            {
                // Retry: Bad communication with the accessory
                // Intentionally preserve accessory status in this case
                accessory->retries = retries + 1;
                accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
                uint16_t retry_addr = cmd->send.addr_checksum;
                retry_addr &= JOYBUS_ACCESSORY_ADDR_MASK_OFFSET;
                joybus_accessory_write_async(
                    port, retry_addr, cmd->send.data,
                    retry_callback, retry_ctx
                );
                return true;
            }
            else
            {
                // Retry limit exceeded; write failed
                accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
                accessory->error = JOYPAD_ACCESSORY_ERROR_CHECKSUM;
                return true;
            }
        }
        default:
        {
            // This should never happen!
            assertf(false, "Unknown joybus_accessory_io_status_t value: %d", crc_status);
            accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
            accessory->error = JOYPAD_ACCESSORY_ERROR_UNKNOWN;
            return true;
        }
    }
}

/**
 * @brief Initialize the Transfer Pak wait timer if necessary.
 * 
 * @param port Joypad port number (#joypad_port_t)
 */
void joypad_transfer_pak_wait_timer_init(joypad_port_t port)
{
    ASSERT_JOYPAD_PORT_VALID(port);
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];
    // Ensure there is a disabled timer ready to restart:
    if (!accessory->transfer_pak_wait_timer)
    {
        // The Transfer Pak takes about 200 milliseconds to fully power-on
        // after being probed; sadly, we must use a hard-coded delay
        accessory->transfer_pak_wait_timer = new_timer_context(
            TIMER_TICKS(200 * 1000), TF_ONE_SHOT | TF_DISABLED,
            joypad_transfer_pak_wait_timer_callback, (void *)port
        );
    }
}

/**
 * @brief Callback for the Transfer Pak wait timer.
 * 
 * @param ovfl Timer overflow ticks
 * @param[in,out] ctx Opaque pointer to the Transfer Pak wait timer context
 */
static void joypad_transfer_pak_wait_timer_callback(int ovfl, void *ctx)
{
    joypad_port_t port = (joypad_port_t)ctx;
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];
    joypad_accessory_state_t state = accessory->state;
    if (state == JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_PROBE_WAIT)
    {
        uint8_t write_data[JOYBUS_ACCESSORY_DATA_SIZE];
        memset(write_data, JOYBUS_TRANSFER_PAK_STATUS_ACCESS, sizeof(write_data));
        accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_STATUS_WRITE;
        accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
        accessory->retries = 0;
        joybus_accessory_write_async(
            port, JOYBUS_ACCESSORY_ADDR_TRANSFER_STATUS, write_data,
            joypad_transfer_pak_enable_write_callback, ctx
        );
    }
    else if (state == JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_STATUS_WAIT)
    {
        accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_STATUS_READ;
        accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
        accessory->retries = 0;
        joybus_accessory_read_async(
            port, JOYBUS_ACCESSORY_ADDR_TRANSFER_STATUS,
            joypad_transfer_pak_enable_read_callback, ctx
        );
    }
}

/**
 * @brief Callback for the accessory read commands used by #joypad_accessory_detect_async.
 * 
 * @param out_dwords Joybus output block
 * @param ctx Opaque pointer used to pass the Joypad port number
 */
static void joypad_accessory_detect_read_callback(uint64_t *out_dwords, void *ctx)
{
    const uint8_t *out_bytes = (void *)out_dwords;
    joypad_port_t port = (joypad_port_t)ctx;
    volatile joypad_device_hot_t *device = &joypad_devices_hot[port];
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];
    joypad_accessory_state_t state = accessory->state;
    if (!joypad_accessory_state_is_detecting(state))
    {
        return; // Unexpected accessory state!
    }

    uint8_t write_data[JOYBUS_ACCESSORY_DATA_SIZE];
    const joybus_cmd_n64_accessory_read_port_t *cmd =
        (void *)&out_bytes[port + JOYBUS_COMMAND_METADATA_SIZE];
    joybus_callback_t retry_callback = joypad_accessory_detect_read_callback;
    if (joypad_accessory_check_read_crc_error(port, cmd, retry_callback, ctx))
    {
        return; // Accessory communication error!
    }
    else if (state == JOYPAD_ACCESSORY_STATE_DETECT_LABEL_READ)
    {
        // Compare the expected label with what was actually read back
        for (size_t i = 0; i < sizeof(write_data); ++i) write_data[i] = i;
        if (memcmp(cmd->recv.data, write_data, sizeof(write_data)) == 0)
        {
            // Success: Label write persisted; this appears to be a Controller Pak
            accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
            accessory->type = JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK;
        }
        else
        {
            // Step 3A: Write probe value to detect Rumble Pak
            memset(write_data, JOYBUS_ACCESSORY_PROBE_RUMBLE_PAK, sizeof(write_data));
            accessory->state = JOYPAD_ACCESSORY_STATE_DETECT_RUMBLE_PROBE_WRITE;
            accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
            accessory->retries = 0;
            joybus_accessory_write_async(
                port, JOYBUS_ACCESSORY_ADDR_PROBE, write_data,
                joypad_accessory_detect_write_callback, ctx
            );
        }
    }
    else if (state == JOYPAD_ACCESSORY_STATE_DETECT_RUMBLE_PROBE_READ)
    {
        uint8_t probe_value = cmd->recv.data[0];
        if (probe_value == JOYBUS_ACCESSORY_PROBE_RUMBLE_PAK)
        {
            // Success: Probe reports that this is a Rumble Pak
            accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
            accessory->type = JOYPAD_ACCESSORY_TYPE_RUMBLE_PAK;
            device->rumble_method = JOYPAD_RUMBLE_METHOD_N64_RUMBLE_PAK;
        }
        else if (probe_value == JOYBUS_ACCESSORY_PROBE_BIO_SENSOR)
        {
            // Success: Bio Sensor responds to all reads with probe value
            accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
            accessory->type = JOYPAD_ACCESSORY_TYPE_BIO_SENSOR;
        }
        else
        {
            // Step 4A: Write probe value to detect Transfer Pak
            memset(write_data, JOYBUS_ACCESSORY_PROBE_TRANSFER_PAK_ON, sizeof(write_data));
            accessory->state = JOYPAD_ACCESSORY_STATE_DETECT_TRANSFER_PROBE_ON;
            accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
            accessory->retries = 0;
            joybus_accessory_write_async(
                port, JOYBUS_ACCESSORY_ADDR_PROBE, write_data,
                joypad_accessory_detect_write_callback, ctx
            );
        }
    }
    else if (state == JOYPAD_ACCESSORY_STATE_DETECT_TRANSFER_PROBE_READ)
    {
        uint8_t probe_value = cmd->recv.data[0];
        if (probe_value == JOYBUS_ACCESSORY_PROBE_TRANSFER_PAK_ON)
        {
            // Step 4C: Write probe value to turn off Transfer Pak
            memset(write_data, JOYBUS_ACCESSORY_PROBE_TRANSFER_PAK_OFF, sizeof(write_data));
            accessory->state = JOYPAD_ACCESSORY_STATE_DETECT_TRANSFER_PROBE_OFF;
            accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
            accessory->retries = 0;
            joybus_accessory_write_async(
                port, JOYBUS_ACCESSORY_ADDR_PROBE, write_data,
                joypad_accessory_detect_write_callback, (void *)port
            );
        }
        else
        {
            // Step 5A: Write probe value to detect Snap Station
            memset(write_data, JOYBUS_ACCESSORY_PROBE_SNAP_STATION, sizeof(write_data));
            accessory->state = JOYPAD_ACCESSORY_STATE_DETECT_SNAP_PROBE_WRITE;
            accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
            accessory->retries = 0;
            joybus_accessory_write_async(
                port, JOYBUS_ACCESSORY_ADDR_PROBE, write_data,
                joypad_accessory_detect_write_callback, ctx
            );
        }
    }
    else if (state == JOYPAD_ACCESSORY_STATE_DETECT_SNAP_PROBE_READ)
    {
        uint8_t probe_value = cmd->recv.data[0];
        if (probe_value == JOYBUS_ACCESSORY_PROBE_SNAP_STATION)
        {
            // Success: Probe reports that this is a Snap Station
            accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
            accessory->type = JOYPAD_ACCESSORY_TYPE_SNAP_STATION;
        }
        else
        {
            // Failure: Unable to determine which accessory is connected
            accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
            accessory->type = JOYPAD_ACCESSORY_TYPE_UNKNOWN;
            accessory->transfer_pak_status.raw = 0x00;
        }
    }
}

/**
 * @brief Callback for the accessory write commands used by #joypad_accessory_detect_async.
 * 
 * @param out_dwords Joybus output block
 * @param ctx Opaque pointer used to pass the Joypad port number
 */
static void joypad_accessory_detect_write_callback(uint64_t *out_dwords, void *ctx)
{
    const uint8_t *out_bytes = (void *)out_dwords;
    joypad_port_t port = (joypad_port_t)ctx;
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];
    joypad_accessory_state_t state = accessory->state;
    if (!joypad_accessory_state_is_detecting(state))
    {
        return; // Unexpected accessory state!
    }

    const joybus_cmd_n64_accessory_write_port_t *cmd =
        (void *)&out_bytes[port + JOYBUS_COMMAND_METADATA_SIZE];
    joybus_callback_t retry_callback = joypad_accessory_detect_write_callback;
    if (joypad_accessory_write_crc_error_check(port, cmd, retry_callback, ctx))
    {
        return; // Accessory communication error!
    }
    else if (state == JOYPAD_ACCESSORY_STATE_DETECT_INIT)
    {
        // Transfer Pak has been turned off; reset Transfer Pak status
        accessory->transfer_pak_status.raw = 0x00;
        // Step 2A: Overwrite "label" area to detect Controller Pak
        uint8_t data[JOYBUS_ACCESSORY_DATA_SIZE];
        for (size_t i = 0; i < sizeof(data); ++i) data[i] = i;
        accessory->state = JOYPAD_ACCESSORY_STATE_DETECT_LABEL_WRITE;
        accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
        accessory->retries = 0;
        joybus_accessory_write_async(
            port, JOYBUS_ACCESSORY_ADDR_LABEL, data,
            joypad_accessory_detect_write_callback, ctx
        );
    }
    else if (state == JOYPAD_ACCESSORY_STATE_DETECT_LABEL_WRITE)
    {
        // Step 2B: Read back the "label" area to detect Controller Pak
        accessory->state = JOYPAD_ACCESSORY_STATE_DETECT_LABEL_READ;
        accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
        accessory->retries = 0;
        joybus_accessory_read_async(
            port, JOYBUS_ACCESSORY_ADDR_LABEL,
            joypad_accessory_detect_read_callback, ctx
        );
    }
    else if (state == JOYPAD_ACCESSORY_STATE_DETECT_RUMBLE_PROBE_WRITE)
    {
        // Step 3B: Read probe value to detect Rumble Pak
        accessory->state = JOYPAD_ACCESSORY_STATE_DETECT_RUMBLE_PROBE_READ;
        accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
        accessory->retries = 0;
        joybus_accessory_read_async(
            port, JOYBUS_ACCESSORY_ADDR_PROBE,
            joypad_accessory_detect_read_callback, ctx
        );
    }
    else if (state == JOYPAD_ACCESSORY_STATE_DETECT_TRANSFER_PROBE_ON)
    {
        // Step 4B: Read probe value to detect Transfer Pak
        accessory->state = JOYPAD_ACCESSORY_STATE_DETECT_TRANSFER_PROBE_READ;
        accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
        accessory->retries = 0;
        joybus_accessory_read_async(
            port, JOYBUS_ACCESSORY_ADDR_PROBE,
            joypad_accessory_detect_read_callback, ctx
        );
    }
    else if (state == JOYPAD_ACCESSORY_STATE_DETECT_TRANSFER_PROBE_OFF)
    {
        // Success: Transfer Pak has been probed and powered off
        accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
        accessory->type = JOYPAD_ACCESSORY_TYPE_TRANSFER_PAK;
        accessory->transfer_pak_status.power = 0;
    }
    else if (state == JOYPAD_ACCESSORY_STATE_DETECT_SNAP_PROBE_WRITE)
    {
        // Step 5B: Read probe value to detect Snap Station
        accessory->state = JOYPAD_ACCESSORY_STATE_DETECT_SNAP_PROBE_READ;
        accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
        accessory->retries = 0;
        joybus_accessory_read_async(
            port, JOYBUS_ACCESSORY_ADDR_PROBE,
            joypad_accessory_detect_read_callback, ctx
        );
    }
}

void joypad_accessory_detect_async(joypad_port_t port)
{
    ASSERT_JOYPAD_PORT_VALID(port);
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];
    // Ensure Transfer Pak wait timer has been initialized
    if (!accessory->transfer_pak_wait_timer)
    {
        joypad_transfer_pak_wait_timer_init(port);
    }
    // Don't interrupt other accessory operations if they are still running
    if (accessory->state == JOYPAD_ACCESSORY_STATE_IDLE)
    {
        // Step 1: Ensure Transfer Pak is turned off
        uint8_t data[JOYBUS_ACCESSORY_DATA_SIZE];
        memset(data, JOYBUS_ACCESSORY_PROBE_TRANSFER_PAK_OFF, sizeof(data));
        accessory->state = JOYPAD_ACCESSORY_STATE_DETECT_INIT;
        accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
        accessory->retries = 0;
        joybus_accessory_write_async(
            port, JOYBUS_ACCESSORY_ADDR_PROBE, data,
            joypad_accessory_detect_write_callback, (void *)port
        );
    }
}

/**
 * @brief Callback for the accessory write commands used by #joypad_rumble_pak_toggle_async.
 * 
 * @param out_dwords Joybus output block
 * @param ctx Opaque pointer used to pass the Joypad port number
 */
static void joypad_rumble_pak_motor_write_callback(uint64_t *out_dwords, void *ctx)
{
    const uint8_t *out_bytes = (void *)out_dwords;
    joypad_port_t port = (joypad_port_t)ctx;
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];
    joypad_accessory_state_t state = accessory->state;
    if (state != JOYPAD_ACCESSORY_STATE_RUMBLE_WRITE)
    {
        return; // Unexpected accessory state!
    }

    const joybus_cmd_n64_accessory_write_port_t *cmd =
        (void *)&out_bytes[port + JOYBUS_COMMAND_METADATA_SIZE];
    joybus_callback_t retry_callback = joypad_rumble_pak_motor_write_callback;
    if (!joypad_accessory_write_crc_error_check(port, cmd, retry_callback, ctx))
    {
        accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
    }
}

void joypad_rumble_pak_toggle_async(joypad_port_t port, bool active)
{
    volatile joypad_device_hot_t *device = &joypad_devices_hot[port];
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];
    device->rumble_active = active;
    accessory->state = JOYPAD_ACCESSORY_STATE_RUMBLE_WRITE;
    accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
    accessory->retries = 0;
    uint8_t motor_data[JOYBUS_ACCESSORY_DATA_SIZE];
    memset(motor_data, active, sizeof(motor_data));
    joybus_accessory_write_async(
        port, JOYBUS_ACCESSORY_ADDR_RUMBLE_MOTOR, motor_data,
        joypad_rumble_pak_motor_write_callback, (void *)port
    );
}

/**
 * @brief Callback for the accessory read commands used by #joypad_transfer_pak_enable_async.
 * 
 * @param out_dwords Joybus output block
 * @param ctx Opaque pointer used to pass the Joypad port number
 */
static void joypad_transfer_pak_enable_read_callback(uint64_t *out_dwords, void *ctx)
{
    const uint8_t *out_bytes = (void *)out_dwords;
    joypad_port_t port = (joypad_port_t)ctx;
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];
    joypad_accessory_state_t state = accessory->state;
    if (!joypad_accessory_state_is_transfer_enabling(state))
    {
        return; // Unexpected accessory state!
    }

    const joybus_cmd_n64_accessory_read_port_t *cmd =
        (void *)&out_bytes[port + JOYBUS_COMMAND_METADATA_SIZE];
    joybus_callback_t retry_callback = joypad_transfer_pak_enable_read_callback;
    if (joypad_accessory_check_read_crc_error(port, cmd, retry_callback, ctx))
    {
        return; // Accessory communication error!
    }
    else if (state == JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_STATUS_READ)
    {
        accessory->transfer_pak_status.raw = cmd->recv.data[0];
        accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
    }
}

/**
 * @brief Callback for the accessory write commands used by #joypad_transfer_pak_enable_async.
 * 
 * @param out_dwords Joybus output block
 * @param ctx Opaque pointer used to pass the Joypad port number
 */
static void joypad_transfer_pak_enable_write_callback(uint64_t *out_dwords, void *ctx)
{
    const uint8_t *out_bytes = (void *)out_dwords;
    joypad_port_t port = (joypad_port_t)ctx;
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];
    joypad_accessory_state_t state = accessory->state;
    if (!joypad_accessory_state_is_transfer_enabling(state))
    {
        return; // Unexpected accessory state!
    }

    const joybus_cmd_n64_accessory_write_port_t *cmd =
        (void *)&out_bytes[port + JOYBUS_COMMAND_METADATA_SIZE];
    joybus_callback_t retry_callback = joypad_transfer_pak_enable_write_callback;
    if (joypad_accessory_write_crc_error_check(port, cmd, retry_callback, ctx))
    {
        return; // Accessory communication error!
    }
    else if (state == JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_PROBE_WRITE)
    {
        if (cmd->send.data[0] == JOYBUS_ACCESSORY_PROBE_TRANSFER_PAK_ON)
        {
            accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_PROBE_WAIT;
            timer_link_t *timer = accessory->transfer_pak_wait_timer;
            assertf(timer, "transfer_pak_wait_timer is NULL on port %d", port + 1);
            restart_timer(timer);
        }
        else
        {
            accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
            accessory->transfer_pak_status.raw = 0x00;
        }
    }
    else if (state == JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_STATUS_WRITE)
    {
        accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_STATUS_WAIT;
        timer_link_t *timer = accessory->transfer_pak_wait_timer;
        assertf(timer, "transfer_pak_wait_timer is NULL on port %d", port + 1);
        restart_timer(timer);
    }
}

void joypad_transfer_pak_enable_async(joypad_port_t port, bool enabled)
{
    ASSERT_JOYPAD_PORT_VALID(port);
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];

    // Turn the Transfer Pak on or off with magic probe values
    uint8_t probe_value = enabled
        ? JOYBUS_ACCESSORY_PROBE_TRANSFER_PAK_ON
        : JOYBUS_ACCESSORY_PROBE_TRANSFER_PAK_OFF
        ;
    uint8_t write_data[JOYBUS_ACCESSORY_DATA_SIZE];
    memset(write_data, probe_value, sizeof(write_data));

    accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_ENABLE_PROBE_WRITE;
    accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
    accessory->retries = 0;
    joybus_accessory_write_async(
        port, JOYBUS_ACCESSORY_ADDR_PROBE, write_data,
        joypad_transfer_pak_enable_write_callback, (void *)port
    );
}

/**
 * @brief Callback for the accessory read commands used by #joypad_transfer_pak_load_async.
 * 
 * @param out_dwords Joybus output block
 * @param ctx Opaque pointer used to pass the Joypad port number
 */
static void joypad_transfer_pak_load_read_callback(uint64_t *out_dwords, void *ctx)
{
    const uint8_t *out_bytes = (void *)out_dwords;
    joypad_port_t port = (joypad_port_t)ctx;
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];
    volatile joypad_transfer_pak_io_t *io = &accessory->transfer_pak_io;
    joypad_accessory_state_t state = accessory->state;
    if (!joypad_accessory_state_is_transfer_loading(state))
    {
        return; // Unexpected accessory state!
    }

    const joybus_cmd_n64_accessory_read_port_t *cmd =
        (void *)&out_bytes[port + JOYBUS_COMMAND_METADATA_SIZE];
    joybus_callback_t retry_callback = joypad_transfer_pak_load_read_callback;
    if (joypad_accessory_check_read_crc_error(port, cmd, retry_callback, ctx))
    {
        return; // Accessory communication error!
    }
    else if (state == JOYPAD_ACCESSORY_STATE_TRANSFER_LOAD_STATUS_READ)
    {
        joybus_transfer_pak_status_t status = { .raw = cmd->recv.data[0] };
        accessory->transfer_pak_status = status;
        if (!status.access || !status.power)
        {
            // The Game Boy cartridge is no longer accessible; bail!
            accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
            accessory->error = JOYPAD_ACCESSORY_ERROR_TRANSFER_PAK_STATUS_CHANGE;
        }
        else if (io->cursor < io->end)
        {
            // Proceed with reading; select a Transfer Pak data bank
            uint8_t write_data[JOYBUS_ACCESSORY_DATA_SIZE];
            memset(write_data, io->bank, sizeof(write_data));
            accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_LOAD_BANK_WRITE;
            accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
            accessory->retries = 0;
            joybus_accessory_write_async(
                port, JOYBUS_ACCESSORY_ADDR_TRANSFER_BANK, write_data,
                joypad_transfer_pak_load_write_callback, ctx
            );
        }
        else
        {
            // Finished reading data
            accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
        }
    }
    else if (state == JOYPAD_ACCESSORY_STATE_TRANSFER_LOAD_DATA_READ)
    {
        memcpy(io->cursor, cmd->recv.data, JOYBUS_ACCESSORY_DATA_SIZE);
        uint8_t *cursor = io->cursor += JOYBUS_ACCESSORY_DATA_SIZE;
        uint16_t tpak_addr = io->tpak_addr += JOYBUS_ACCESSORY_DATA_SIZE;
        uint16_t cart_addr = io->cart_addr += JOYBUS_ACCESSORY_DATA_SIZE;
        int next_bank = cart_addr / JOYBUS_ACCESSORY_TRANSFER_BANK_SIZE;
        if (cursor >= io->end)
        {
            // Check the Transfer Pak status again after storing:
            // If the status says reset or cart pulled, you've got a problem!
            accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_LOAD_STATUS_READ;
            accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
            accessory->retries = 0;
            joybus_accessory_read_async(
                port, JOYBUS_ACCESSORY_ADDR_TRANSFER_STATUS,
                joypad_transfer_pak_load_read_callback, ctx
            );
        }
        else if (next_bank == io->bank)
        {
            // Continue reading data
            accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_LOAD_DATA_READ;
            accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
            accessory->retries = 0;
            joybus_accessory_read_async(
                port, tpak_addr,
                joypad_transfer_pak_load_read_callback, ctx
            );
        }
        else
        {
            // Switch to the next bank
            io->tpak_addr = JOYBUS_ACCESSORY_ADDR_TRANSFER_CART;
            io->bank = next_bank;
            uint8_t write_data[JOYBUS_ACCESSORY_DATA_SIZE];
            memset(write_data, next_bank, sizeof(write_data));
            accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_LOAD_BANK_WRITE;
            accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
            accessory->retries = 0;
            joybus_accessory_write_async(
                port, JOYBUS_ACCESSORY_ADDR_TRANSFER_BANK, write_data,
                joypad_transfer_pak_load_write_callback, ctx
            );
        }
    }
}

/**
 * @brief Callback for the accessory write commands used by #joypad_transfer_pak_load_async.
 * 
 * @param out_dwords Joybus output block
 * @param ctx Opaque pointer used to pass the Joypad port number
 */
static void joypad_transfer_pak_load_write_callback(uint64_t *out_dwords, void *ctx)
{
    const uint8_t *out_bytes = (void *)out_dwords;
    joypad_port_t port = (joypad_port_t)ctx;
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];
    joypad_accessory_state_t state = accessory->state;
    if (!joypad_accessory_state_is_transfer_loading(state))
    {
        return; // Unexpected accessory state!
    }

    const joybus_cmd_n64_accessory_write_port_t *cmd =
        (void *)&out_bytes[port + JOYBUS_COMMAND_METADATA_SIZE];
    joybus_callback_t retry_callback = joypad_transfer_pak_load_write_callback;
    if (joypad_accessory_write_crc_error_check(port, cmd, retry_callback, ctx))
    {
        return; // Accessory communication error!
    }
    else if (state == JOYPAD_ACCESSORY_STATE_TRANSFER_LOAD_BANK_WRITE)
    {
        accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_LOAD_DATA_READ;
        accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
        accessory->retries = 0;
        joybus_accessory_read_async(
            port, accessory->transfer_pak_io.tpak_addr,
            joypad_transfer_pak_load_read_callback, ctx
        );
    }
}

void joypad_transfer_pak_load_async(joypad_port_t port, uint16_t cart_addr, void *dst, size_t len)
{
    ASSERT_JOYPAD_PORT_VALID(port);
    assert(cart_addr % JOYBUS_ACCESSORY_DATA_SIZE == 0);
    assert(len % JOYBUS_ACCESSORY_DATA_SIZE == 0);
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];

    uint8_t bank = cart_addr / JOYBUS_ACCESSORY_TRANSFER_BANK_SIZE;
    uint16_t tpak_addr = JOYBUS_ACCESSORY_ADDR_TRANSFER_CART;
    tpak_addr += cart_addr % JOYBUS_ACCESSORY_TRANSFER_BANK_SIZE;
    accessory->transfer_pak_io = (joypad_transfer_pak_io_t){
        .start = dst,
        .end = dst + len,
        .cursor = dst,
        .bank = bank,
        .cart_addr = cart_addr,
        .tpak_addr = tpak_addr,
    };

    accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_LOAD_STATUS_READ;
    accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
    accessory->retries = 0;
    joybus_accessory_read_async(
        port, JOYBUS_ACCESSORY_ADDR_TRANSFER_STATUS,
        joypad_transfer_pak_load_read_callback, (void *)port
    );
}

/**
 * @brief Callback for the accessory read commands used by #joypad_transfer_pak_store_async.
 * 
 * @param out_dwords Joybus output block
 * @param ctx Opaque pointer used to pass the Joypad port number
 */
static void joypad_transfer_pak_store_read_callback(uint64_t *out_dwords, void *ctx)
{
    const uint8_t *out_bytes = (void *)out_dwords;
    joypad_port_t port = (joypad_port_t)ctx;
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];
    volatile joypad_transfer_pak_io_t *io = &accessory->transfer_pak_io;
    joypad_accessory_state_t state = accessory->state;
    if (!joypad_accessory_state_is_transfer_storing(state))
    {
        return; // Unexpected accessory state!
    }

    const joybus_cmd_n64_accessory_read_port_t *cmd =
        (void *)&out_bytes[port + JOYBUS_COMMAND_METADATA_SIZE];
    joybus_callback_t retry_callback = joypad_transfer_pak_store_read_callback;
    if (joypad_accessory_check_read_crc_error(port, cmd, retry_callback, ctx))
    {
        return; // Accessory communication error!
    }
    else if (state == JOYPAD_ACCESSORY_STATE_TRANSFER_STORE_STATUS_READ)
    {
        joybus_transfer_pak_status_t status = { .raw = cmd->recv.data[0] };
        accessory->transfer_pak_status = status;
        if (!status.access || !status.power)
        {
            // The Game Boy cartridge is no longer accessible; bail!
            accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
            accessory->error = JOYPAD_ACCESSORY_ERROR_TRANSFER_PAK_STATUS_CHANGE;
        }
        else if (io->cursor < io->end)
        {
            // Proceed with writing; select a Transfer Pak data bank
            uint8_t write_data[JOYBUS_ACCESSORY_DATA_SIZE];
            memset(write_data, io->bank, sizeof(write_data));
            accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_STORE_BANK_WRITE;
            accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
            accessory->retries = 0;
            joybus_accessory_write_async(
                port, JOYBUS_ACCESSORY_ADDR_TRANSFER_BANK, write_data,
                joypad_transfer_pak_store_write_callback, ctx
            );
        }
        else
        {
            // Finished writing data
            accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
        }
    }
}

/**
 * @brief Callback for the accessory write commands used by #joypad_transfer_pak_store_async.
 * 
 * @param out_dwords Joybus output block
 * @param ctx Opaque pointer used to pass the Joypad port number
 */
static void joypad_transfer_pak_store_write_callback(uint64_t *out_dwords, void *ctx)
{
    const uint8_t *out_bytes = (void *)out_dwords;
    joypad_port_t port = (joypad_port_t)ctx;
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];
    volatile joypad_transfer_pak_io_t *io = &accessory->transfer_pak_io;
    joypad_accessory_state_t state = accessory->state;
    if (!joypad_accessory_state_is_transfer_storing(state))
    {
        return; // Unexpected accessory state!
    }

    const joybus_cmd_n64_accessory_write_port_t *cmd =
        (void *)&out_bytes[port + JOYBUS_COMMAND_METADATA_SIZE];
    joybus_callback_t retry_callback = joypad_transfer_pak_store_write_callback;
    if (joypad_accessory_write_crc_error_check(port, cmd, retry_callback, ctx))
    {
        return; // Accessory communication error!
    }
    else if (state == JOYPAD_ACCESSORY_STATE_TRANSFER_STORE_BANK_WRITE)
    {
        // Continue writing data
        accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_STORE_DATA_WRITE;
        accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
        accessory->retries = 0;
        joybus_accessory_write_async(
            port, io->tpak_addr, io->cursor,
            joypad_transfer_pak_store_write_callback, ctx
        );
    }
    else if (state == JOYPAD_ACCESSORY_STATE_TRANSFER_STORE_DATA_WRITE)
    {
        uint8_t *cursor = io->cursor += JOYBUS_ACCESSORY_DATA_SIZE;
        uint16_t tpak_addr = io->tpak_addr += JOYBUS_ACCESSORY_DATA_SIZE;
        uint16_t cart_addr = io->cart_addr += JOYBUS_ACCESSORY_DATA_SIZE;
        int next_bank = cart_addr / JOYBUS_ACCESSORY_TRANSFER_BANK_SIZE;
        if (cursor >= io->end)
        {
            // Check the Transfer Pak status again after storing:
            // If the status says reset or cart pulled, you've got a problem!
            accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_STORE_STATUS_READ;
            accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
            accessory->retries = 0;
            joybus_accessory_read_async(
                port, JOYBUS_ACCESSORY_ADDR_TRANSFER_STATUS,
                joypad_transfer_pak_store_read_callback, ctx
            );
        }
        else if (next_bank == io->bank)
        {
            // Continue writing data
            accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_STORE_DATA_WRITE;
            accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
            accessory->retries = 0;
            joybus_accessory_write_async(
                port, tpak_addr, cursor,
                joypad_transfer_pak_store_write_callback, ctx
            );
        }
        else
        {
            // Switch to the next bank
            io->tpak_addr = JOYBUS_ACCESSORY_ADDR_TRANSFER_CART;
            io->bank = next_bank;
            uint8_t write_data[JOYBUS_ACCESSORY_DATA_SIZE];
            memset(write_data, next_bank, sizeof(write_data));
            accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_STORE_BANK_WRITE;
            accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
            accessory->retries = 0;
            joybus_accessory_write_async(
                port, JOYBUS_ACCESSORY_ADDR_TRANSFER_BANK, write_data,
                joypad_transfer_pak_store_write_callback, ctx
            );
        }
    }
}

void joypad_transfer_pak_store_async(joypad_port_t port, uint16_t cart_addr, void *src, size_t len)
{
    ASSERT_JOYPAD_PORT_VALID(port);
    assert(cart_addr % JOYBUS_ACCESSORY_DATA_SIZE == 0);
    assert(len % JOYBUS_ACCESSORY_DATA_SIZE == 0);
    volatile joypad_accessory_t *accessory = &joypad_accessories_hot[port];

    uint8_t bank = cart_addr / JOYBUS_ACCESSORY_TRANSFER_BANK_SIZE;
    uint16_t tpak_addr = JOYBUS_ACCESSORY_ADDR_TRANSFER_CART;
    tpak_addr += cart_addr % JOYBUS_ACCESSORY_TRANSFER_BANK_SIZE;
    accessory->transfer_pak_io = (joypad_transfer_pak_io_t){
        .start = src,
        .end = src + len,
        .cursor = src,
        .bank = bank,
        .cart_addr = cart_addr,
        .tpak_addr = tpak_addr,
    };

    accessory->state = JOYPAD_ACCESSORY_STATE_TRANSFER_STORE_STATUS_READ;
    accessory->error = JOYPAD_ACCESSORY_ERROR_PENDING;
    accessory->retries = 0;
    joybus_accessory_read_async(
        port, JOYBUS_ACCESSORY_ADDR_TRANSFER_STATUS,
        joypad_transfer_pak_store_read_callback, (void *)port
    );
}

/** @} */ /* joypad */
