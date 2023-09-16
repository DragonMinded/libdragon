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

void print_inputs(_SI_condat *inputs)
{
    printf(
        "Stick: %+04d,%+04d\n",
        inputs->x, inputs->y
    );
    printf(
        "D-U:%d D-D:%d D-L:%d D-R:%d C-U:%d C-D:%d C-L:%d C-R:%d\n",
        inputs->up, inputs->down,
        inputs->left, inputs->right,
        inputs->C_up, inputs->C_down,
        inputs->C_left, inputs->C_right
    );
    printf(
        "A:%d B:%d L:%d R:%d Z:%d Start:%d\n",
        inputs->A, inputs->B,
        inputs->L, inputs->R,
        inputs->Z, inputs->start
    );
}

int main(void)
{
    timer_init();
    controller_init();
    debug_init_isviewer();
    console_init();
    console_set_render_mode(RENDER_MANUAL);
    console_set_debug(false);

    while (1)
    {
        console_clear();

        printf("LibDragon Controller Subsystem Test\n\n");

        controller_scan();
        struct controller_data inputs = get_keys_pressed();

        for (int port = 0; port < 4; port++)
        {
            int accessory_type = identify_accessory(port);

            printf("Port %d ", port + 1);
            printf("Accessory: %s ", format_accessory_type(accessory_type));
            printf("\n");
            print_inputs(&inputs.c[port]);
            printf("\n");
        }

        console_render();
    }
}
