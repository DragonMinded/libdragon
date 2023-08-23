/**
 * @file joypad_internal.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joypad internal
 * @ingroup joypad 
 */

#ifndef __JOYPAD_INTERNAL_H
#define __JOYPAD_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "joypad_accessory.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup joypad
 * @{
 */

typedef enum
{
    JOYPAD_RUMBLE_METHOD_NONE = 0,
    JOYPAD_RUMBLE_METHOD_N64_RUMBLE_PAK,
    JOYPAD_RUMBLE_METHOD_GCN_CONTROLLER,
} joypad_rumble_method_t;

typedef struct joypad_gcn_origin_s
{
    uint8_t stick_x;
    uint8_t stick_y;
    uint8_t cstick_x;
    uint8_t cstick_y;
    uint8_t analog_l;
    uint8_t analog_r;
} joypad_gcn_origin_t;

#define JOYPAD_GCN_ORIGIN_INIT \
    ((joypad_gcn_origin_t){ 127, 127, 127, 127, 0, 0 })

typedef union joypad_buttons_raw_u
{
    uint16_t value;
    joypad_buttons_t buttons;
} joypad_buttons_raw_t;

typedef struct joypad_device_cold_s
{
    joypad_style_t style;
    joypad_inputs_t current;
    joypad_inputs_t previous;
} joypad_device_cold_t;

typedef struct joypad_device_hot_s
{
    joypad_style_t style;
    joypad_rumble_method_t rumble_method;
    bool rumble_active;
} joypad_device_hot_t;

extern volatile joypad_identifier_t joypad_identifiers_hot[JOYPAD_PORT_COUNT];
extern volatile joypad_device_hot_t joypad_devices_hot[JOYPAD_PORT_COUNT];
extern volatile joypad_gcn_origin_t joypad_origins_hot[JOYPAD_PORT_COUNT];
extern volatile joypad_accessory_t  joypad_accessories_hot[JOYPAD_PORT_COUNT];

#ifdef __cplusplus
}
#endif

/** @} */ /* joypad */

#endif
