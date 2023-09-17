/**
 * @file joypadtest.c
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief N64 test ROM for Joypad subsystem
 */

#include <string.h>
#include <libdragon.h>

const char *format_joypad_style(joypad_style_t style)
{
    switch (style)
    {
    case JOYPAD_STYLE_NONE:
        return "None   ";
    case JOYPAD_STYLE_N64:
        return "N64    ";
    case JOYPAD_STYLE_GCN:
        return "GCN    ";
    case JOYPAD_STYLE_MOUSE:
        return "Mouse  ";
    default:
        return "Unknown";
    }
}

const char *format_joypad_accessory_type(joypad_accessory_type_t accessory_type)
{
    switch (accessory_type)
    {
    case JOYPAD_ACCESSORY_TYPE_NONE:
        return "None        ";
    case JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK:
        return "Memory      ";
    case JOYPAD_ACCESSORY_TYPE_RUMBLE_PAK:
        return "Rumble Pak  ";
    case JOYPAD_ACCESSORY_TYPE_TRANSFER_PAK:
        return "Transfer Pak";
    case JOYPAD_ACCESSORY_TYPE_BIO_SENSOR:
        return "Bio Sensor  ";
    case JOYPAD_ACCESSORY_TYPE_SNAP_STATION:
        return "Snap Station";
    default:
        return "Unknown     ";
    }
}

const char *format_joypad_rumble(bool supported, bool enabled)
{
    if (!supported) return "Unavailable";
    if (enabled) return "Active";
    return "Idle";
}

void print_joypad_inputs(joypad_inputs_t inputs)
{
    printf(
        "Stick: %+04d,%+04d C-Stick: %+04d,%+04d L-Trig:%03d R-Trig:%03d\n",
        inputs.stick_x, inputs.stick_y,
        inputs.cstick_x, inputs.cstick_y,
        inputs.analog_l, inputs.analog_r
    );
    printf(
        "D-U:%d D-D:%d D-L:%d D-R:%d C-U:%d C-D:%d C-L:%d C-R:%d\n",
        inputs.btn.d_up, inputs.btn.d_down,
        inputs.btn.d_left, inputs.btn.d_right,
        inputs.btn.c_up, inputs.btn.c_down,
        inputs.btn.c_left, inputs.btn.c_right
    );
    printf(
        "A:%d B:%d X:%d Y:%d L:%d R:%d Z:%d Start:%d\n",
        inputs.btn.a, inputs.btn.b,
        inputs.btn.x, inputs.btn.y,
        inputs.btn.l, inputs.btn.r,
        inputs.btn.z, inputs.btn.start
    );
}

int main(void)
{
    joypad_style_t style;
    joypad_accessory_type_t accessory_type;
    bool rumble_supported;
    bool rumble_active;
    joypad_inputs_t inputs;

    timer_init();
    joypad_init();
    debug_init_isviewer();
    console_init();
    console_set_render_mode(RENDER_MANUAL);
    console_set_debug(false);

    while (1)
    {
        console_clear();

        printf("LibDragon Joypad Subsystem Test\n\n");

        joypad_poll();

        JOYPAD_PORT_FOREACH (port)
        {
            style = joypad_get_style(port);
            accessory_type = joypad_get_accessory_type(port);
            rumble_supported = joypad_get_rumble_supported(port);
            rumble_active = joypad_get_rumble_active(port);
            inputs = joypad_get_inputs(port);

            if (rumble_supported)
            {
                if (inputs.btn.a && !rumble_active)
                {
                    joypad_set_rumble_active(port, true);
                }
                else if (!inputs.btn.a && rumble_active)
                {
                    joypad_set_rumble_active(port, false);
                }
            }

            printf("Port %d ", port + 1);
            printf("Style: %s ", format_joypad_style(style));
            printf("Pak: %s ", format_joypad_accessory_type(accessory_type));
            printf("Rumble: %s", format_joypad_rumble(rumble_supported, rumble_active));
            printf("\n");
            print_joypad_inputs(inputs);
            printf("\n");
        }

        console_render();
    }
}
