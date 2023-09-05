/**
 * @file joypad_internal.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joypad internal
 * @ingroup joypad
 */

#ifndef __LIBDRAGON_JOYPAD_INTERNAL_H
#define __LIBDRAGON_JOYPAD_INTERNAL_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "joypad_accessory.h"
#include "utils.h"

/**
 * @addtogroup joypad
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Convenience macro to validate a Joypad port number */
#define ASSERT_JOYPAD_PORT_VALID(port) \
    assert((port) >= 0 && (port) < JOYPAD_PORT_COUNT)

/** @brief Joypad rumble methods enumeration. */
typedef enum
{
    /** @brief Rumble not supported. */
    JOYPAD_RUMBLE_METHOD_NONE = 0,
    /** @brief Nintendo 64 controller with Rumble Pak. */
    JOYPAD_RUMBLE_METHOD_N64_RUMBLE_PAK,
    /** @brief GameCube controller with rumble motors. */
    JOYPAD_RUMBLE_METHOD_GCN_CONTROLLER,
} joypad_rumble_method_t;

/** @brief Joypad GameCube controller origins structure. */
typedef struct joypad_gcn_origin_s
{
    /** @brief Analog stick X-axis. */
    uint8_t stick_x;
    /** @brief Analog stick Y-axis. */
    uint8_t stick_y;
    /** @brief Analog C-stick X-axis */
    uint8_t cstick_x;
    /** @brief Analog C-stick Y-axis */
    uint8_t cstick_y;
    /** @brief Analog L-trigger */
    uint8_t analog_l;
    /** @brief Analog R-trigger */
    uint8_t analog_r;
} joypad_gcn_origin_t;

/** @brief Initial state for GameCube controller origins. */
#define JOYPAD_GCN_ORIGIN_INIT \
    ((joypad_gcn_origin_t){ 127, 127, 127, 127, 0, 0 })

/** @brief Ensure value is in range of an analog stick axis. */
#define CLAMP_ANALOG_STICK(value) CLAMP((int)(value), -127, 127)

/** @brief Ensure value is in range of an analog trigger. */
#define CLAMP_ANALOG_TRIGGER(value) CLAMP((int)(value), 0, 255)

/** @brief "Cold" (non-volatile) Joypad device structure. */
typedef struct joypad_device_cold_s
{
    /** @brief Joypad style. */
    joypad_style_t style;
    /** @brief Joypad inputs for current frame. */
    joypad_inputs_t current;
    /** @brief Joypad inputs for previous frame. */
    joypad_inputs_t previous;
} joypad_device_cold_t;

/** @brief "Hot" (interrupt-driven) Joypad device structure. */
typedef struct joypad_device_hot_s
{
    /** @brief Joypad style. */
    joypad_style_t style;
    /** @brief Joypad rumble method. */
    joypad_rumble_method_t rumble_method;
    /** @brief Is the Joypad currently rumbling? */
    bool rumble_active;
} joypad_device_hot_t;

extern volatile joybus_identifier_t joypad_identifiers_hot[JOYPAD_PORT_COUNT];
extern volatile joypad_device_hot_t joypad_devices_hot[JOYPAD_PORT_COUNT];
extern volatile joypad_gcn_origin_t joypad_origins_hot[JOYPAD_PORT_COUNT];
extern volatile joypad_accessory_t  joypad_accessories_hot[JOYPAD_PORT_COUNT];

#ifdef __cplusplus
}
#endif

/** @} */ /* joypad */

#endif
