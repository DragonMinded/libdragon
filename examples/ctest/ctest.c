#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <libdragon.h>

char* format_type(joypad_accessory_type_t accessory)
{
    switch(accessory)
    {
        case JOYPAD_ACCESSORY_TYPE_RUMBLE_PAK:
            return "(rumble)";
        case JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK:
            return "(controller pak)";
        case JOYPAD_ACCESSORY_TYPE_TRANSFER_PAK:
            return "(transfer)";
        case JOYPAD_ACCESSORY_TYPE_BIO_SENSOR:
            return "(bio sensor)";
        case JOYPAD_ACCESSORY_TYPE_SNAP_STATION:
            return "(snap station)";

        #if 0
        //  VRU is not implemented in joypad module; can't check for it
        case ACCESSORY_VRU:
            return "(vru)";
        #endif
        
        case JOYPAD_ACCESSORY_TYPE_UNKNOWN:
            return "(unknown)";
        default:
            return "(unspecified)";
    }
}

int main(void)
{
    /* Initialize peripherals */
    console_init();
    joypad_init();
    timer_init();
    rtc_init();

    console_set_render_mode(RENDER_MANUAL);

    time_t current_time = -1;
    int testv = 0;
    int press = 0;
    uint8_t data[32];
    memset( data, 0, 32 );

    /* Main loop test */
    while(1) 
    {
        console_clear();

        /* To do initialize routines */
        joypad_poll();

        joypad_buttons_t pressed;

        JOYPAD_PORT_FOREACH (port)
        {
            pressed = joypad_get_buttons_pressed(port);
            if (pressed.a)
            {
                joypad_set_rumble_active(port, true);
            }

            if (pressed.a)
            {
                joypad_set_rumble_active(port, false);
            }

            if (pressed.z)
            {
                press = joybus_accessory_read(port, 0x0000, data);
            }

            printf("Controller %i %spresent\n", (int) (port + 1), (joypad_is_connected(port)) ? "" : "not ");
        }

        JOYPAD_PORT_FOREACH (port)
        {
            joypad_accessory_type_t accessory = joypad_get_accessory_type(port);
            printf("Accessory %i %spresent %s\n", (int) (port + 1), (accessory == JOYPAD_ACCESSORY_TYPE_NONE) ? "not " : "", 
                                              (accessory == JOYPAD_ACCESSORY_TYPE_NONE) ? "" : format_type(accessory));
        }

        printf("\n%d\n\n", testv++ );

        current_time = time( NULL );
        if( current_time != -1 )
        {
            printf("Current date/time: %s\n\n", ctime( &current_time ));
        }

        for( int i = 0; i < 32; i++ )
        {
            printf( "%02X", data[i] );
        }

        printf( "\n\n" );
        printf( "Operation returned: %d\n", press );

        console_render();
    }
}
