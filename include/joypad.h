/**
 * @file joypad.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joypad Subsystem
 * @ingroup joypad
 */

#ifndef __LIBDRAGON_JOYPAD_H
#define __LIBDRAGON_JOYPAD_H

#include <stdbool.h>

#include "joybus.h"

/**
 * @defgroup joypad Joypad Subsystem
 * @ingroup peripherals
 * @brief Joypad abstraction interface.
 *
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
 * To read the controllers, first call #joypad_poll once per frame to process
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
 * * #joypad_get_direction
 * 
 * To read analog directions as digital inputs for a Joypad device:
 * * #joypad_get_axis_pressed
 * * #joypad_get_axis_released
 * * #joypad_get_axis_held
 * 
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Joypad Port Numbers */
typedef enum
{
    /** @brief Joypad Port 1 */
    JOYPAD_PORT_1     = 0,
    /** @brief Joypad Port 2 */
    JOYPAD_PORT_2     = 1,
    /** @brief Joypad Port 3 */
    JOYPAD_PORT_3     = 2,
    /** @brief Joypad Port 4 */
    JOYPAD_PORT_4     = 3,
} joypad_port_t;

/** @brief Count of Joypad ports */
#define JOYPAD_PORT_COUNT 4

/** @brief Convenience macro to iterate through all Joypad ports */
#define JOYPAD_PORT_FOREACH(iterator_token) \
    for (\
        joypad_port_t iterator_token = JOYPAD_PORT_1; \
        iterator_token < JOYPAD_PORT_COUNT; \
        iterator_token += 1 \
    )

/** @brief Joypad Styles enumeration */
typedef enum
{
    /** @brief Unsupported Joypad Style */
    JOYPAD_STYLE_NONE = 0,
    /**
     * @brief Nintendo 64 Joypad Style.
     * 
     * A standard N64 controller, which has an analog stick,
     * directional pad, start button, L & R shoulder buttons,
     * a Z trigger, A & B face buttons, and a C-directional pad.
     * 
     * For convenience, the N64 style will coarsely simulate
     * certain GameCube controller inputs:
     * 
     * * C-directional pad maps to the C-stick.
     * * L & R shoulder buttons map to the analog triggers.
     */
    JOYPAD_STYLE_N64,
    /**
     * @brief GameCube Joypad Style.
     * 
     * A standard GameCube controller, which is supported on N64
     * when using a passive adapter to convert the plug.
     *
     * The GameCube controller has more and different buttons
     * than a Nintendo 64 controller: X & Y buttons, analog
     * L & R triggers, and an analog C-stick instead of buttons.
     * 
     * For convenience, the GameCube style will coarsely simulate
     * the C-directional pad using C-stick inputs.
     */
    JOYPAD_STYLE_GCN,
    /**
     * @brief Mouse Joypad Style.
     *
     * The N64 Mouse peripheral is read like a controller, but
     * only has A & B buttons, and the analog stick reports
     * the relative value since it was last read.
     */
    JOYPAD_STYLE_MOUSE,
} joypad_style_t;

/** @brief Joypad Accessories enumeration */
typedef enum
{
    /** @brief No accessory. */
    JOYPAD_ACCESSORY_TYPE_NONE = 0,
    /** @brief Unknown or malfunctioning accessory. */
    JOYPAD_ACCESSORY_TYPE_UNKNOWN,
    /** @brief Controller Pak accessory. */
    JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK,
    /** @brief Rumble Pak accessory. */
    JOYPAD_ACCESSORY_TYPE_RUMBLE_PAK,
    /** @brief Transfer Pak accessory. */
    JOYPAD_ACCESSORY_TYPE_TRANSFER_PAK,
    /** @brief Bio Sensor accessory. */
    JOYPAD_ACCESSORY_TYPE_BIO_SENSOR,
    /** @brief Pokemon Snap Station accessory. */
    JOYPAD_ACCESSORY_TYPE_SNAP_STATION,
} joypad_accessory_type_t;

/** @brief Joypad Buttons */
typedef union joypad_buttons_u
{
    /** @brief Raw button data as a 16-bit value. */
    uint16_t raw;
    /// @cond
    struct __attribute__((packed))
    {
    /// @endcond
        /** @brief State of the A button */
        unsigned a : 1;
        /** @brief State of the B button */
        unsigned b : 1;
        /** @brief State of the Z button */
        unsigned z : 1;
        /** @brief State of the Start button */
        unsigned start : 1;
        /** @brief State of the D-Pad Up button */
        unsigned d_up : 1;
        /** @brief State of the D-Pad Down button */
        unsigned d_down : 1;
        /** @brief State of the D-Pad Left button */
        unsigned d_left : 1;
        /** @brief State of the D-Pad Right button */
        unsigned d_right : 1;
        /**
         * @brief State of the Y button.
         * 
         * This input only exists on GameCube controllers.
         */
        unsigned y : 1;
        /**
         * @brief State of the X button.
         * 
         * This input only exists on GameCube controllers.
         */
        unsigned x : 1;
        /** @brief State of the digital L trigger */
        unsigned l : 1;
        /** @brief State of the digital R trigger */
        unsigned r : 1;
        /**
         * @brief State of the C-Up button.
         * 
         * For GameCube controllers, the value will be
         * emulated based on the C-Stick Y axis position.
         */
        unsigned c_up : 1;
        /**
         * @brief State of the C-Down button.
         * 
         * For GameCube controllers, the value will be
         * emulated based on the C-Stick Y axis position.
         */
        unsigned c_down : 1;
        /**
         * @brief State of the C-Left button.
         * 
         * For GameCube controllers, the value will be
         * emulated based on the C-Stick X axis position.
         */
        unsigned c_left : 1;
        /**
         * @brief State of the C-Right button.
         * 
         * For GameCube controllers, the value will be
         * emulated based on the C-Stick X axis position.
         */
        unsigned c_right : 1;
    /// @cond
    };
    /// @endcond
} joypad_buttons_t;

/** @brief Joypad Inputs Unified State Structure */
typedef struct __attribute__((packed)) joypad_inputs_s
{
    /**
     * @brief Structure containing digital button inputs state.
     */
    joypad_buttons_t btn;
    /**
     * @brief Position of the analog joystick X axis. (-127, +127)
     * 
     * On real controllers the range of this axis is roughly (-100, +100).
     * 
     * For well-worn N64 controllers, the range may be as low as (-60, +60).
     * 
     * On startup, an N64 controller will report its current stick position
     * as (0, 0). To reset the origin on an N64 controller, hold the L & R
     * shoulder buttons and the start button for several seconds with the
     * analog stick in a neutral position.
     * 
     * For GameCube controllers, this value will be relative to its origin.
     * The Joypad subsystem will automatically read the origins of GameCube
     * controllers and account for them when resolving the analog inputs.
     * To reset the origin on a GameCube controller, hold the X & Y buttons
     * and the start button for several seconds with the analog inputs in
     * neutral positions.
     */
    int8_t stick_x;
    /**
     * @brief Position of the analog joystick Y axis. (-127, +127)
     * 
     * On real controllers the range of this axis is roughly (-100, +100).
     * 
     * For well-worn N64 controllers, the range may be as low as (-60, +60).
     * 
     * On startup, an N64 controller will report its current stick position
     * as (0, 0). To reset the origin on an N64 controller, hold the L & R
     * shoulder buttons and the start button for several seconds with the
     * analog stick in a neutral position.
     * 
     * For GameCube controllers, this value will be relative to its origin.
     * The Joypad subsystem will automatically read the origins of GameCube
     * controllers and account for them when resolving the analog inputs.
     * To reset the origin on a GameCube controller, hold the X & Y buttons
     * and the start button for several seconds with the analog inputs in
     * neutral positions.
     */
    int8_t stick_y;
    /**
     * @brief Position of the analog "C-Stick" X axis. (-127, +127)
     * 
     * On real controllers the range of this axis is roughly (-76, +76).
     * 
     * For N64 controllers, this value will be emulated based on the
     * digital C-Left and C-Right button values (-76=C-Left, +76=C-Right).
     * 
     * For GameCube controllers, this value will be relative to its origin.
     * The Joypad subsystem will automatically read the origins of GameCube
     * controllers and account for them when resolving the analog inputs.
     * To reset the origin on a GameCube controller, hold the X & Y buttons
     * and the start button for several seconds with the analog inputs in
     * neutral positions.
     */
    int8_t cstick_x;
    /**
     * @brief Position of the analog "C-Stick" Y axis. (-127, +127)
     * 
     * On real controllers the range of this axis is roughly (-76, +76).
     * 
     * For N64 controllers, this value will be emulated based on the
     * digital C-Up and C-Down button values (-76=C-Down, +76=C-Up).
     * 
     * For GameCube controllers, this value will be relative to its origin.
     * The Joypad subsystem will automatically read the origins of GameCube
     * controllers and account for them when resolving the analog inputs.
     * To reset the origin on a GameCube controller, hold the X & Y buttons
     * and the start button for several seconds with the analog inputs in
     * neutral positions.
     */
    int8_t cstick_y;
    /**
     * @brief Position of the analog L trigger. (0, 255)
     * 
     * This value will be close to zero when no pressure is applied,
     * and close to 200 when full pressure is applied.
     * 
     * For N64 controllers, this value will be emulated based on the
     * digital L trigger button value (0=unpressed, 200=pressed).
     * 
     * For GameCube controllers, this value will be relative to its origin.
     * The Joypad subsystem will automatically read the origins of GameCube
     * controllers and account for them when resolving the analog inputs.
     * To reset the origin on a GameCube controller, hold the X & Y buttons
     * and the start button for several seconds with the analog inputs in
     * neutral positions.
     */
    uint8_t analog_l;
    /**
     * @brief Position of the analog R trigger. (0, 255)
     * 
     * This value will be close to zero when no pressure is applied,
     * and close to 200 when full pressure is applied.
     * 
     * For N64 controllers, this value will be emulated based on the
     * digital R trigger button value (0=unpressed, 200=pressed).
     * 
     * For GameCube controllers, this value will be relative to its origin.
     * The Joypad subsystem will automatically read the origins of GameCube
     * controllers and account for them when resolving the analog inputs.
     * To reset the origin on a GameCube controller, hold the X & Y buttons
     * and the start button for several seconds with the analog inputs in
     * neutral positions.
     */
    uint8_t analog_r;
} joypad_inputs_t;

/**
 * @anchor JOYPAD_RANGE
 * @name Joybus analog value ranges
 * @{
 */
/**
 * @brief Maximum range of a Nintendo 64 controller analog stick.
 */
#define JOYPAD_RANGE_N64_STICK_MAX    90
/**
 * @brief Maximum range of a GameCube controller analog stick.
 */
#define JOYPAD_RANGE_GCN_STICK_MAX    100
/**
 * @brief Maximum range of a GameCube controller analog C-stick.
 */
#define JOYPAD_RANGE_GCN_CSTICK_MAX   76
/**
 * @brief Maximum range of a GameCube controller analog trigger.
 */
#define JOYPAD_RANGE_GCN_TRIGGER_MAX  200
/** @} */

/**
 * @brief Joypad Axis enumeration values.
 * 
 * These values are used to index into the #joypad_inputs_t structure.
 */
typedef enum
{
    /** @brief Joypad stick X axis. */
    JOYPAD_AXIS_STICK_X  = offsetof(joypad_inputs_t, stick_x),
    /** @brief Joypad stick Y axis. */
    JOYPAD_AXIS_STICK_Y  = offsetof(joypad_inputs_t, stick_y),
    /** @brief Joypad C-stick X axis. */
    JOYPAD_AXIS_CSTICK_X = offsetof(joypad_inputs_t, cstick_x),
    /** @brief Joypad C-stick Y axis. */
    JOYPAD_AXIS_CSTICK_Y = offsetof(joypad_inputs_t, cstick_y),
    /** @brief Joypad analog L trigger axis. */
    JOYPAD_AXIS_ANALOG_L = offsetof(joypad_inputs_t, analog_l),
    /** @brief Joypad analog R trigger axis. */
    JOYPAD_AXIS_ANALOG_R = offsetof(joypad_inputs_t, analog_r),
} joypad_axis_t;

/**
 * @brief Joypad 2D axes enumeration.
 * 
 * These values are used to select one or more 2D input axes.
 */
typedef enum
{
    /** @brief Analog stick 2D axes. */
    JOYPAD_2D_STICK = (1 << 0),
    /** @brief D-Pad 2D axes. */
    JOYPAD_2D_DPAD  = (1 << 1),
    /** @brief C buttons 2D axes.*/
    JOYPAD_2D_C     = (1 << 2),
    /** @brief Left-Hand 2D axes: Analog stick or D-Pad. */
    JOYPAD_2D_LH    = (JOYPAD_2D_STICK | JOYPAD_2D_DPAD),
    /** @brief Right-Hand 2D axes: Analog stick or C buttons. */
    JOYPAD_2D_RH    = (JOYPAD_2D_STICK | JOYPAD_2D_C),
    /** @brief Any 2D axes: Analog stick, D-Pad, or C buttons. */
    JOYPAD_2D_ANY   = (JOYPAD_2D_STICK | JOYPAD_2D_DPAD | JOYPAD_2D_C),
} joypad_2d_t;

/** @brief Joypad 8-way directional enumeration */
typedef enum
{
    /** @brief 8-way no direction. */
    JOYPAD_8WAY_NONE       = -1,
    /** @brief 8-way right direction. */
    JOYPAD_8WAY_RIGHT      = 0,
    /** @brief 8-way up-right direction. */
    JOYPAD_8WAY_UP_RIGHT   = 1,
    /** @brief 8-way up direction. */
    JOYPAD_8WAY_UP         = 2,
    /** @brief 8-way up-left direction. */
    JOYPAD_8WAY_UP_LEFT    = 3,
    /** @brief 8-way left direction. */
    JOYPAD_8WAY_LEFT       = 4,
    /** @brief 8-way down-left direction. */
    JOYPAD_8WAY_DOWN_LEFT  = 5,
    /** @brief 8-way down direction. */
    JOYPAD_8WAY_DOWN       = 6,
    /** @brief 8-way down-right direction. */
    JOYPAD_8WAY_DOWN_RIGHT = 7,
} joypad_8way_t;

/**
 * @brief Initialize the Joypad subsystem.
 * 
 * Starts reading Joypads during VI interrupt.
 */
void joypad_init(void);

/**
 * @brief Close the Joypad subsystem.
 * 
 * Stops reading Joypads during VI interrupt.
 */
void joypad_close(void);

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
 * * #joypad_get_direction
 * * #joypad_get_axis_pressed
 * * #joypad_get_axis_released
 * * #joypad_get_axis_held
 * 
 * This function is very fast. In fact, joypads are read in background
 * asynchronously under interrupt, so this function just synchronizes the
 * internal state.
 */
void joypad_poll(void);

/**
 * @brief Whether a Joybus device is plugged in to a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @retval true A Joybus device is connected to the Joypad port.
 * @retval false Nothing is connected to the Joypad port.
 */
bool joypad_is_connected(joypad_port_t port);

/**
 * @brief Get the Joybus device identifier for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joybus device identifier (#joybus_identifier_t)
 */
joybus_identifier_t joypad_get_identifier(joypad_port_t port);

/**
 * @brief Get the Joypad style for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad style enumeration value (#joypad_style_t)
 */
joypad_style_t joypad_get_style(joypad_port_t port);

/**
 * @brief Get the Joypad accessory type for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad accessory type enumeration value (#joypad_accessory_type_t)
 */
joypad_accessory_type_t joypad_get_accessory_type(joypad_port_t port);

/**
 * @brief Is rumble supported for a Joypad port?
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Whether rumble is supported
 */
bool joypad_get_rumble_supported(joypad_port_t port);

/**
 * @brief Is rumble active for a Joypad port?
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Whether rumble is active
 */
bool joypad_get_rumble_active(joypad_port_t port);

/**
 * @brief Activate or deactivate rumble on a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * @param active Whether rumble should be active
 */
void joypad_set_rumble_active(joypad_port_t port, bool active);

/**
 * @brief Get the current Joypad inputs state for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad inputs structure (#joypad_inputs_t)
 */
joypad_inputs_t joypad_get_inputs(joypad_port_t port);

/**
 * @brief Get the current Joypad buttons state for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad buttons structure (#joypad_buttons_t)
 */
joypad_buttons_t joypad_get_buttons(joypad_port_t port);

/**
 * @brief Get the Joypad buttons that were pressed since the last
 *        time Joypads were read for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad buttons structure (#joypad_buttons_t)
 */
joypad_buttons_t joypad_get_buttons_pressed(joypad_port_t port);

/**
 * @brief Get the Joypad buttons that were released since the last
 *        time Joypads were read for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad buttons structure (#joypad_buttons_t)
 */
joypad_buttons_t joypad_get_buttons_released(joypad_port_t port);

/**
 * @brief Get the Joypad buttons that are held down since the last
 *        time Joypads were read for a Joypad port.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * 
 * @return Joypad buttons structure (#joypad_buttons_t)
 */
joypad_buttons_t joypad_get_buttons_held(joypad_port_t port);

/**
 * @brief Get the 8-way direction for a Joypad port's directional axes.
 * 
 * @param port Joypad port number (#joypad_port_t)
 * @param axes 2D axes enumeration value (#joypad_2d_t)
 * @return Joypad 8-way direction enumeration value (#joypad_8way_t) 
 */
joypad_8way_t joypad_get_direction(joypad_port_t port, joypad_2d_t axes);

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
int joypad_get_axis_pressed(joypad_port_t port, joypad_axis_t axis);

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
int joypad_get_axis_released(joypad_port_t port, joypad_axis_t axis);

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
int joypad_get_axis_held(joypad_port_t port, joypad_axis_t axis);

#ifdef __cplusplus
}
#endif

/** @} */ /* joypad */

#endif
