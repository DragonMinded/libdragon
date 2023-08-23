/**
 * @file joypad_utils.h
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Joypad utilities
 * @ingroup joypad 
 */

#ifndef __JOYPAD_UTILS_H
#define __JOYPAD_UTILS_H

#define CLAMP(value, low, high) ({\
    typeof(value) _value = (value); \
    typeof(low) _low = (low); \
    typeof(high) _high = (high); \
    _value < _low ? _low : (_value > _high ? _high : _value); \
})

#define CLAMP_ANALOG_AXIS(value) CLAMP((int)(value), -127, 127)
#define CLAMP_ANALOG_TRIGGER(value) CLAMP((int)(value), 0, 255)

#endif
