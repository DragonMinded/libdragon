/**
 * @file controllertest.c
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief N64 test ROM for Controller subsystem
 */

#include <string.h>
#include <libdragon.h>

const char *format_accessory_type(int accessory_type)
{
    switch (accessory_type)
    {
    case ACCESSORY_NONE:
        return "None        ";
    case ACCESSORY_MEMPAK:
        return "Memory      ";
    case ACCESSORY_RUMBLEPAK:
        return "Rumble Pak  ";
    case ACCESSORY_TRANSFERPAK:
        return "Transfer Pak";
    case ACCESSORY_VRU:
        return "VRU         ";
    default:
        return "Unknown     ";
    }
}

void print_inputs(joypad_inputs_t inputs)
{
    printf(
        "Stick: %+04d,%+04d\n",
        inputs.stick_x, inputs.stick_y
    );
    printf(
        "D-U:%d D-D:%d D-L:%d D-R:%d C-U:%d C-D:%d C-L:%d C-R:%d\n",
        inputs.btn.d_up, inputs.btn.d_down,
        inputs.btn.d_left, inputs.btn.d_right,
        inputs.btn.c_up, inputs.btn.c_down,
        inputs.btn.c_left, inputs.btn.c_right
    );
    printf(
        "A:%d B:%d L:%d R:%d Z:%d Start:%d\n",
        inputs.btn.a, inputs.btn.b,
        inputs.btn.l, inputs.btn.r,
        inputs.btn.z, inputs.btn.start
    );
}

int main(void)
{
    timer_init();
    joypad_init();
    debug_init_isviewer();
    console_init();
    console_set_render_mode(RENDER_MANUAL);
    console_set_debug(false);

    while (1)
    {
        console_clear();

        printf("LibDragon Controller Subsystem Test\n\n");

        joypad_poll();

        for (int port = 0; port < 4; port++)
        {
            if (!joypad_is_connected(port)) continue;

            joypad_inputs_t in = joypad_get_inputs(port);

            int accessory_type = joypad_get_accessory_type(port);

            printf("Port %d ", port + 1);
            printf("Accessory: %s ", format_accessory_type(accessory_type));
            printf("\n");
            print_inputs(in);
            printf("\n");
        }

        console_render();
    }
}
