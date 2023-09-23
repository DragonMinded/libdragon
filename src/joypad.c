/**
 * @file joypad.c
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joypad Subsystem
 * @ingroup joypad
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "debug.h"
#include "interrupt.h"
#include "joypad_internal.h"

/**
 * @addtogroup joypad
 * @{
 */

/**
 * @brief Number of ticks between Joybus identify commands.
 *
 * During VI interrupt, the Joypad subsystem will periodically re-identify
 * the connected devices to check if the identifier has changed or if any
 * accessories have been connected/disconnected.
 */
#define JOYPAD_IDENTIFY_INTERVAL_TICKS TICKS_PER_SECOND

/** @brief Convenience macro to ensure Joypad subsystem is initialized. */
#define ASSERT_JOYPAD_INITIALIZED() \
    assertf(joypad_init_refcount > 0, "joypad_init() was not called")

/**
 * @anchor joypad_hot_state
 * @name "Hot" (interrupt-driven) global state
 * @{
 */
/** @brief Timer ticks when the Joypads were last identified. */
static volatile uint32_t joypad_identify_last_ticks = 0;
/** @brief Are we in the middle of identifying the Joypads? */
static volatile bool joypad_identify_pending = false;
/** @brief Is the cached Joybus input block for identifying Joypads valid? */
static volatile uint8_t joypad_identify_input_valid = false;
/** @brief Cached Joybus input block for identifying all Joypads. */
static volatile uint8_t joypad_identify_input[JOYBUS_BLOCK_SIZE] = {0};

/** @brief Are we in the middle of reading the Joypads? */
static volatile bool joypad_read_pending = false;
/** @brief Is the cached Joybus input block for reading Joypads valid? */
static volatile bool joypad_read_input_valid = false;
/** @brief Cached offsets of Joypad read commands for each port in #joypad_read_input. */
static volatile size_t joypad_read_input_offsets[JOYPAD_PORT_COUNT] = {0};
/** @brief Cached Joybus input block for reading Joypads. */
static volatile uint8_t joypad_read_input[JOYBUS_BLOCK_SIZE] = {0};
/** @brief Cached Joybus output block for reading Joypads. */
static volatile uint8_t joypad_read_output[JOYBUS_BLOCK_SIZE] = {0};

/** @brief Are we in the middle of reading GameCube origins? */
static volatile bool joypad_gcn_origin_pending = false;
/** @brief Is the cached Joybus input block for reading GameCube origins valid? */
static volatile bool joypad_gcn_origin_input_valid = false;
/** @brief Cached Joybus input block for reading GameCube origins. */
static volatile uint8_t joypad_gcn_origin_input[JOYBUS_BLOCK_SIZE] = {0};

/** @brief Joypad identifiers for each port. */
volatile joybus_identifier_t joypad_identifiers_hot[JOYPAD_PORT_COUNT] = {0};
/** @brief Joypad "hot" devices for each port. */
volatile joypad_device_hot_t joypad_devices_hot[JOYPAD_PORT_COUNT] = {0};
/** @brief Joypad origins for each port. */
volatile joypad_gcn_origin_t joypad_origins_hot[JOYPAD_PORT_COUNT] = {0};
/** @brief Joypad accessories for each port. */
volatile joypad_accessory_t  joypad_accessories_hot[JOYPAD_PORT_COUNT] = {0};
/** @} */ /* joypad_hot_state */

/**
 * @anchor joypad_cold_state
 * @name "Cold" (non-volatile) global state
 * @{
 */
/** @brief Refcount of #joypad_init vs #joypad_close calls. */
static int joypad_init_refcount = 0;
/** @brief Joypad "cold" devices for each port. */
static joypad_device_cold_t joypad_devices_cold[JOYPAD_PORT_COUNT] = {0};
/** @} */ /* joypad_cold_state */

/**
 * @brief Reinitialize the state of a Joypad device when its identifier changes.
 * 
 * @param port Joypad port to reinitialize.
 * @param identifier New Joypad identifier for the port.
 */
static void joypad_device_changed(joypad_port_t port, joybus_identifier_t identifier)
{
    timer_link_t *timer = joypad_accessories_hot[port].transfer_pak_wait_timer;
    if (timer) { stop_timer(timer); }

    joypad_identifiers_hot[port] = identifier;
    joypad_origins_hot[port] = JOYPAD_GCN_ORIGIN_INIT;
    memset((void *)&joypad_devices_cold[port], 0, sizeof(joypad_devices_cold[port]));
    memset((void *)&joypad_devices_hot[port], 0, sizeof(joypad_devices_hot[port]));
    memset((void *)&joypad_accessories_hot[port], 0, sizeof(joypad_accessories_hot[port]));

    // Restore the timer pointer on the cleared accessory
    joypad_accessories_hot[port].transfer_pak_wait_timer = timer;
}

/**
 * @brief Convert a "N64 Controller Read" command response to Joypad inputs.
 * 
 * @param[in] cmd "N64 Controller Read" command response.
 * @return Joypad inputs structure (#joypad_inputs_t).
 */
static joypad_inputs_t joypad_inputs_from_n64_controller_read(
    const joybus_cmd_n64_controller_read_port_t *cmd
)
{
    // Emulate analog C-stick based on digital C-buttons
    int c_x_direction = cmd->recv.c_right - cmd->recv.c_left;
    int c_y_direction = cmd->recv.c_down - cmd->recv.c_up;
    int cstick_x = c_x_direction * JOYPAD_RANGE_GCN_CSTICK_MAX;
    int cstick_y = c_y_direction * JOYPAD_RANGE_GCN_CSTICK_MAX;

    // Emulate analog triggers based on digital shoulder buttons
    int analog_l = cmd->recv.l ? JOYPAD_RANGE_GCN_TRIGGER_MAX : 0;
    int analog_r = cmd->recv.r ? JOYPAD_RANGE_GCN_TRIGGER_MAX : 0;

    return (joypad_inputs_t){
        .btn = {
            .a = cmd->recv.a,
            .b = cmd->recv.b,
            .z = cmd->recv.z,
            .start = cmd->recv.start,
            .d_up = cmd->recv.d_up,
            .d_down = cmd->recv.d_down,
            .d_left = cmd->recv.d_left,
            .d_right = cmd->recv.d_right,
            .y = 0,
            .x = 0,
            .l = cmd->recv.l,
            .r = cmd->recv.r,
            .c_up = cmd->recv.c_up,
            .c_down = cmd->recv.c_down,
            .c_left = cmd->recv.c_left,
            .c_right = cmd->recv.c_right,
        },
        .stick_x = cmd->recv.stick_x,
        .stick_y = cmd->recv.stick_y,
        .cstick_x = cstick_x,
        .cstick_y = cstick_y,
        .analog_l = analog_l,
        .analog_r = analog_r,
    };
}

/**
 * @brief Convert a "GameCube Controller Read" command response to Joypad inputs.
 * 
 * @param[in] cmd "GameCube Controller Read" command response.
 * @param[in] origin Pointer to GameCube controller origin structure (or NULL for defaults).
 * @return Joypad inputs structure (#joypad_inputs_t).
 */
static joypad_inputs_t joypad_inputs_from_gcn_controller_read(
    const joybus_cmd_gcn_controller_read_port_t *cmd, 
    const joypad_gcn_origin_t *origin
)
{
    // Use default origin if none is provided
    static const joypad_gcn_origin_t default_origin = JOYPAD_GCN_ORIGIN_INIT;
    if ( origin == NULL ) { origin = &default_origin; }

    // Emulate directional C-buttons based on C-stick position
    static const int cstick_threshold = JOYPAD_RANGE_GCN_CSTICK_MAX / 2;

    // Bias the analog values with the corresponding origin
    int stick_x = CLAMP_ANALOG_STICK(cmd->recv.stick_x - origin->stick_x);
    int stick_y = CLAMP_ANALOG_STICK(cmd->recv.stick_y - origin->stick_y);
    int cstick_x = CLAMP_ANALOG_STICK(cmd->recv.cstick_x - origin->cstick_x);
    int cstick_y = CLAMP_ANALOG_STICK(cmd->recv.cstick_y - origin->cstick_y);
    int analog_l = CLAMP_ANALOG_TRIGGER(cmd->recv.analog_l - origin->analog_l);
    int analog_r = CLAMP_ANALOG_TRIGGER(cmd->recv.analog_r - origin->analog_r);

    return (joypad_inputs_t){
        .btn = {
            .a = cmd->recv.a,
            .b = cmd->recv.b,
            .z = cmd->recv.z,
            .start = cmd->recv.start,
            .d_up    = cmd->recv.d_up,
            .d_down  = cmd->recv.d_down,
            .d_left  = cmd->recv.d_left,
            .d_right = cmd->recv.d_right,
            .y = cmd->recv.y,
            .x = cmd->recv.x,
            .l = cmd->recv.l,
            .r = cmd->recv.r,
            .c_up    = cstick_y > +cstick_threshold,
            .c_down  = cstick_y < -cstick_threshold,
            .c_left  = cstick_x < -cstick_threshold,
            .c_right = cstick_x > +cstick_threshold,
        },
        .stick_x = stick_x,
        .stick_y = stick_y,
        .cstick_x = cstick_x,
        .cstick_y = cstick_y,
        .analog_l = analog_l,
        .analog_r = analog_r,
    };
}

/**
 * @brief Toggle the rumble state of a GameCube controller.
 * 
 * @param port Joypad port of the GameCube controller.
 * @param active Whether to enable (true) or disable (false) rumble motors.
 */
static void joypad_gcn_controller_rumble_toggle(joypad_port_t port, bool active)
{
    volatile joypad_device_hot_t *device = &joypad_devices_hot[port];
    device->rumble_active = active;
    if (joypad_read_input_valid)
    {
        // Set rumble active flag on cached GameCube controller read command
        const size_t cmd_offset = joypad_read_input_offsets[port];
        const size_t send_len_offset = cmd_offset + JOYBUS_COMMAND_OFFSET_SEND_LEN;
        joybus_cmd_gcn_controller_read_port_t *cmd;
        assert(joypad_read_input[send_len_offset] == sizeof(cmd->send));
        cmd = (void *)&joypad_read_input[cmd_offset + JOYBUS_COMMAND_METADATA_SIZE];
        assert(cmd->send.command == JOYBUS_COMMAND_ID_GCN_CONTROLLER_READ);
        cmd->send.rumble = active;
    }
}

/**
 * @brief Callback for reading GameCube controller origins.
 * 
 * @param[in] out_dwords Joybus output block.
 * @param[in,out] ctx Not used.
 */
static void joypad_gcn_origin_callback(uint64_t *out_dwords, void *ctx)
{
    const uint8_t *out_bytes = (void *)out_dwords;
    const joybus_cmd_gcn_controller_origin_port_t *cmd;
    size_t i = 0;

    JOYPAD_PORT_FOREACH (port)
    {
        // Check send_len to figure out if this port has a command on it
        if (out_bytes[i + JOYBUS_COMMAND_OFFSET_SEND_LEN] == 0)
        {
            // Skip this port
            i += JOYBUS_COMMAND_SKIP_SIZE;
        }
        else if (joypad_devices_hot[port].style != JOYPAD_STYLE_GCN)
        {
            // Skip this port
            i += JOYBUS_COMMAND_METADATA_SIZE + sizeof(*cmd);
        }
        else
        {
            cmd = (void *)&out_bytes[i + JOYBUS_COMMAND_METADATA_SIZE];
            i += JOYBUS_COMMAND_METADATA_SIZE + sizeof(*cmd);

            joypad_origins_hot[port] = (joypad_gcn_origin_t){
                .stick_x = cmd->recv.stick_x,
                .stick_y = cmd->recv.stick_y,
                .cstick_x = cmd->recv.cstick_x,
                .cstick_y = cmd->recv.cstick_y,
                .analog_l = cmd->recv.analog_l,
                .analog_r = cmd->recv.analog_r,
            };
        }
    }

    joypad_gcn_origin_pending = false;
}

/**
 * @brief Read GameCube controller origins asynchronously.
 */
static void joypad_gcn_origin_check_async(void)
{
    // Bail if this operation is already in-progress
    if (joypad_gcn_origin_pending) { return; }
    joypad_gcn_origin_pending = true;

    uint8_t * const input = (void *)joypad_gcn_origin_input;
    if (!joypad_gcn_origin_input_valid)
    {
        const joybus_cmd_gcn_controller_origin_port_t cmd = { .send = {
            .command = JOYBUS_COMMAND_ID_GCN_CONTROLLER_ORIGIN,
        }};
        const size_t recv_offset = offsetof(typeof(cmd), recv);
        size_t i = 0;

        // Populate the Joybus commands on each port
        memset(input, 0, JOYBUS_BLOCK_SIZE);
        JOYPAD_PORT_FOREACH (port)
        {
            // Set the command metadata
            input[i++] = sizeof(cmd.send);
            input[i++] = sizeof(cmd.recv);
            // Micro-optimization: Minimize copy length
            memcpy(&input[i], &cmd, recv_offset);
            i += sizeof(cmd);
        }

        // Close out the Joybus operation block
        input[i] = 0xFE;
        input[JOYBUS_BLOCK_SIZE - 1] = 0x01;

        joypad_gcn_origin_input_valid = true;
    }

    joybus_exec_async(input, joypad_gcn_origin_callback, NULL);
}

/**
 * @brief Callback for identifying Joypads.
 * 
 * @param[in] out_dwords Joybus output block.
 * @param[in,out] ctx Not used.
 */
static void joypad_identify_callback(uint64_t *out_dwords, void *ctx)
{
    const uint8_t *out_bytes = (void *)out_dwords;
    const joybus_cmd_identify_port_t *cmd;
    volatile joypad_device_hot_t *device;
    volatile joypad_accessory_t *accessory;
    bool devices_changed = false;
    size_t i = 0;

    JOYPAD_PORT_FOREACH (port)
    {
        device = &joypad_devices_hot[port];
        accessory = &joypad_accessories_hot[port];
        cmd = (void *)&out_bytes[i + JOYBUS_COMMAND_METADATA_SIZE];
        i += JOYBUS_COMMAND_METADATA_SIZE + sizeof(*cmd);

        joybus_identifier_t identifier = cmd->recv.identifier;
        if (joypad_identifiers_hot[port] != identifier)
        {
            // The identifier has changed; reinitialize device state
            joypad_device_changed(port, identifier);
            devices_changed = true;
        }

        if (
            (identifier & JOYBUS_IDENTIFIER_MASK_PLATFORM) == JOYBUS_IDENTIFIER_PLATFORM_GCN &&
            (identifier & JOYBUS_IDENTIFIER_MASK_GCN_CONTROLLER)
        )
        {
            device->style = JOYPAD_STYLE_GCN;
            bool has_rumble = !(identifier & JOYBUS_IDENTIFIER_MASK_GCN_NORUMBLE);
            device->rumble_method = has_rumble
              ? JOYPAD_RUMBLE_METHOD_GCN_CONTROLLER
              : JOYPAD_RUMBLE_METHOD_NONE;
            device->rumble_active = has_rumble && device->rumble_active;
        }
        else if (identifier == JOYBUS_IDENTIFIER_N64_CONTROLLER)
        {
            device->style = JOYPAD_STYLE_N64;
            uint8_t prev_accessory_status = accessory->status;
            uint8_t accessory_status = cmd->recv.status & JOYBUS_IDENTIFY_STATUS_ACCESSORY_MASK;
            // Work-around third-party controllers that don't correctly report accessory status
            bool accessory_absent = (
                accessory_status == JOYBUS_IDENTIFY_STATUS_ACCESSORY_ABSENT ||
                accessory_status == JOYBUS_IDENTIFY_STATUS_ACCESSORY_UNSUPPORTED
            );
            bool accessory_changed = (
                accessory_status == JOYBUS_IDENTIFY_STATUS_ACCESSORY_CHANGED ||
                (
                    accessory_status == JOYBUS_IDENTIFY_STATUS_ACCESSORY_PRESENT &&
                    prev_accessory_status != JOYBUS_IDENTIFY_STATUS_ACCESSORY_PRESENT &&
                    prev_accessory_status != JOYBUS_IDENTIFY_STATUS_ACCESSORY_CHANGED
                )
            );
            if (accessory_absent || accessory_changed)
            {
                accessory->state = JOYPAD_ACCESSORY_STATE_IDLE;
                accessory->type = JOYPAD_ACCESSORY_TYPE_NONE;
                device->rumble_method = JOYPAD_RUMBLE_METHOD_NONE;
                device->rumble_active = false;
            }
            if (accessory_changed)
            {
                accessory->type = JOYPAD_ACCESSORY_TYPE_UNKNOWN;
                joypad_accessory_detect_async(port);
            }
            accessory->status = accessory_status;
        }
        else if (identifier == JOYBUS_IDENTIFIER_N64_MOUSE)
        {
            device->style = JOYPAD_STYLE_MOUSE;
        }
    }

    if (devices_changed) joypad_read_input_valid = false;
    joypad_identify_last_ticks = TICKS_READ();
    joypad_identify_pending = false;
}

/**
 * @brief Identify Joypads asynchronously.
 * 
 * @param reset Whether to reset the devices.
 */
static void joypad_identify_async(bool reset)
{
    // Bail if this operation is already in-progress
    if (joypad_identify_pending) { return; }
    joypad_identify_pending = true;

    uint8_t * const input = (void *)joypad_identify_input;
    // Reset invalidates the cached input block
    if (!joypad_identify_input_valid || reset)
    {
        const joybus_cmd_identify_port_t cmd = { .send = {
            .command = reset ? JOYBUS_COMMAND_ID_RESET : JOYBUS_COMMAND_ID_IDENTIFY,
        } };
        const size_t recv_offset = offsetof(typeof(cmd), recv);
        size_t i = 0;

        // Populate the Joybus commands on each port
        memset(input, 0, JOYBUS_BLOCK_SIZE);
        JOYPAD_PORT_FOREACH (port)
        {
            // Set the command metadata
            input[i++] = sizeof(cmd.send);
            input[i++] = sizeof(cmd.recv);
            // Micro-optimization: Minimize copy length
            memcpy(&input[i], &cmd, recv_offset);
            i += sizeof(cmd);
        }

        // Close out the Joybus operation block
        input[i] = 0xFE;
        input[JOYBUS_BLOCK_SIZE - 1] = 0x01;

        // Identify is more common than reset, so don't cache resets
        joypad_identify_input_valid = !reset;
    }

    joybus_exec_async(input, joypad_identify_callback, NULL);
}

/**
 * @brief Callback for reading Joypad inputs.
 * 
 * @param[in] out_dwords Joybus output block.
 * @param[in,out] ctx Not used.
 */
static void joypad_read_callback(uint64_t *out_dwords, void *ctx)
{
    memcpy((void *)joypad_read_output, out_dwords, JOYBUS_BLOCK_SIZE);
    joypad_read_pending = false;
}

/**
 * @brief Read Joypad inputs asynchronously.
 */
static void joypad_read_async(void)
{
    // Bail if this operation is already in-progress
    if (joypad_read_pending) { return; }
    joypad_read_pending = true;

    uint8_t * const input = (void *)joypad_read_input;
    if (!joypad_read_input_valid)
    {
        volatile joypad_device_hot_t *device;
        joybus_identifier_t identifier;
        size_t i = 0;

        // Populate Joybus controller read commands on each port
        memset(input, 0, JOYBUS_BLOCK_SIZE);
        JOYPAD_PORT_FOREACH (port)
        {
            joypad_read_input_offsets[port] = i;
            device = &joypad_devices_hot[port];
            identifier = joypad_identifiers_hot[port];

             if (device->style == JOYPAD_STYLE_GCN)
            {
                const joybus_cmd_gcn_controller_read_port_t cmd = { .send = {
                    .command = JOYBUS_COMMAND_ID_GCN_CONTROLLER_READ,
                    .mode = 3, // Most-compatible analog mode
                    .rumble = device->rumble_active,
                } };
                // Set the command metadata
                input[i++] = sizeof(cmd.send);
                input[i++] = sizeof(cmd.recv);
                // Micro-optimization: Minimize copy length
                const size_t recv_offset = offsetof(typeof(cmd), recv);
                memcpy(&input[i], &cmd, recv_offset);
                i += sizeof(cmd);
            }
            else if (
                identifier == JOYBUS_IDENTIFIER_N64_CONTROLLER ||
                identifier == JOYBUS_IDENTIFIER_N64_MOUSE
            )
            {
                const joybus_cmd_n64_controller_read_port_t cmd = { .send = {
                    .command = JOYBUS_COMMAND_ID_N64_CONTROLLER_READ,
                } };
                // Set the command metadata
                input[i++] = sizeof(cmd.send);
                input[i++] = sizeof(cmd.recv);
                // Micro-optimization: Minimize copy length
                const size_t recv_offset = offsetof(typeof(cmd), recv);
                memcpy(&input[i], &cmd, recv_offset);
                i += sizeof(cmd);
            }
            else
            {
                // Skip this port
                i += JOYBUS_COMMAND_SKIP_SIZE;
            }
        }

        // Close out the Joybus operation block
        input[i] = 0xFE;
        input[JOYBUS_BLOCK_SIZE - 1] = 0x01;

        joypad_read_input_valid = true;
    }

    joybus_exec_async(input, joypad_read_callback, NULL);
}

/**
 * @brief Callback for VI interrupt to read and identify Joypads.
 */
static void joypad_vi_interrupt_callback(void)
{
    joypad_read_async();
    int32_t ticks_since_identify = TICKS_SINCE(joypad_identify_last_ticks);
    if (ticks_since_identify > JOYPAD_IDENTIFY_INTERVAL_TICKS)
    {
        joypad_identify_async(false);
    }
}

/**
 * @brief Re-identify and reset all Joypads and wait for completion.
 */
static void joypad_reset(void)
{
    ASSERT_JOYPAD_INITIALIZED();
    // Wait for pending identify/reset operation to resolve
    while (joypad_identify_pending) { /* Spinlock */ }
    // Enqueue this identify/reset operation
    joypad_identify_async(true);
    // Wait for the operation to finish
    while (joypad_identify_pending) { /* Spinlock */ }
}

/**
 * @brief Read Joypad inputs and wait for completion.
 * 
 * Implicitly scans the read inputs to save you a step.
 */
static void joypad_read(void)
{
    ASSERT_JOYPAD_INITIALIZED();
    joypad_read_async();
    while (joypad_read_pending) { /* Spinlock */ }
    joypad_poll();
}

joypad_inputs_t joypad_read_n64_inputs(joypad_port_t port)
{

    joybus_cmd_n64_controller_read_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_N64_CONTROLLER_READ,
    } };
    joybus_exec_cmd_struct(port, cmd);

    return joypad_inputs_from_n64_controller_read(&cmd);
}

void joypad_init(void)
{
    // Just increment the refcount if already initialized
	if (joypad_init_refcount++ > 0) { return; }

    // Initialize the timer subsystem (or increment its refcount)
    timer_init();

    // Reinitialize all Joypad devices to unknown state
    JOYPAD_PORT_FOREACH (port)
    {
        joypad_device_changed(port, JOYBUS_IDENTIFIER_UNKNOWN);
    }

    // Ensure the Joypads are ready immediately after init returns
    joypad_reset();
    joypad_read();

    // Update the Joypads on VI interrupt
    register_VI_handler(joypad_vi_interrupt_callback);
}

void joypad_close(void)
{
    // Do nothing if there are still dangling references.
	if (--joypad_init_refcount > 0) { return; }

    // Stop updating the Joypads on VI interrupt
    unregister_VI_handler(joypad_vi_interrupt_callback);
    
    // Decrement the timer subsystem refcount (possibly closing it)
    timer_close();
}

void joypad_poll(void)
{
    ASSERT_JOYPAD_INITIALIZED();

    uint8_t output[JOYBUS_BLOCK_SIZE];
    joypad_gcn_origin_t origins[JOYPAD_PORT_COUNT];
    joybus_identifier_t identifiers[JOYPAD_PORT_COUNT];

    // Take a snapshot of the current "hot" state
    disable_interrupts();
    memcpy(output, (void *)joypad_read_output, sizeof(output));
    memcpy(origins, (void *)joypad_origins_hot, sizeof(origins));
    memcpy(identifiers, (void *)joypad_identifiers_hot, sizeof(identifiers));
    enable_interrupts();

    uint8_t send_len, recv_len, command_id, command_len;
    joypad_device_cold_t *device;
    bool check_origins = false;
    size_t i = 0;

    JOYPAD_PORT_FOREACH (port)
    {
        device = &joypad_devices_cold[port];
        // Check send_len to figure out if this port has a command on it
        send_len = output[i + JOYBUS_COMMAND_OFFSET_SEND_LEN];
        if (send_len == 0)
        {
            // Commands with send_len of 0 have no recv_len or command_id
            recv_len = 0;
            command_id = JOYBUS_COMMAND_ID_RESET;
            command_len = JOYBUS_COMMAND_SKIP_SIZE;
        }
        else
        {
            recv_len = output[i + JOYBUS_COMMAND_OFFSET_RECV_LEN];
            command_id = output[i + JOYBUS_COMMAND_OFFSET_COMMAND_ID];
            command_len = JOYBUS_COMMAND_METADATA_SIZE + send_len + recv_len;
        }

        if (command_id == JOYBUS_COMMAND_ID_N64_CONTROLLER_READ)
        {
            const joybus_cmd_n64_controller_read_port_t *cmd;
            cmd = (void *)&output[i + JOYBUS_COMMAND_METADATA_SIZE];
            i += JOYBUS_COMMAND_METADATA_SIZE + sizeof(*cmd);

            // N64 Mouse uses the same read command as a Controller
            if (identifiers[port] == JOYBUS_IDENTIFIER_N64_MOUSE)
            {
                device->style = JOYPAD_STYLE_MOUSE;
            }
            else
            {
                device->style = JOYPAD_STYLE_N64;
            }

            device->previous = device->current;
            device->current = joypad_inputs_from_n64_controller_read(cmd);
        }
        else if (command_id == JOYBUS_COMMAND_ID_GCN_CONTROLLER_READ)
        {
            // Normalize GameCube controller read response
            const joybus_cmd_gcn_controller_read_port_t *cmd;
            cmd = (void *)&output[i + JOYBUS_COMMAND_METADATA_SIZE];
            i += JOYBUS_COMMAND_METADATA_SIZE + sizeof(*cmd);

            if (cmd->recv.check_origin) { check_origins = true; }

            device->style = JOYPAD_STYLE_GCN;
            device->previous = device->current;
            device->current = joypad_inputs_from_gcn_controller_read(cmd, &origins[port]);
        }
        else
        {
            // No recognized joypad on this port
            memset(device, 0, sizeof(*device));
            i += command_len;
        }
    }

    if (check_origins) joypad_gcn_origin_check_async();
}

bool joypad_is_connected(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    joybus_identifier_t identifier = joypad_identifiers_hot[port];
    return (
        identifier != JOYBUS_IDENTIFIER_NONE &&
        identifier != JOYBUS_IDENTIFIER_UNKNOWN
    );
}

joybus_identifier_t joypad_get_identifier(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_identifiers_hot[port];
}

joypad_style_t joypad_get_style(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_devices_cold[port].style;
}

joypad_accessory_type_t joypad_get_accessory_type(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_accessories_hot[port].type;
}

int joypad_get_accessory_state(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_accessories_hot[port].state;
}

int joypad_get_accessory_error(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_accessories_hot[port].error;
}

uint8_t joypad_get_transfer_pak_status(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_accessories_hot[port].transfer_pak_status.raw;
}

bool joypad_get_rumble_supported(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_devices_hot[port].rumble_method != JOYPAD_RUMBLE_METHOD_NONE;
}

bool joypad_get_rumble_active(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_devices_hot[port].rumble_active;
}

void joypad_set_rumble_active(joypad_port_t port, bool active)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    disable_interrupts();
    volatile joypad_device_hot_t *device = &joypad_devices_hot[port];
    joypad_rumble_method_t rumble_method = device->rumble_method;
    if (rumble_method == JOYPAD_RUMBLE_METHOD_N64_RUMBLE_PAK)
    {
        joypad_rumble_pak_toggle_async(port, active);
    }
    else if (rumble_method == JOYPAD_RUMBLE_METHOD_GCN_CONTROLLER)
    {
        joypad_gcn_controller_rumble_toggle(port, active);
    }
    enable_interrupts();
}

joypad_inputs_t joypad_get_inputs(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_devices_cold[port].current;
}


joypad_buttons_t joypad_get_buttons(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_devices_cold[port].current.btn;
}

joypad_buttons_t joypad_get_buttons_pressed(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    const uint16_t current = joypad_devices_cold[port].current.btn.raw;
    const uint16_t previous = joypad_devices_cold[port].previous.btn.raw;
    return (joypad_buttons_t){ .raw = current & ~previous };
}

joypad_buttons_t joypad_get_buttons_released(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    const uint16_t current = joypad_devices_cold[port].current.btn.raw;
    const uint16_t previous = joypad_devices_cold[port].previous.btn.raw;
    return (joypad_buttons_t){ .raw = ~(current & previous) };
}

joypad_buttons_t joypad_get_buttons_held(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    const uint16_t current = joypad_devices_cold[port].current.btn.raw;
    const uint16_t previous = joypad_devices_cold[port].previous.btn.raw;
    return (joypad_buttons_t){ .raw = current & previous };
}

/**
 * @brief Get the analog values for a Joypad port's axis.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * @param axis Offset of the axis in the #joypad_inputs_t structure
 * @param[out] current Current analog value for the axis
 * @param[out] previous Analog value for the axis on the previous frame
 * @param[out] threshold Analog value that must be exceeded to be considered a "press"
 */
static void joypad_get_axis_values(
    joypad_port_t port, joypad_axis_t axis,
    int *current, int *previous, int *threshold
)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    assert(axis < sizeof(joypad_inputs_t));
    if (current != NULL)
    {
        void *current_inputs = (void *)&joypad_devices_cold[port].current;
        *current = *(int8_t *)(current_inputs + axis);
    }
    if (previous != NULL)
    {
        void *previous_inputs = (void *)&joypad_devices_cold[port].previous;
        *previous = *(int8_t *)(previous_inputs + axis);
    }
    if (threshold != NULL)
    {
        if ( axis == JOYPAD_AXIS_STICK_X || axis == JOYPAD_AXIS_STICK_Y )
        {
            joypad_style_t style = joypad_devices_cold[port].style;
            if ( style == JOYPAD_STYLE_GCN )
            {
                *threshold = JOYPAD_RANGE_GCN_STICK_MAX / 2;
            }
            else
            {
                *threshold = JOYPAD_RANGE_N64_STICK_MAX / 2;
            }
        }
        else if ( axis == JOYPAD_AXIS_CSTICK_X || axis == JOYPAD_AXIS_CSTICK_Y )
        {
            *threshold = JOYPAD_RANGE_GCN_CSTICK_MAX / 2;
        }
        else if ( axis == JOYPAD_AXIS_ANALOG_L || axis == JOYPAD_AXIS_ANALOG_R )
        {
            *threshold = JOYPAD_RANGE_GCN_TRIGGER_MAX / 2;
        }
    }
}

int joypad_get_axis_pressed(joypad_port_t port, joypad_axis_t axis)
{
    int current = 0, previous = 0, threshold = 0;
    joypad_get_axis_values(port, axis, &current, &previous, &threshold);
    if ( current > +threshold && previous <= +threshold) { return +1; }
    if ( current < -threshold && previous >= -threshold) { return -1; }
    return 0;
}

int joypad_get_axis_released(joypad_port_t port, joypad_axis_t axis)
{
    int current = 0, previous = 0, threshold = 0;
    joypad_get_axis_values(port, axis, &current, &previous, &threshold);
    if ( current <= +threshold && previous > +threshold) { return +1; }
    if ( current >= -threshold && previous < -threshold) { return -1; }
    return 0;
}

int joypad_get_axis_held(joypad_port_t port, joypad_axis_t axis)
{
    int current = 0, previous = 0, threshold = 0;
    joypad_get_axis_values(port, axis, &current, &previous, &threshold);
    if ( current > +threshold && previous > +threshold ) { return +1; }
    if ( current < -threshold && previous < -threshold ) { return -1; }
    return 0;
}

/**
 * @brief Get the 8-way direction for a Joypad port's analog stick.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad 8-way direction enumeration value (#joypad_8way_t)
 */
static joypad_8way_t joypad_get_stick_direction(joypad_port_t port)
{
    int x = 0, y = 0, x_threshold = 0, y_threshold = 0;
    joypad_get_axis_values(port, JOYPAD_AXIS_STICK_X, &x, NULL, &x_threshold);
    joypad_get_axis_values(port, JOYPAD_AXIS_STICK_Y, &y, NULL, &y_threshold);
    if ( y > +y_threshold && x < -x_threshold ) { return JOYPAD_8WAY_UP_LEFT; }
    if ( y > +y_threshold && x > +x_threshold ) { return JOYPAD_8WAY_UP_RIGHT; }
    if ( y < -y_threshold && x < -x_threshold ) { return JOYPAD_8WAY_DOWN_LEFT; }
    if ( y < -y_threshold && x > +x_threshold ) { return JOYPAD_8WAY_DOWN_RIGHT; }
    if ( x > +x_threshold )                     { return JOYPAD_8WAY_RIGHT; }
    if ( y > +y_threshold )                     { return JOYPAD_8WAY_UP; }
    if ( x < -x_threshold )                     { return JOYPAD_8WAY_LEFT; }
    if ( y < -y_threshold )                     { return JOYPAD_8WAY_DOWN; }
    return JOYPAD_8WAY_NONE;
}

/**
 * @brief Convert a bitfield of up/down/left/right bits into an 8-way direction.
 * 
 * @param raw2d #JOYPAD_RAW_2D bitfield
 * @return Joypad 8-way direction enumeration value (#joypad_8way_t) 
 */
static joypad_8way_t joypad_raw2d_to_8way(unsigned raw2d)
{
    switch (raw2d)
    {
        case JOYPAD_RAW_2D_UP   | JOYPAD_RAW_2D_LEFT:  return JOYPAD_8WAY_UP_LEFT;
        case JOYPAD_RAW_2D_UP   | JOYPAD_RAW_2D_RIGHT: return JOYPAD_8WAY_UP_RIGHT;
        case JOYPAD_RAW_2D_DOWN | JOYPAD_RAW_2D_LEFT:  return JOYPAD_8WAY_DOWN_LEFT;
        case JOYPAD_RAW_2D_DOWN | JOYPAD_RAW_2D_RIGHT: return JOYPAD_8WAY_DOWN_RIGHT;
        case JOYPAD_RAW_2D_RIGHT:                      return JOYPAD_8WAY_RIGHT;
        case JOYPAD_RAW_2D_UP:                         return JOYPAD_8WAY_UP;
        case JOYPAD_RAW_2D_LEFT:                       return JOYPAD_8WAY_LEFT;
        case JOYPAD_RAW_2D_DOWN:                       return JOYPAD_8WAY_DOWN;
        default:                                       return JOYPAD_8WAY_NONE;
    }
}

joypad_8way_t joypad_get_direction(joypad_port_t port, joypad_2d_t axes)
{
    joypad_8way_t dir;
    if (axes & JOYPAD_2D_STICK)
    {
        dir = joypad_get_stick_direction(port);
        if (dir != JOYPAD_8WAY_NONE) { return dir; }
    }
    if (axes & JOYPAD_2D_DPAD)
    {
        // Convert D-Pad up/down/left/right into a #JOYPAD_RAW_2D bitfield
        const unsigned raw2d = (joypad_get_buttons(port).raw & 0x0F00) >> 8;
        dir = joypad_raw2d_to_8way(raw2d);
        if (dir != JOYPAD_8WAY_NONE) { return dir; }
    }
    if (axes & JOYPAD_2D_C)
    {
        // Convert C-up/down/left/right into a #JOYPAD_RAW_2D bitfield
        const unsigned raw2d = joypad_get_buttons(port).raw & 0x000F;
        dir = joypad_raw2d_to_8way(raw2d);
        if (dir != JOYPAD_8WAY_NONE) { return dir; }
    }
    return JOYPAD_8WAY_NONE;
}

/** @} */ /* joypad */
