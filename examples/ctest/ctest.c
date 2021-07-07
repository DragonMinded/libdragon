#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

static resolution_t res = RESOLUTION_320x240;
static bitdepth_t bit = DEPTH_32_BPP;

const char * format_type( int accessory )
{
    switch( accessory )
    {
        case ACCESSORY_RUMBLEPAK:
            return "(rumble)";
        case ACCESSORY_MEMPAK:
            return "(memory)";
        case ACCESSORY_VRU:
            return "(vru)";
        default:
            return "(none)";
    }
}

const char * format_eeprom_type( eeprom_type_t type )
{
    switch(type)
    {
        case EEPROM_NONE:
            return "No EEPROM present.";
        case EEPROM_4K:
            return "4k EEPROM detected.";
        case EEPROM_16K:
            return "16k EEPROM detected.";
        default:
            return "Unknown EEPROM detected.";
    }
}

const char * format_rtc_status( int status )
{
    switch(status)
    {
        case RTC_MISSING:
            return "No RTC detected.";
        case RTC_PRESENT:
            return "RTC detected.";
        default:
            return "Unknown RTC status.";
    }
}

int main(void)
{
    /* enable interrupts (on the CPU) */
    init_interrupts();

    /* Initialize peripherals */
    display_init( res, bit, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );
    console_init();
    controller_init();

    console_set_render_mode(RENDER_MANUAL);

    int testv = 0;
    int press = 0;
    uint8_t data[32];
    memset( data, 0, 32 );
    eeprom_type_t eeprom = eeprom_present();
    int rtc = rtc_status();
    rtc_time_t time;

    /* Main loop test */
    while(1) 
    {
        console_clear();

        /* To do initialize routines */
        controller_scan();

        struct controller_data keys = get_keys_down();

        for( int i = 0; i < 4; i++ )
        {
            if( keys.c[i].A )
            {
                rumble_start( i );
            }

            if( keys.c[i].B )
            {
                rumble_stop( i );
            }

            if( keys.c[i].Z )
            {
                press = read_mempak_address( i, 0x0000, data );
            }
        }

        int controllers = get_controllers_present();

        printf( "Controller 1 %spresent\n", (controllers & CONTROLLER_1_INSERTED) ? "" : "not " );
        printf( "Controller 2 %spresent\n", (controllers & CONTROLLER_2_INSERTED) ? "" : "not " );
        printf( "Controller 3 %spresent\n", (controllers & CONTROLLER_3_INSERTED) ? "" : "not " );
        printf( "Controller 4 %spresent\n", (controllers & CONTROLLER_4_INSERTED) ? "" : "not " );

        struct controller_data output;
        int accessories = get_accessories_present( &output );

        printf( "Accessory 1 %spresent %s\n", (accessories & CONTROLLER_1_INSERTED) ? "" : "not ", 
                                              (accessories & CONTROLLER_1_INSERTED) ? format_type( identify_accessory( 0 ) ) : "" );
        printf( "Accessory 2 %spresent %s\n", (accessories & CONTROLLER_2_INSERTED) ? "" : "not ",
                                              (accessories & CONTROLLER_2_INSERTED) ? format_type( identify_accessory( 1 ) ) : "" );
        printf( "Accessory 3 %spresent %s\n", (accessories & CONTROLLER_3_INSERTED) ? "" : "not ",
                                              (accessories & CONTROLLER_3_INSERTED) ? format_type( identify_accessory( 2 ) ) : "" );
        printf( "Accessory 4 %spresent %s\n", (accessories & CONTROLLER_4_INSERTED) ? "" : "not ",
                                              (accessories & CONTROLLER_4_INSERTED) ? format_type( identify_accessory( 3 ) ) : "" );

        printf("\n%s\n", format_eeprom_type(eeprom));
        printf("\n%s\n", format_rtc_status(rtc));
        if( rtc )
        {
            rtc_read_time( &time );
            printf("%04d-%02d-%02d %02d:%02d:%02d\n", time.year, time.month+1, time.day, time.hour, time.min, time.sec);
        }

        printf("\n%d\n\n", testv++ );

        for( int i = 0; i < 32; i++ )
        {
            printf( "%02X", data[i] );
        }

        printf( "\n\n" );
        printf( "Operation returned: %d\n", press );

        console_render();
    }
}
