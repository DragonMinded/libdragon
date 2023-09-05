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
 * @defgroup joypad Joypad Subsystem
 * @ingroup peripherals
 * @brief Joypad abstraction interface.
 *
 * The Joypad subsystem is the successor to the LibDragon Controller subsystem.
 * The Joypad subsystem is in charge of communication with the controller ports
 * and provides a common interface to support a variety of input devices:
 *
 * * Nintendo 64 controller
 * * Nintendo 64 mouse
 * * GameCube controller (with a passive adapter)
 *
 * To use a Joypad, the developer must first call #joypad_init. Once initialized,
 * The Joypad subsystem will automatically identify and read all connected input
 * devices once per frame.
 *
 * To refer to individual ports, use the #joypad_port_t enumeration values.
 * To iterate across all ports, use the #JOYPAD_PORT_FOREACH macro.
 *
 * To read the controllers, first call #joypad_scan once per frame to process
 * the input data. #joypad_get_style will return which "style" of device is
 * connected to a port (#joypad_style_t). #joypad_get_inputs will return the
 * buttons and analog input state for a given controller port.
 *
 * Developers can determine whether the input device is capable of rumble by
 * calling #joypad_get_rumble_supported and then starting/stopping the rumble
 * motor by calling #joypad_set_rumble_active.
 *
 * The Joypad subsystem will automatically detect which accessory is connected
 * to Nintendo 64 controllers. Call #joypad_get_accessory_type to determine
 * which accessory was detected.
 *
 * For advanced use-cases, a developer can determine exactly which type of
 * input device is connected by calling #joypad_get_identifier, which will
 * return the 16-bit device identifier value from the Joybus "Info" response.
 * 
 * To read digital button state for a Joypad device:
 * * #joypad_get_buttons
 * * #joypad_get_buttons_pressed
 * * #joypad_get_buttons_released
 * * #joypad_get_buttons_held
 * 
 * To read 8-way directional state for a Joypad device:
 * * #joypad_get_stick_direction
 * * #joypad_get_dpad_direction
 * * #joypad_get_c_direction
 * 
 * To read analog directions as digital inputs for a Joypad device:
 * * #joypad_get_axis_pressed
 * * #joypad_get_axis_released
 * * #joypad_get_axis_held
 * 
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

/** @brief Convenience macro for ensuring Joypad subsystem is initialized. */
#define ASSERT_JOYPAD_INITIALIZED() \
    assertf(joypad_init_refcount > 0, "joypad_init() was not called")

/**
 * @anchor joypad_hot_state
 * @name "Hot" (interrupt-driven) global state
 * @{
 */
/** @brief Timer ticks when the Joypads were last identified. */
static volatile int64_t joypad_identify_last_ticks = 0;
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
 * @brief Reset the state of a Joypad device.
 * 
 * @param port Joypad port to reset.
 * @param identifier New Joypad identifier for the port.
 */
static void joypad_device_reset(joypad_port_t port, joybus_identifier_t identifier)
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
 * @param[in] recv_cmd "N64 Controller Read" command response.
 * @return Joypad inputs structure (#joypad_inputs_t).
 */
static joypad_inputs_t joypad_inputs_from_n64_controller_read(
    const joybus_cmd_n64_controller_read_port_t *recv_cmd
)
{
    // Emulate analog C-stick based on digital C-buttons
    int c_x_direction = recv_cmd->c_right - recv_cmd->c_left;
    int c_y_direction = recv_cmd->c_down - recv_cmd->c_up;
    int cstick_x = c_x_direction * JOYPAD_RANGE_GCN_CSTICK_MAX;
    int cstick_y = c_y_direction * JOYPAD_RANGE_GCN_CSTICK_MAX;

    // Emulate analog triggers based on digital shoulder buttons
    int analog_l = recv_cmd->l ? JOYPAD_RANGE_GCN_TRIGGER_MAX : 0;
    int analog_r = recv_cmd->r ? JOYPAD_RANGE_GCN_TRIGGER_MAX : 0;

    return (joypad_inputs_t){
        .a = recv_cmd->a,
        .b = recv_cmd->b,
        .z = recv_cmd->z,
        .start = recv_cmd->start,
        .d_up = recv_cmd->d_up,
        .d_down = recv_cmd->d_down,
        .d_left = recv_cmd->d_left,
        .d_right = recv_cmd->d_right,
        .y = 0,
        .x = 0,
        .l = recv_cmd->l,
        .r = recv_cmd->r,
        .c_up = recv_cmd->c_up,
        .c_down = recv_cmd->c_down,
        .c_left = recv_cmd->c_left,
        .c_right = recv_cmd->c_right,
        .stick_x = recv_cmd->stick_x,
        .stick_y = recv_cmd->stick_y,
        .cstick_x = cstick_x,
        .cstick_y = cstick_y,
        .analog_l = analog_l,
        .analog_r = analog_r,
    };
}

/**
 * @brief Convert a "GameCube Controller Read" command response to Joypad inputs.
 * 
 * @param[in] recv_cmd "GameCube Controller Read" command response.
 * @param[in] origin Pointer to GameCube controller origin structure (or NULL for defaults).
 * @return Joypad inputs structure (#joypad_inputs_t).
 */
static joypad_inputs_t joypad_inputs_from_gcn_controller_read(
    const joybus_cmd_gcn_controller_read_port_t *recv_cmd, 
    const joypad_gcn_origin_t *origin
)
{
    // Use default origin if none is provided
    static const joypad_gcn_origin_t default_origin = JOYPAD_GCN_ORIGIN_INIT;
    if ( origin == NULL ) { origin = &default_origin; }

    // Emulate directional C-buttons based on C-stick position
    static const int cstick_threshold = JOYPAD_RANGE_GCN_CSTICK_MAX / 2;

    // Bias the analog values with the corresponding origin
    int stick_x = CLAMP_ANALOG_STICK(recv_cmd->stick_x - origin->stick_x);
    int stick_y = CLAMP_ANALOG_STICK(recv_cmd->stick_y - origin->stick_y);
    int cstick_x = CLAMP_ANALOG_STICK(recv_cmd->cstick_x - origin->cstick_x);
    int cstick_y = CLAMP_ANALOG_STICK(recv_cmd->cstick_y - origin->cstick_y);
    int analog_l = CLAMP_ANALOG_TRIGGER(recv_cmd->analog_l - origin->analog_l);
    int analog_r = CLAMP_ANALOG_TRIGGER(recv_cmd->analog_r - origin->analog_r);

    return (joypad_inputs_t){
        .a = recv_cmd->a,
        .b = recv_cmd->b,
        .z = recv_cmd->z,
        .start = recv_cmd->start,
        .d_up    = recv_cmd->d_up,
        .d_down  = recv_cmd->d_down,
        .d_left  = recv_cmd->d_left,
        .d_right = recv_cmd->d_right,
        .y = recv_cmd->y,
        .x = recv_cmd->x,
        .l = recv_cmd->l,
        .r = recv_cmd->r,
        .c_up    = cstick_y > +cstick_threshold,
        .c_down  = cstick_y < -cstick_threshold,
        .c_left  = cstick_x < -cstick_threshold,
        .c_right = cstick_x > +cstick_threshold,
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
        size_t cmd_offset = joypad_read_input_offsets[port];
        joybus_cmd_gcn_controller_read_port_t *read_cmd;
        read_cmd = (void *)&joypad_read_input[cmd_offset];
        assert(read_cmd->send_len == sizeof(read_cmd->send_bytes));
        assert(read_cmd->command == JOYBUS_COMMAND_ID_GCN_CONTROLLER_READ);
        read_cmd->rumble = active;
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
    const joybus_cmd_gcn_controller_origin_port_t *recv_cmd;
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
            i += sizeof(*recv_cmd);
        }
        else
        {
            recv_cmd = (void *)&out_bytes[i];
            i += sizeof(*recv_cmd);

            joypad_origins_hot[port] = (joypad_gcn_origin_t){
                .stick_x = recv_cmd->stick_x,
                .stick_y = recv_cmd->stick_y,
                .cstick_x = recv_cmd->cstick_x,
                .cstick_y = recv_cmd->cstick_y,
                .analog_l = recv_cmd->analog_l,
                .analog_r = recv_cmd->analog_r,
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
        const joybus_cmd_gcn_controller_origin_port_t send_cmd = {
            .send_len = sizeof(send_cmd.send_bytes),
            .recv_len = sizeof(send_cmd.recv_bytes),
            .command = JOYBUS_COMMAND_ID_GCN_CONTROLLER_ORIGIN,
        };
        const size_t recv_offset = offsetof(typeof(send_cmd), recv_bytes);
        size_t i = 0;

        // Populate the Joybus commands on each port
        memset(input, 0, JOYBUS_BLOCK_SIZE);
        JOYPAD_PORT_FOREACH (port)
        {
            // Micro-optimization: Minimize copy length
            memcpy(&input[i], &send_cmd, recv_offset);
            i += sizeof(send_cmd);
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
    const joybus_cmd_identify_port_t *recv_cmd;
    volatile joypad_device_hot_t *device;
    volatile joypad_accessory_t *accessory;
    bool devices_changed = false;
    size_t i = 0;

    JOYPAD_PORT_FOREACH (port)
    {
        device = &joypad_devices_hot[port];
        accessory = &joypad_accessories_hot[port];
        recv_cmd = (void *)&out_bytes[i];
        i += sizeof(*recv_cmd);

        joybus_identifier_t identifier = recv_cmd->identifier;
        if (joypad_identifiers_hot[port] != identifier)
        {
            // The identifier has changed; reset device state
            joypad_device_reset(port, identifier);
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
            uint8_t accessory_status = recv_cmd->status & JOYBUS_IDENTIFY_STATUS_ACCESSORY_MASK;
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
    joypad_identify_last_ticks = timer_ticks();
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
        const joybus_cmd_identify_port_t send_cmd = {
            .send_len = sizeof(send_cmd.send_bytes),
            .recv_len = sizeof(send_cmd.recv_bytes),
            .command = reset ? JOYBUS_COMMAND_ID_RESET : JOYBUS_COMMAND_ID_IDENTIFY,
        };
        const size_t recv_offset = offsetof(typeof(send_cmd), recv_bytes);
        size_t i = 0;

        // Populate the Joybus commands on each port
        memset(input, 0, JOYBUS_BLOCK_SIZE);
        JOYPAD_PORT_FOREACH (port)
        {
            // Micro-optimization: Minimize copy length
            memcpy(&input[i], &send_cmd, recv_offset);
            i += sizeof(send_cmd);
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
                const joybus_cmd_gcn_controller_read_port_t send_cmd = {
                    .send_len = sizeof(send_cmd.send_bytes),
                    .recv_len = sizeof(send_cmd.recv_bytes),
                    .command = JOYBUS_COMMAND_ID_GCN_CONTROLLER_READ,
                    .mode = 3, // Most-compatible analog mode
                    .rumble = device->rumble_active,
                };
                // Micro-optimization: Minimize copy length
                const size_t recv_offset = offsetof(typeof(send_cmd), recv_bytes);
                memcpy(&input[i], &send_cmd, recv_offset);
                i += sizeof(send_cmd);
            }
            else if (
                identifier == JOYBUS_IDENTIFIER_N64_CONTROLLER ||
                identifier == JOYBUS_IDENTIFIER_N64_MOUSE
            )
            {
                const joybus_cmd_n64_controller_read_port_t send_cmd = {
                    .send_len = sizeof(send_cmd.send_bytes),
                    .recv_len = sizeof(send_cmd.recv_bytes),
                    .command = JOYBUS_COMMAND_ID_N64_CONTROLLER_READ,
                };
                // Micro-optimization: Minimize copy length
                const size_t recv_offset = offsetof(typeof(send_cmd), recv_bytes);
                memcpy(&input[i], &send_cmd, recv_offset);
                i += sizeof(send_cmd);
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
    if (joypad_identify_last_ticks + JOYPAD_IDENTIFY_INTERVAL_TICKS < timer_ticks())
    {
        joypad_identify_async(false);
    }
}

/**
 * @brief Synchronously read the inputs from a Nintendo 64 controller.
 * 
 * This function is intended for use in situations where interrupts may
 * be disabled or where joypad_init may not have been called.
 * 
 * @note This function is slow: it blocks for about 10% of a frame.
 *       To avoid this performance hit, use the managed function in
 *       the Joypad subsystem instead: #joypad_get_inputs
 * 
 * @param port Joypad port (#joypad_port_t) to read from.
 * @return Joypad inputs structure (#joypad_inputs_t)
 */
joypad_inputs_t joypad_read_n64_inputs_sync(joypad_port_t port)
{
    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    uint8_t output[JOYBUS_BLOCK_SIZE] = {0};
    size_t i = port;

    const joybus_cmd_n64_controller_read_port_t send_cmd = {
        .send_len = sizeof(send_cmd.send_bytes),
        .recv_len = sizeof(send_cmd.recv_bytes),
        .command = JOYBUS_COMMAND_ID_N64_CONTROLLER_READ,
    };
    // Micro-optimization: Minimize copy length
    const size_t recv_offset = offsetof(typeof(send_cmd), recv_bytes);
    memcpy(&input[i], &send_cmd, recv_offset);
    i += sizeof(send_cmd);

    // Close out the Joybus operation block
    input[i] = 0xFE;
    input[sizeof(input) - 1] = 0x01;

    joybus_exec(input, output);
    const joybus_cmd_n64_controller_read_port_t *recv_cmd =
        (void *)&output[port];

    return joypad_inputs_from_n64_controller_read(recv_cmd);
}

/**
 * @brief Initialize the Joypad subsystem.
 * 
 * Starts reading Joypads during VI interrupt.
 */
void joypad_init(void)
{
    // Just increment the refcount if already initialized
	if (joypad_init_refcount++ > 0) { return; }

    // Initialize the timer subsystem (or increment its refcount)
    timer_init();

    JOYPAD_PORT_FOREACH (port)
    {
        joypad_device_reset(port, JOYBUS_IDENTIFIER_UNKNOWN);
    }
    register_VI_handler(joypad_vi_interrupt_callback);
    joypad_identify_sync(true);
    joypad_read_sync();
}

/**
 * @brief Close the Joypad subsystem.
 * 
 * Stops reading Joypads during VI interrupt.
 */
void joypad_close(void)
{
    // Do nothing if there are still dangling references.
	if (--joypad_init_refcount > 0) { return; }

    unregister_VI_handler(joypad_vi_interrupt_callback);
    
    // Decrement the timer subsystem refcount (possibly closing it)
    timer_close();
}

/**
 * @brief Identify Joypads and wait for completion.
 * 
 * @param reset Whether to reset the devices.
 */
void joypad_identify_sync(bool reset)
{
    ASSERT_JOYPAD_INITIALIZED();
    // Wait for pending identify/reset operation to resolve
    while (joypad_identify_pending) { /* Spinlock */ }
    // Enqueue this identify/reset operation
    joypad_identify_async(reset);
    // Wait for the operation to finish
    while (joypad_identify_pending) { /* Spinlock */ }
}

/**
 * @brief Read Joypad inputs and wait for completion.
 * 
 * Implicitly scans the read inputs to save you a step.
 */
void joypad_read_sync(void)
{
    ASSERT_JOYPAD_INITIALIZED();
    joypad_read_async();
    while (joypad_read_pending) { /* Spinlock */ }
    joypad_scan();
}

/**
 * @brief Fetch the current Joypad input state.
 * 
 * This function must be called once per frame, or any time after the
 * Joypads have been read. After calling this function, you can read
 * the Joypad state using the following functions:
 * 
 * * #joypad_get_inputs
 * * #joypad_get_buttons
 * * #joypad_get_buttons_pressed
 * * #joypad_get_buttons_released
 * * #joypad_get_buttons_held
 * * #joypad_get_dpad_direction
 * * #joypad_get_c_direction
 * * #joypad_get_stick_direction
 * * #joypad_get_axis_pressed
 * * #joypad_get_axis_released
 * * #joypad_get_axis_held
 * 
 * This function is very fast. In fact, joypads are read in background
 * asynchronously under interrupt, so this function just synchronizes the
 * internal state.
 */
void joypad_scan(void)
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
            const joybus_cmd_n64_controller_read_port_t *recv_cmd;
            recv_cmd = (void *)&output[i];
            i += sizeof(*recv_cmd);

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
            device->current = joypad_inputs_from_n64_controller_read(recv_cmd);
        }
        else if (command_id == JOYBUS_COMMAND_ID_GCN_CONTROLLER_READ)
        {
            // Normalize GameCube controller read response
            const joybus_cmd_gcn_controller_read_port_t *recv_cmd;
            recv_cmd = (void *)&output[i];
            i += sizeof(*recv_cmd);

            if (recv_cmd->check_origin) check_origins = true;

            device->style = JOYPAD_STYLE_GCN;
            device->previous = device->current;
            device->current = joypad_inputs_from_gcn_controller_read(recv_cmd, &origins[port]);
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

/**
 * @brief Whether a Joybus device is plugged in to a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @retval true A Joybus device is connected to the Joypad port.
 * @retval false Nothing is connected to the Joypad port.
 */
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

/**
 * @brief Get the Joybus device identifier for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joybus device identifier (#joybus_identifier_t)
 */
joybus_identifier_t joypad_get_identifier(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_identifiers_hot[port];
}

/**
 * @brief Get the Joypad style for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad style enumeration value (#joypad_style_t)
 */
joypad_style_t joypad_get_style(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_devices_cold[port].style;
}

/**
 * @brief Get the Joypad accessory type for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad accessory type enumeration value (#joypad_accessory_type_t)
 */
joypad_accessory_type_t joypad_get_accessory_type(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_accessories_hot[port].type;
}

/**
 * @brief Get the Joypad accessory state for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad accessory state enumeration value 
 */
int joypad_get_accessory_state(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_accessories_hot[port].state;
}

/**
 * @brief Get the Joypad accessory error for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * @return Joypad accessory error enumeration value 
 */
int joypad_get_accessory_error(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_accessories_hot[port].error;
}

/**
 * @brief Get the Transfer Pak status byte for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Transfer Pak status byte 
 */
uint8_t joypad_get_accessory_transfer_pak_status(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_accessories_hot[port].transfer_pak_status.raw;
}

/**
 * @brief Is rumble supported for a Joypad port?
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Whether rumble is supported
 */
bool joypad_get_rumble_supported(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_devices_hot[port].rumble_method != JOYPAD_RUMBLE_METHOD_NONE;
}

/**
 * @brief Is rumble active for a Joypad port?
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Whether rumble is active
 */
bool joypad_get_rumble_active(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_devices_hot[port].rumble_active;
}

/**
 * @brief Activate or deactivate rumble on a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * @param active Whether rumble should be active
 */
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

/**
 * @brief Get the Joypad inputs for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad inputs structure (#joypad_inputs_t)
 */
joypad_inputs_t joypad_get_inputs(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_devices_cold[port].current;
}

/**
 * @brief Get the Joypad buttons for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad buttons structure (#joypad_buttons_t)
 */
joypad_buttons_t joypad_get_buttons(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    return joypad_devices_cold[port].current.buttons;
}

/**
 * @brief Get the Joypad buttons that were pressed since the last
 * time Joypads were read for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad buttons structure (#joypad_buttons_t)
 */
joypad_buttons_t joypad_get_buttons_pressed(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    const uint16_t current = joypad_devices_cold[port].current.buttons.raw;
    const uint16_t previous = joypad_devices_cold[port].previous.buttons.raw;
    return (joypad_buttons_t){ .raw = current & ~previous };
}

/**
 * @brief Get the Joypad buttons that were released since the last
 * time Joypads were read for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad buttons structure (#joypad_buttons_t)
 */
joypad_buttons_t joypad_get_buttons_released(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    const uint16_t current = joypad_devices_cold[port].current.buttons.raw;
    const uint16_t previous = joypad_devices_cold[port].previous.buttons.raw;
    return (joypad_buttons_t){ .raw = ~(current & previous) };
}

/**
 * @brief Get the Joypad buttons that are held down since the last
 * time Joypads were read for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad buttons structure (#joypad_buttons_t)
 */
joypad_buttons_t joypad_get_buttons_held(joypad_port_t port)
{
    ASSERT_JOYPAD_INITIALIZED();
    ASSERT_JOYPAD_PORT_VALID(port);
    const uint16_t current = joypad_devices_cold[port].current.buttons.raw;
    const uint16_t previous = joypad_devices_cold[port].previous.buttons.raw;
    return (joypad_buttons_t){ .raw = current & previous };
}

/**
 * @brief Get the 8-way direction for a Joypad port's D-pad.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad 8-way direction enumeration value (#joypad_8way_t)
 */
joypad_8way_t joypad_get_dpad_direction(joypad_port_t port)
{
    joypad_buttons_t buttons = joypad_get_buttons(port);
    if ( buttons.d_up && buttons.d_left )    { return JOYPAD_8WAY_UP_LEFT; }
    if ( buttons.d_up && buttons.d_right )   { return JOYPAD_8WAY_UP_RIGHT; }
    if ( buttons.d_down && buttons.d_left )  { return JOYPAD_8WAY_DOWN_LEFT; }
    if ( buttons.d_down && buttons.d_right ) { return JOYPAD_8WAY_DOWN_RIGHT; }
    if ( buttons.d_right )                   { return JOYPAD_8WAY_RIGHT; }
    if ( buttons.d_up )                      { return JOYPAD_8WAY_UP; }
    if ( buttons.d_left )                    { return JOYPAD_8WAY_LEFT; }
    if ( buttons.d_down )                    { return JOYPAD_8WAY_DOWN; }
    return JOYPAD_8WAY_NONE;
}

/**
 * @brief Get the 8-way direction for a Joypad port's C-buttons.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad 8-way direction enumeration value (#joypad_8way_t)
 */
joypad_8way_t joypad_get_c_direction(joypad_port_t port)
{
    joypad_buttons_t buttons = joypad_get_buttons(port);
    if ( buttons.c_up && buttons.c_left )    { return JOYPAD_8WAY_UP_LEFT; }
    if ( buttons.c_up && buttons.c_right )   { return JOYPAD_8WAY_UP_RIGHT; }
    if ( buttons.c_down && buttons.c_left )  { return JOYPAD_8WAY_DOWN_LEFT; }
    if ( buttons.c_down && buttons.c_right ) { return JOYPAD_8WAY_DOWN_RIGHT; }
    if ( buttons.c_right )                   { return JOYPAD_8WAY_RIGHT; }
    if ( buttons.c_up )                      { return JOYPAD_8WAY_UP; }
    if ( buttons.c_left )                    { return JOYPAD_8WAY_LEFT; }
    if ( buttons.c_down )                    { return JOYPAD_8WAY_DOWN; }
    return JOYPAD_8WAY_NONE;
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

/**
 * @brief Get the 8-way direction for a Joypad port's analog stick.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad 8-way direction enumeration value (#joypad_8way_t)
 */
joypad_8way_t joypad_get_stick_direction(joypad_port_t port)
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
 * @brief Get the direction of a "press" of an axis on a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * @param axis Joypad axis enumeration value (#joypad_axis_t)
 * 
 * @retval +1 Axis is pressed in the positive direction
 * @retval -1 Axis is pressed in the negative direction
 * @retval  0 Axis is not pressed
 */
int joypad_get_axis_pressed(joypad_port_t port, joypad_axis_t axis)
{
    int current = 0, previous = 0, threshold = 0;
    joypad_get_axis_values(port, axis, &current, &previous, &threshold);
    if ( current > +threshold && previous <= +threshold) { return +1; }
    if ( current < -threshold && previous >= -threshold) { return -1; }
    return 0;
}

/**
 * @brief Get the direction of a "release" of an axis on a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * @param axis Joypad axis enumeration value (#joypad_axis_t)
 * 
 * @retval +1 Axis was released in the positive direction
 * @retval -1 Axis was released in the negative direction
 * @retval  0 Axis is not released
 */
int joypad_get_axis_released(joypad_port_t port, joypad_axis_t axis)
{
    int current = 0, previous = 0, threshold = 0;
    joypad_get_axis_values(port, axis, &current, &previous, &threshold);
    if ( current <= +threshold && previous > +threshold) { return +1; }
    if ( current >= -threshold && previous < -threshold) { return -1; }
    return 0;
}

/**
 * @brief Get the direction that an axis is held on a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * @param axis Joypad axis enumeration value (#joypad_axis_t)
 * 
 * @retval +1 Axis is being held in the positive direction
 * @retval -1 Axis is being held in the negative direction
 * @retval  0 Axis is not being held
 */
int joypad_get_axis_held(joypad_port_t port, joypad_axis_t axis)
{
    int current = 0, previous = 0, threshold = 0;
    joypad_get_axis_values(port, axis, &current, &previous, &threshold);
    if ( current > +threshold && previous > +threshold ) { return +1; }
    if ( current < -threshold && previous < -threshold ) { return -1; }
    return 0;
}

/** @} */ /* joypad */
