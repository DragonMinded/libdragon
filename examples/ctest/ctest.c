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

const char * format_save_type( cart_save_type_t save_type )
{
    switch(save_type)
    {
        case SAVE_TYPE_NONE:
            return "No cartridge save detected";
        case SAVE_TYPE_EEPROM_4KBIT:
            return "4 kilobit EEPROM detected";
        case SAVE_TYPE_EEPROM_16KBIT:
            return "16 kilobit EEPROM detected";
        case SAVE_TYPE_SRAM_256KBIT:
            return "256 kilobit SRAM detected";
        case SAVE_TYPE_SRAM_768KBIT:
            return "768 kilobit SRAM detected";
        case SAVE_TYPE_FLASHRAM_1MBIT:
            return "1 megabit FlashRAM detected";
        case SAVE_TYPE_SRAM_1MBIT_CONTIGUOUS:
            return "1 megabit contiguous SRAM detected";
        case SAVE_TYPE_SRAM_1MBIT_BANKED:
            return "1 megabit banked SRAM detected";
        default:
            return "Unknown cartridge save type";
    }
}

const char * format_flashram_type( flashram_type_t flashram )
{
    switch( flashram )
    {
        case FLASHRAM_TYPE_NONE:
            return "FlashRAM not detected";
        case FLASHRAM_TYPE_MX29L0000:
            return "FlashRAM identified: Macronix MX29L0000";
        case FLASHRAM_TYPE_MX29L0001:
            return "FlashRAM identified: Macronix MX29L0001";
        case FLASHRAM_TYPE_MX29L1100:
            return "FlashRAM identified: Macronix MX29L1100";
        case FLASHRAM_TYPE_MX29L1101_A:
            return "FlashRAM identified: Macronix MX29L1101";
        case FLASHRAM_TYPE_MX29L1101_B:
        case FLASHRAM_TYPE_MX29L1101_C:
            return "FlashRAM identified: 29L1100KC-15B0 (MX29L1101 compatible)";
        case FLASHRAM_TYPE_MN63F81MPN:
            return "FlashRAM identified: Matsushita MN63F81MPN";
        default:
            return "Unknown FlashRAM detected";
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
    timer_init();
    rtc_init();

    console_set_render_mode(RENDER_MANUAL);

    time_t current_time = -1;
    int testv = 0;
    int press = 0;
    uint8_t data[32];
    memset( data, 0, 32 );

    cart_save_type_t save_type = cart_detect_save_type();
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

        printf("\n%s\n", format_save_type(save_type));
        if( save_type == SAVE_TYPE_FLASHRAM_1MBIT )
        {
            printf("%s\n", format_flashram_type(flashram));
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
