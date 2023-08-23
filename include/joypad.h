/**
 * @file joypad.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joypad Subsystem
 * @ingroup joypad 
 */

#ifndef __JOYPAD_H
#define __JOYPAD_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup joypad
 * @{
 */

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

#define JOYPAD_PORT_COUNT 4

/** @brief Convenience macro to iterate through all Joypad ports */
#define JOYPAD_PORT_FOREACH(iterator_token) for (\
    joypad_port_t iterator_token = JOYPAD_PORT_1; \
    iterator_token < JOYPAD_PORT_COUNT; \
    iterator_token += 1 \
)

/** 
 * @brief Joypad Identifier type
 * 
 * For example values, see @ref JOYBUS_IDENTIFIER
 */
typedef uint16_t joypad_identifier_t;

/** @brief Joypad Styles enumeration */
typedef enum
{
    /** @brief Unsupported Joypad Style */
    JOYPAD_STYLE_NONE = 0,
    /**
     * @brief Nintendo 64 Joypad Style.
     * 
     * The N64 controller has fewer and different buttons than
     * a GameCube controller: no X or Y buttons, no analog triggers,
     * and four C-directional buttons instead of an analog C-stick.
     */
    JOYPAD_STYLE_N64,
    /**
     * @brief GameCube Joypad Style.
     * 
     * The GameCube controller has more and different buttons
     * than a Nintendo 64 controller: X & Y buttons, analog
     * L & R triggers, and an analog C-stick instead of buttons.
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
    JOYPAD_ACCESSORY_TYPE_NONE = 0,
    JOYPAD_ACCESSORY_TYPE_UNKNOWN,
    JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK,
    JOYPAD_ACCESSORY_TYPE_RUMBLE_PAK,
    JOYPAD_ACCESSORY_TYPE_TRANSFER_PAK,
    JOYPAD_ACCESSORY_TYPE_BIO_SENSOR,
    JOYPAD_ACCESSORY_TYPE_SNAP_STATION,
} joypad_accessory_type_t;

/** @brief Joypad Buttons Common State Structure */
typedef struct __attribute__((packed)) joypad_buttons_s
{
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
     * This input only exists on GCN controllers.
     */
    unsigned y : 1;
    /**
     * @brief State of the X button.
     * This input only exists on GCN controllers.
     */
    unsigned x : 1;
    /** @brief State of the digital L trigger */
    unsigned l : 1;
    /** @brief State of the digital R trigger */
    unsigned r : 1;
    /**
     * @brief State of the C-Up button.
     * For GameCube controllers, the value will be
     * emulated based on the C-Stick Y axis position.
     */
    unsigned c_up : 1;
    /**
     * @brief State of the C-Down button.
     * For GameCube controllers, the value will be
     * emulated based on the C-Stick Y axis position.
     */
    unsigned c_down : 1;
    /**
     * @brief State of the C-Left button.
     * For GameCube controllers, the value will be
     * emulated based on the C-Stick X axis position.
     */
    unsigned c_left : 1;
    /**
     * @brief State of the C-Right button.
     * For GameCube controllers, the value will be
     * emulated based on the C-Stick X axis position.
     */
    unsigned c_right : 1;
} joypad_buttons_t;

/** @brief Joypad Inputs Common State Structure */
typedef struct __attribute__((packed)) joypad_inputs_s
{
    union
    {
        joypad_buttons_t buttons;
        struct __attribute__((packed))
        {
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
             * This input only exists on GCN controllers.
             */
            unsigned y : 1;
            /**
             * @brief State of the X button.
             * This input only exists on GCN controllers.
             */
            unsigned x : 1;
            /** @brief State of the digital L trigger */
            unsigned l : 1;
            /** @brief State of the digital R trigger */
            unsigned r : 1;
            /**
             * @brief State of the C-Up button.
             * For GameCube controllers, the value will be
             * emulated based on the C-Stick Y axis position.
             */
            unsigned c_up : 1;
            /**
             * @brief State of the C-Down button.
             * For GameCube controllers, the value will be
             * emulated based on the C-Stick Y axis position.
             */
            unsigned c_down : 1;
            /**
             * @brief State of the C-Left button.
             * For GameCube controllers, the value will be
             * emulated based on the C-Stick X axis position.
             */
            unsigned c_left : 1;
            /**
             * @brief State of the C-Right button.
             * For GameCube controllers, the value will be
             * emulated based on the C-Stick X axis position.
             */
            unsigned c_right : 1;
        };
    };
    /**
     * @brief Position of the analog joystick X axis. (-127, +127)
     * On real controllers the range of this axis is roughly (-100, +100).
     * For well-worn N64 controllers, the range may be as low as (-60, +60).
     * For GCN controllers, this value will be relative to its origin.
     */
    int8_t stick_x;
    /**
     * @brief Position of the analog joystick Y axis. (-127, +127)
     * On real controllers the range of this axis is roughly (-100, +100).
     * For well-worn N64 controllers, the range may be as low as (-60, +60).
     * For GCN controllers, this value will be relative to its origin.
     */
    int8_t stick_y;
    /**
     * @brief Position of the analog "C-Stick" X axis. (-127, +127)
     * On real controllers the range of this axis is roughly (-76, +76).
     * For GCN controllers, this value will be relative to its origin.
     * For N64 controllers, this value will be emulated based on the
     * digital C-Left and C-Right button values (-76=C-Left, +76=C-Right).
     */
    int8_t cstick_x;
    /**
     * @brief Position of the analog "C-Stick" Y axis. (-127, +127)
     * On real controllers the range of this axis is roughly (-76, +76).
     * The value will be relative to the corresponding origin.
     * For N64 controllers, this value will be emulated based on the
     * digital C-Up and C-Down button values (-76=C-Down, +76=C-Up).
     */
    int8_t cstick_y;
    /**
     * @brief Position of the analog L trigger. (0, 255)
     * This value will be close to zero when no pressure is applied,
     * and close to 200 when full pressure is applied.
     * For GCN controllers, this value will be relative to its origin.
     * For N64 controllers, this value will be emulated based on the 
     * digital L trigger button value (0=unpressed, 200=pressed).
     */
    uint8_t analog_l;
    /**
     * @brief Position of the analog R trigger. (0, 255)
     * This value will be close to zero when no pressure is applied,
     * and close to 200 when full pressure is applied.
     * For GCN controllers, this value will be relative to its origin.
     * For N64 controllers, this value will be emulated based on the
     * digital R trigger button value (0=unpressed, 200=pressed).
     */
    uint8_t analog_r;
} joypad_inputs_t;

/** @brief Joypad Axis enumeration */
typedef enum
{
    JOYPAD_AXIS_STICK_X  = offsetof(joypad_inputs_t, stick_x),
    JOYPAD_AXIS_STICK_Y  = offsetof(joypad_inputs_t, stick_y),
    JOYPAD_AXIS_CSTICK_X = offsetof(joypad_inputs_t, cstick_x),
    JOYPAD_AXIS_CSTICK_Y = offsetof(joypad_inputs_t, cstick_y),
    JOYPAD_AXIS_ANALOG_L = offsetof(joypad_inputs_t, analog_l),
    JOYPAD_AXIS_ANALOG_R = offsetof(joypad_inputs_t, analog_r),
} joypad_axis_t;

void joypad_init(void);
void joypad_close(void);
void joypad_identify_sync(bool reset);
void joypad_read_sync(void);
void joypad_scan(void);

joypad_identifier_t joypad_get_identifier(joypad_port_t port);
joypad_style_t joypad_get_style(joypad_port_t port);
joypad_accessory_type_t joypad_get_accessory_type(joypad_port_t port);
bool joypad_get_rumble_supported(joypad_port_t port);
bool joypad_get_rumble_active(joypad_port_t port);
void joypad_set_rumble_active(joypad_port_t port, bool active);

joypad_inputs_t joypad_get_inputs(joypad_port_t port);
joypad_buttons_t joypad_get_buttons(joypad_port_t port);
joypad_buttons_t joypad_get_buttons_pressed(joypad_port_t port);
joypad_buttons_t joypad_get_buttons_released(joypad_port_t port);
joypad_buttons_t joypad_get_buttons_held(joypad_port_t port);

int joypad_get_axis_pressed(joypad_port_t port, joypad_axis_t axis);
int joypad_get_axis_released(joypad_port_t port, joypad_axis_t axis);
int joypad_get_axis_held(joypad_port_t port, joypad_axis_t axis);

#ifdef __cplusplus
}
#endif

/** @} */ /* joypad */

#endif
