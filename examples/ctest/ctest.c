#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
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

const char * format_flashram_type( flashram_type_t flashram )
{
    switch( flashram )
    {
        case FLASHRAM_TYPE_NONE:
            return "not detected";
        case FLASHRAM_TYPE_MX29L0000:
            return "identified: Macronix MX29L0000";
        case FLASHRAM_TYPE_MX29L0001:
            return "identified: Macronix MX29L0001";
        case FLASHRAM_TYPE_MX29L1100:
            return "identified: Macronix MX29L1100";
        case FLASHRAM_TYPE_MX29L1101_A:
            return "identified: Macronix MX29L1101";
        case FLASHRAM_TYPE_MX29L1101_B:
        case FLASHRAM_TYPE_MX29L1101_C:
            return "identified: 29L1100KC-15B0 (MX29L1101 compatible)";
        case FLASHRAM_TYPE_MN63F81MPN:
            return "identified: Matsushita MN63F81MPN";
        default:
            return "not identified";
    }
}

void print_save_type( uint8_t save_type, flashram_type_t flashram )
{
    if( save_type == SAVE_TYPE_NONE )
        printf( "No cartridge save detected\n" );
    if( save_type & SAVE_TYPE_EEPROM_4KBIT )
        printf( "4Kbit EEPROM detected\n" );
    if( save_type & SAVE_TYPE_EEPROM_16KBIT )
        printf( "16Kbit EEPROM detected\n" );
    if( save_type & SAVE_TYPE_SRAM_256KBIT )
        printf( "256Kbit SRAM detected\n" );
    if( save_type & SAVE_TYPE_SRAM_768KBIT_BANKED )
        printf( "768Kbit banked SRAM detected\n" );
    if( save_type & SAVE_TYPE_SRAM_1MBIT_BANKED )
        printf( "1Mbit banked SRAM detected\n" );
    if( save_type & SAVE_TYPE_SRAM_768KBIT_CONTIGUOUS )
        printf( "768Kbit contiguous SRAM detected\n" );
    if( save_type & SAVE_TYPE_SRAM_1MBIT_CONTIGUOUS )
        printf( "1Mbit contiguous SRAM detected\n" );
    if( flashram )
        printf( "1Mbit FlashRAM %s\n", format_flashram_type( flashram ) );
}

int main(void)
{
    /* enable interrupts (on the CPU) */
    init_interrupts();

    /* Initialize peripherals */
    display_init( res, bit, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );
    console_init();
    controller_init();
    timer_init();
    rtc_init();

    console_set_render_mode(RENDER_MANUAL);

    time_t current_time = -1;
    int testv = 0;
    int press = 0;
    uint8_t data[32];
    memset( data, 0, 32 );

    uint8_t save_type = cart_detect_save_type();
    flashram_type_t flashram = cart_detect_flashram();

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

        printf( "\n" );
        print_save_type( save_type, flashram );

        printf( "\n%d\n\n", testv++ );

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
