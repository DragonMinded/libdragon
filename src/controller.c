/**
 * @file controller.c
 * @brief Controller Subsystem
 * @ingroup controller
 */

#include <stdbool.h>
#include <string.h>

#include "controller.h"
#include "interrupt.h"
#include "joybus_commands.h"
#include "joybus_internal.h"

/**
 * @defgroup controller Controller Subsystem
 * @ingroup libdragon
 * @brief Controller and accessory interface.
 * 
 * @deprecated This module is now deprecated.
 *             Please use the @ref joypad "Joypad Subsystem" instead.
 *
 * This module contains an old API to communicate with controllers and accessories.
 * The API had several deficiencies, notably implementing partial support for
 * GameCube controllers, but only supporting asynchronous reading of N64 controllers.
 * It has been deprecated in favor of the @ref joypad "Joypad Subsystem", which
 * offers full support for both N64 and GameCube controllers, and implements
 * asynchronous detection of controller port devices and accessories.
 * 
 * All controller subsystem functions are now implemented as thin compatibility shims
 * that call the corresponding @ref joypad "Joypad Subsystem" functions. They will
 * continue to work just as before, but there will be no further work on them.
 * The functions have all been explicitly marked as deprecated, and will generate a
 * warning at compile time. The warning suggests the alternative Joypad API to use
 * instead. In most cases, the change should be straightforward.
 *
 * @{
 */

/**************************************
 *  DEPRECATED FUNCTIONS 
 **************************************/

///@cond

/**
 * @brief Assign Joypad buttons to a controller data structure.
 * 
 * @param port Joypad port number (0-3)
 * @param[out] out Destination controller data structure
 * @param[in] buttons Joypad buttons data structure
 */
static inline void controller_data_from_joypad_buttons(
    int port,
    struct controller_data *out,
    const joypad_buttons_t *buttons
)
{
    struct SI_condat * c = &out->c[port];

    c->A = buttons->a;
    c->B = buttons->b;
    c->Z = buttons->z;
    c->start = buttons->start;
    c->up = buttons->d_up;
    c->down = buttons->d_down;
    c->left = buttons->d_left;
    c->right = buttons->d_right;
    c->L = buttons->l;
    c->R = buttons->r;
    c->C_up = buttons->c_up;
    c->C_down = buttons->c_down;
    c->C_left = buttons->c_left;
    c->C_right = buttons->c_right;
}

/**
 * @brief Read the controller button status for all N64 controllers
 *
 * Read the controller button status immediately and return results to data.
 * 
 * @note This function is slow: it blocks for about 10% of a frame time. To avoid
 *       this hit, use the managed functions (#get_keys_down, etc.).
 *
 * @param[out] output
 *             Structure to place the returned controller button status
 * 
 * @deprecated Use the @ref joypad "Joypad subsystem" instead of reading
 *             N64 controller inputs directly.
 */
void controller_read( struct controller_data * output )
{
    static const unsigned long long SI_read_con_block[8] =
    {
        0xff010401ffffffff,
        0xff010401ffffffff,
        0xff010401ffffffff,
        0xff010401ffffffff,
        0xfe00000000000000,
        0,
        0,
        1
    };

    joybus_exec( SI_read_con_block, output );
}

/**
 * @brief Read the controller button status for all controllers, GC version
 *
 * Read the controller button status immediately and return results to data.
 *
 * @param[out] outdata
 *             Structure to place the returned controller button status
 * @param[in]  rumble
 *             Set to 1 to start rumble, 0 to stop it.
 *
 * @deprecated Use the @ref joypad "Joypad subsystem" instead of reading
 *             GameCube controller inputs directly.
 */
void controller_read_gc( struct controller_data * outdata, const uint8_t rumble[4] )
{
    static const unsigned long long SI_read_con_block[8] =
    {
        0x0308400300ffffff,
        0xffffffffff030840,
        0x0300ffffffffffff,
        0xffff0308400300ff,
        0xffffffffffffff03,
        0x08400300ffffffff,
        0xfffffffffe000000,
        1
    };

    static unsigned long long output[8], input[8];

    memcpy( input, SI_read_con_block, 64 );

    // Fill in the rumbles
    if (rumble[0])
        input[0] |= 1LLU << 24;
    if (rumble[1])
        input[2] |= 1LLU << 48;
    if (rumble[2])
        input[3] |= 1LLU << 8;
    if (rumble[3])
        input[5] |= 1LLU << 32;

    joybus_exec( input, output );

    memcpy( &outdata->gc[0], ((uint8_t *) output) + 5, 8 );
    memcpy( &outdata->gc[1], ((uint8_t *) output) + 5 + 13, 8 );
    memcpy( &outdata->gc[2], ((uint8_t *) output) + 5 + 13 * 2, 8 );
    memcpy( &outdata->gc[3], ((uint8_t *) output) + 5 + 13 * 3, 8 );
}

/**
 * @brief Read the controller origin status for all controllers, GC version
 *
 * This returns the values set on power up, or the values the user requested
 * by reseting the controller by holding X-Y-start. Apps should use these
 * as the center stick values. The meaning of the two deadzone values is unknown.
 *
 * @param[out] outdata
 *             Structure to place the returned controller button status
 * 
 * @deprecated Use the @ref joypad "Joypad subsystem" instead of reading
 *             GameCube controller origins directly.
 */
void controller_read_gc_origin( struct controller_origin_data * outdata )
{
    static const unsigned long long SI_read_con_block[8] =
    {
        0x010a41ffffffffff,
        0xffffffffff010a41,
        0xffffffffffffffff,
        0xffff010a41ffffff,
        0xffffffffffffff01,
        0x0a41ffffffffffff,
        0xfffffffffe000000,
        1
    };

    static unsigned long long output[8];

    joybus_exec( SI_read_con_block, output );

    memcpy( &outdata->gc[0], ((uint8_t *) output) + 3, 10 );
    memcpy( &outdata->gc[1], ((uint8_t *) output) + 3 + 13, 10 );
    memcpy( &outdata->gc[2], ((uint8_t *) output) + 3 + 13 * 2, 10 );
    memcpy( &outdata->gc[3], ((uint8_t *) output) + 3 + 13 * 3, 10 );
}

/**
 * @brief Executes a Joybus command synchronously on the given port.
 *
 * This function is not a stable feature of the libdragon API and should be
 * considered experimental!
 * 
 * The usage of this function will likely change as a result of the ongoing
 * effort to integrate the multitasking kernel with asynchronous operations.
 * 
 * @note This function is slow: it blocks until the command completes.
 *       Calling this function multiple times per frame may cause
 *       audio and video stuttering.
 * 
 * @param      port
 *             The Joybus port (0-4) to send the command to.
 * @param      command_id
 *             Joybus command identifier. See @ref JOYBUS_COMMAND_ID.
 * @param      send_len
 *             Number of bytes in the send payload
 *             (not including command ID).
 * @param      recv_len
 *             Number of bytes in the recieve payload.
 * @param[in]  send_data
 *             Buffer of send_len bytes to send to the Joybus device
 *             (not including command ID byte).
 * @param[out] recv_data
 *             Buffer of recv_len bytes for reply from the Joybus device.
 * 
 * @deprecated Use #joybus_exec_cmd instead
 */
void execute_raw_command(
    int controller,
    uint8_t command_id,
    size_t send_len,
    size_t recv_len,
    const void *send_data,
    void *recv_data
)
{
    assert((controller >= 0) && (controller < JOYBUS_PORT_COUNT));
    assert((controller + send_len + recv_len) < (JOYBUS_BLOCK_SIZE - 5));
    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    uint8_t output[JOYBUS_BLOCK_SIZE] = {0};
    size_t i = controller;
    // Set the command metadata
    input[i++] = send_len + 1; // Add a byte for the command ID
    input[i++] = recv_len;
    input[i++] = command_id;
    // Copy the send_data into the input buffer
    memcpy( &input[i], send_data, send_len );
    i += send_len + recv_len;
    // Close out the Joybus operation block
    input[i] = 0xFE;
    input[sizeof(input) - 1] = 0x01;
    // Execute the Joybus operation
    joybus_exec(input, output);
    // Copy recv_data from the output buffer
    memcpy(recv_data, &output[i - recv_len], recv_len);
}

/** 
 * @brief Initialize the controller subsystem.
 * 
 * After initialization, the controllers will be scanned automatically in
 * background one time per frame. You can access the last scanned status
 * using #get_keys_down, #get_keys_up, #get_keys_held #get_keys_pressed,
 * and #get_dpad_direction.
 * 
 * @deprecated Use #joypad_init instead.
 */
void controller_init( void ) { joypad_init(); }

/**
 * @brief Fetch the current controller state.
 * 
 * This function must be called once per frame, or any time we want to update
 * the state of the controllers. After calling this function, you can use
 * #get_keys_down, #get_keys_up, #get_keys_held, #get_keys_pressed and
 * #get_dpad_direction to inspect the controller state.
 * 
 * This function is very fast. In fact, controllers are read in background
 * asynchronously under interrupt, so this function just synchronizes the
 * internal state.
 * 
 * @deprecated Use #joypad_poll instead.
 */
void controller_scan( void ) { joypad_poll(); }

/**
 * @brief Get keys that were pressed since the last inspection
 *
 * Return keys pressed since last detection. This returns a standard
 * #SI_controllers_state_t struct identical to #controller_read. However, buttons
 * are only set if they were pressed down since the last read.
 *
 * @return A structure representing which buttons were just pressed down
 * 
 * @deprecated Use #joypad_get_buttons_pressed instead.
 */
struct controller_data get_keys_down( void )
{
    joypad_buttons_t buttons;
    struct controller_data ret = { 0 };

    JOYPAD_PORT_FOREACH (port)
    {
        buttons = joypad_get_buttons_pressed(port);
        controller_data_from_joypad_buttons(port, &ret, &buttons);
    }

    return ret;
}

/**
 * @brief Get keys that were released since the last inspection
 *
 * Return keys released since last detection. This returns a standard
 * #SI_controllers_state_t struct identical to #controller_read. However, buttons
 * are only set if they were released since the last read.
 *
 * @return A structure representing which buttons were just released
 * 
 * @deprecated Use #joypad_get_buttons_released instead.
 */
struct controller_data get_keys_up( void )
{
    joypad_buttons_t buttons;
    struct controller_data ret = { 0 };

    JOYPAD_PORT_FOREACH (port)
    {
        buttons = joypad_get_buttons_released(port);
        controller_data_from_joypad_buttons(port, &ret, &buttons);
    }

    return ret;
}

/**
 * @brief Get keys that were held since the last inspection
 *
 * Return keys held since last detection. This returns a standard
 * #SI_controllers_state_t struct identical to #controller_read. However, buttons
 * are only set if they were held since the last read.
 *
 * @return A structure representing which buttons were held
 * 
 * @deprecated Use #joypad_get_buttons_held instead.
 */
struct controller_data get_keys_held( void )
{
    joypad_buttons_t buttons;
    struct controller_data ret = { 0 };

    JOYPAD_PORT_FOREACH (port)
    {
        buttons = joypad_get_buttons_held(port);
        controller_data_from_joypad_buttons(port, &ret, &buttons);
    }

    return ret;
}

/**
 * @brief Get keys that are currently pressed, regardless of previous state
 *
 * This function works identically to #controller_read except that it returns
 * the cached data from the last background autoscan.
 *
 * @return A structure representing which buttons were pressed
 * 
 * @deprecated Use #joypad_get_inputs instead
 */
struct controller_data get_keys_pressed( void )
{
    joypad_inputs_t inputs;
    joypad_buttons_t buttons;
    struct controller_data ret = { 0 };

    JOYPAD_PORT_FOREACH (port)
    {
        inputs = joypad_get_inputs(port);
        buttons = inputs.__buttons;
        controller_data_from_joypad_buttons(port, &ret, &buttons);
        ret.c[port].x = inputs.stick_x;
        ret.c[port].y = inputs.stick_y;
    }

    return ret;
}

/**
 * @brief Return the DPAD calculated direction
 *
 * Return the direction of the DPAD specified in controller.  Follows standard
 * polar coordinates, where 0 = 0, pi/4 = 1, pi/2 = 2, etc...  Returns -1 when
 * not pressed.
 *
 * @param[in] controller
 *            The controller (0-3) to inspect
 *
 * @return A value 0-7 to represent which direction is held, or -1 when not pressed
 * 
 * @deprecated Use #joypad_get_direction instead.
 */
int get_dpad_direction( int controller )
{
    return joypad_get_direction((joypad_port_t) controller, JOYPAD_2D_DPAD);
}

/**
 * @brief Return a bitmask representing which controllers are present
 *
 * Queries the controller interface and returns a bitmask specifying which
 * controllers are present.  See #CONTROLLER_1_INSERTED, #CONTROLLER_2_INSERTED,
 * #CONTROLLER_3_INSERTED and #CONTROLLER_4_INSERTED.
 *
 * @return A bitmask representing controllers present
 * 
 * @deprecated Use #joypad_is_connected or #joypad_get_identifier instead.
 */
int get_controllers_present( void )
{
    return (
        (joypad_is_connected(JOYPAD_PORT_1) ? CONTROLLER_1_INSERTED : 0) |
        (joypad_is_connected(JOYPAD_PORT_2) ? CONTROLLER_2_INSERTED : 0) |
        (joypad_is_connected(JOYPAD_PORT_3) ? CONTROLLER_3_INSERTED : 0) |
        (joypad_is_connected(JOYPAD_PORT_4) ? CONTROLLER_4_INSERTED : 0)
    );
}

/**
 * @brief Return a bitmask specifying which controllers have recognized accessories
 *
 * Queries the controller interface and returns a bitmask specifying which
 * controllers have recognized accessories present.  See #CONTROLLER_1_INSERTED, 
 * #CONTROLLER_2_INSERTED, #CONTROLLER_3_INSERTED and #CONTROLLER_4_INSERTED.
 * 
 * @param[out] out
 *             Deprecated. Not used. Pass in NULL.
 *
 * @return A bitmask representing accessories recognized
 * 
 * @deprecated Use #joypad_get_accessory_type instead.
 */
int get_accessories_present(struct controller_data *out)
{
    return (
        (joypad_get_accessory_type(JOYPAD_PORT_1) ? CONTROLLER_1_INSERTED : 0) |
        (joypad_get_accessory_type(JOYPAD_PORT_2) ? CONTROLLER_2_INSERTED : 0) |
        (joypad_get_accessory_type(JOYPAD_PORT_3) ? CONTROLLER_3_INSERTED : 0) |
        (joypad_get_accessory_type(JOYPAD_PORT_4) ? CONTROLLER_4_INSERTED : 0)
    );
}

/**
 * @brief Identify the accessory connected to a controller
 *
 * Given a controller, identify the particular accessory type inserted.
 *
 * @param[in] controller
 *            The controller (0-3) to identify accessories on
 *
 * @retval #ACCESSORY_RUMBLEPAK The accessory connected is a rumblepak
 * @retval #ACCESSORY_MEMPAK The accessory connected is a mempak
 * @retval #ACCESSORY_TRANSFERPAK The accessory connected is a transferpak
 * @retval #ACCESSORY_VRU The accessory connected is a VRU
 * @retval #ACCESSORY_NONE The accessory was not recognized
 * 
 * @deprecated Use #joypad_get_accessory_type instead.
 *             For VRU/VRS devices, use #joypad_get_identifier.
 */
int identify_accessory( int controller )
{
    switch ( joypad_get_identifier(controller) )
    {
        case JOYBUS_IDENTIFIER_N64_CONTROLLER:
            switch ( joypad_get_accessory_type(controller) )
            {
                case JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK:
                    return ACCESSORY_MEMPAK;
                case JOYPAD_ACCESSORY_TYPE_RUMBLE_PAK:
                    return ACCESSORY_RUMBLEPAK;
                case JOYPAD_ACCESSORY_TYPE_TRANSFER_PAK:
                    return ACCESSORY_TRANSFERPAK;
                default:
                    return ACCESSORY_NONE;
            }
        case JOYBUS_IDENTIFIER_N64_VOICE_RECOGNITION:
            return ACCESSORY_VRU;
        default:
            return ACCESSORY_NONE;
    }
}

/**
 * @brief Read a chunk of data from a mempak
 *
 * Given a controller and an address, read 32 bytes from a mempak and
 * return them in data.
 *
 * @param[in]  controller
 *             Which controller to read the data from (0-3)
 * @param[in]  address
 *             A 32 byte aligned offset to read from on the mempak
 * @param[out] data
 *             Buffer to place 32 bytes of data read from the mempak
 *
 * @retval 0  if reading was successful
 * @retval -1 if the controller was out of range
 * @retval -2 if there was no mempak present in the controller
 * @retval -3 if the mempak returned invalid data
 * 
 * @deprecated Use #joybus_accessory_read instead.
 */
int read_mempak_address( int controller, uint16_t address, uint8_t *data )
{
    return joybus_accessory_read((joypad_port_t) controller, address, data);
}

/**
 * @brief Write a chunk of data to a mempak
 *
 * Given a controller and an address, write 32 bytes to a mempak from data.
 *
 * @param[in]  controller
 *             Which controller to write the data to (0-3)
 * @param[in]  address
 *             A 32 byte aligned offset to write to on the mempak
 * @param[out] data
 *             Buffer to source 32 bytes of data to write to the mempak
 *
 * @retval 0  if writing was successful
 * @retval -1 if the controller was out of range
 * @retval -2 if there was no mempak present in the controller
 * @retval -3 if the mempak returned invalid data
 * 
 * @deprecated Use #joybus_accessory_write instead.
 */
int write_mempak_address( int controller, uint16_t address, uint8_t *data )
{
    return joybus_accessory_write((joypad_port_t) controller, address, data);
}

/**
 * @brief Turn rumble on for a particular controller
 *
 * @param[in] controller
 *            The controller (0-3) who's rumblepak should activate
 * 
 * @deprecated Use #joypad_set_rumble_active instead.
 */
void rumble_start( int controller )
{
    joypad_set_rumble_active((joypad_port_t) controller, true);
}

/**
 * @brief Turn rumble off for a particular controller
 *
 * @param[in] controller
 *            The controller (0-3) who's rumblepak should deactivate
 * 
 * @deprecated Use #joypad_set_rumble_active instead.
 */
void rumble_stop( int controller )
{
    joypad_set_rumble_active((joypad_port_t) controller, false);
}

///@endcond

/** @} */ /* controller */
