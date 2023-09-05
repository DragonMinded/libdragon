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
 * The controller subsystem is in charge of communication with all controllers 
 * and accessories plugged into the N64 controller ports. The controller subsystem
 * leverages the @ref joybus "Joybus Subsystem" to provide controller data and
 * interface with accessories such as the Controller Pak, Rumble Pak, Transfer Pak,
 * and the Voice-Recognition Unit.
 *
 * Code wishing to communicate with a controller or an accessory should first call
 * #controller_init. The controller subsystem performs an automatic background
 * scanning of all controllers, in an efficient way, saving the current status
 * in a local cache. Alternatively, it is possible to execute direct, blocking
 * controller I/O reads, though they might be quite slow.
 * 
 * To read the controller status from this cache, call #controller_scan once per
 * frame (or whenever you want to perform the reading), and then call #get_keys_down,
 * #get_keys_up, #get_keys_held and #get_keys_pressed, that will return the
 * status of all keys relative to the previous inspection. #get_dpad_direction will
 * return a number signifying the polar direction that the D-Pad is being
 * pressed in.
 *
 * To perform direct reads to the controllers, call #controller_read.  This will
 * return a structure consisting of all button states on all controllers currently
 * inserted. Note that this function takes about 10% of a frame's worth of time.
 *
 * Controllers can be enumerated with 
 * #get_controllers_present.  Similarly, accessories can be enumerated with
 * #get_accessories_present and #identify_accessory.
 *
 * To enable or disable rumbling on a controller, use #rumble_start and #rumble_stop.
 * These functions will turn rumble on and off at full speed respectively, so if
 * different rumble effects are desired, consider using the @ref timer for accurate
 * timing.
 *
 * A mempak attached to a controller can be treated in one of two ways: as a raw binary
 * string, or as a formatted mempak with notes.  The former allows storage of any
 * data as long as it fits, in any format convenient to the coder, but destroys any
 * non-homebrew data on the mempak.  The latter is recommended as it is completely
 * compatible with official N64 games, though it allows less data to be stored due to
 * filesystem overhead.  To read and write raw sectors, use #read_mempak_address and
 * #write_mempak_address.  The @ref mempak handles reading and writing from the mempak
 * in a way compatible with official games.
 *
 * @{
 */

/**
 * @brief Read the controller button status for all controllers
 *
 * Read the controller button status immediately and return results to data.
 * 
 * @note This function is slow: it blocks for about 10% of a frame time. To avoid
 *       this hit, use the managed functions (#get_keys_down, etc.).
 *
 * @param[out] output
 *             Structure to place the returned controller button status
 * 
 * @deprecated Use #joypad_read_n64_inputs_sync instead.
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
        ret.gc[port].a = ret.c[port].A = buttons.a;
        ret.gc[port].b = ret.c[port].B = buttons.b;
        ret.gc[port].z = ret.c[port].Z = buttons.z;
        ret.gc[port].start = ret.c[port].start = buttons.start;
        ret.gc[port].up = ret.c[port].up = buttons.d_up;
        ret.gc[port].down = ret.c[port].down = buttons.d_down;
        ret.gc[port].left = ret.c[port].left = buttons.d_left;
        ret.gc[port].right = ret.c[port].right = buttons.d_right;
        ret.gc[port].x = buttons.x;
        ret.gc[port].y = buttons.y;
        ret.gc[port].l = ret.c[port].L = buttons.l;
        ret.gc[port].r = ret.c[port].R = buttons.r;
        ret.c[port].C_up = buttons.c_up;
        ret.c[port].C_down = buttons.c_down;
        ret.c[port].C_left = buttons.c_left;
        ret.c[port].C_right = buttons.c_right;
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
        ret.gc[port].a = ret.c[port].A = buttons.a;
        ret.gc[port].b = ret.c[port].B = buttons.b;
        ret.gc[port].z = ret.c[port].Z = buttons.z;
        ret.gc[port].start = ret.c[port].start = buttons.start;
        ret.gc[port].up = ret.c[port].up = buttons.d_up;
        ret.gc[port].down = ret.c[port].down = buttons.d_down;
        ret.gc[port].left = ret.c[port].left = buttons.d_left;
        ret.gc[port].right = ret.c[port].right = buttons.d_right;
        ret.gc[port].x = buttons.x;
        ret.gc[port].y = buttons.y;
        ret.gc[port].l = ret.c[port].L = buttons.l;
        ret.gc[port].r = ret.c[port].R = buttons.r;
        ret.c[port].C_up = buttons.c_up;
        ret.c[port].C_down = buttons.c_down;
        ret.c[port].C_left = buttons.c_left;
        ret.c[port].C_right = buttons.c_right;
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
        ret.gc[port].a = ret.c[port].A = buttons.a;
        ret.gc[port].b = ret.c[port].B = buttons.b;
        ret.gc[port].z = ret.c[port].Z = buttons.z;
        ret.gc[port].start = ret.c[port].start = buttons.start;
        ret.gc[port].up = ret.c[port].up = buttons.d_up;
        ret.gc[port].down = ret.c[port].down = buttons.d_down;
        ret.gc[port].left = ret.c[port].left = buttons.d_left;
        ret.gc[port].right = ret.c[port].right = buttons.d_right;
        ret.gc[port].x = buttons.x;
        ret.gc[port].y = buttons.y;
        ret.gc[port].l = ret.c[port].L = buttons.l;
        ret.gc[port].r = ret.c[port].R = buttons.r;
        ret.c[port].C_up = buttons.c_up;
        ret.c[port].C_down = buttons.c_down;
        ret.c[port].C_left = buttons.c_left;
        ret.c[port].C_right = buttons.c_right;
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
 * @deprecated Use #joypad_get_buttons instead
 */
struct controller_data get_keys_pressed( void )
{
    joypad_buttons_t buttons;
    struct controller_data ret = { 0 };

    JOYPAD_PORT_FOREACH (port)
    {
        buttons = joypad_get_buttons(port);
        ret.gc[port].a = ret.c[port].A = buttons.a;
        ret.gc[port].b = ret.c[port].B = buttons.b;
        ret.gc[port].z = ret.c[port].Z = buttons.z;
        ret.gc[port].start = ret.c[port].start = buttons.start;
        ret.gc[port].up = ret.c[port].up = buttons.d_up;
        ret.gc[port].down = ret.c[port].down = buttons.d_down;
        ret.gc[port].left = ret.c[port].left = buttons.d_left;
        ret.gc[port].right = ret.c[port].right = buttons.d_right;
        ret.gc[port].x = buttons.x;
        ret.gc[port].y = buttons.y;
        ret.gc[port].l = ret.c[port].L = buttons.l;
        ret.gc[port].r = ret.c[port].R = buttons.r;
        ret.c[port].C_up = buttons.c_up;
        ret.c[port].C_down = buttons.c_down;
        ret.c[port].C_left = buttons.c_left;
        ret.c[port].C_right = buttons.c_right;
    }

    return ret;
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
    int ret = 0;

    if ( joypad_is_connected(JOYPAD_PORT_1) ) { ret |= CONTROLLER_1_INSERTED; }
    if ( joypad_is_connected(JOYPAD_PORT_2) ) { ret |= CONTROLLER_2_INSERTED; }
    if ( joypad_is_connected(JOYPAD_PORT_3) ) { ret |= CONTROLLER_3_INSERTED; }
    if ( joypad_is_connected(JOYPAD_PORT_4) ) { ret |= CONTROLLER_4_INSERTED; }

    return ret;
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
    int ret = 0;

    if ( joypad_get_accessory_type(JOYPAD_PORT_1) ) { ret |= CONTROLLER_1_INSERTED; }
    if ( joypad_get_accessory_type(JOYPAD_PORT_2) ) { ret |= CONTROLLER_2_INSERTED; }
    if ( joypad_get_accessory_type(JOYPAD_PORT_3) ) { ret |= CONTROLLER_3_INSERTED; }
    if ( joypad_get_accessory_type(JOYPAD_PORT_4) ) { ret |= CONTROLLER_4_INSERTED; }

    return ret;
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
    if ( joypad_get_identifier(controller) == JOYBUS_IDENTIFIER_N64_VOICE_RECOGNITION )
    {
        return ACCESSORY_VRU;
    }
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
}

/** @} */ /* controller */
