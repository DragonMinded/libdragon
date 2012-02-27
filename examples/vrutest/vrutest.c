#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

static resolution_t res = RESOLUTION_320x240;
static bitdepth_t bit = DEPTH_32_BPP;

void print_request( int len, uint8_t *res )
{
    for( int i = 0; i < len; i++ )
    {
        printf( "%02X", res[i] );
    }

    printf( " " );
}

void print_result( int len, uint8_t *res )
{
    for( int i = 0; i < len; i++ )
    {
        printf( "%02X", res[i] );
    }

    printf( "\n" );
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

    /* Main loop test */
    while(1) 
    {
        console_clear();

        /* To do initialize routines */
        controller_scan();

        int controllers = get_controllers_present();

        if( controllers & CONTROLLER_1_INSERTED )
        {
            int accessories = get_accessories_present();

            if( (accessories & CONTROLLER_4_INSERTED) && identify_accessory( 3 ) == ACCESSORY_VRU )
            {
                uint8_t out[64];
                uint8_t in[64];

                // Initial sequence
                out[0] = 0x00;
                out[1] = 0x00;
                print_request( 2, out );
                execute_raw_command( 3, 0xB, 2, 3, out, in );
                print_result( 3, in );

                // LODS OF 0x0B
                out[0] = 0x1E;
                out[1] = 0x0C;
                print_request( 2, out );
                execute_raw_command( 3, 0xD, 2, 1, out, in );
                print_result( 1, in );

                out[0] = 0x6E;
                out[1] = 0x07;
                print_request( 2, out );
                execute_raw_command( 3, 0xD, 2, 1, out, in );
                print_result( 1, in );

                out[0] = 0x08;
                out[1] = 0x0E;
                print_request( 2, out );
                execute_raw_command( 3, 0xD, 2, 1, out, in );
                print_result( 1, in );

                out[0] = 0x56;
                out[1] = 0x18;
                print_request( 2, out );
                execute_raw_command( 3, 0xD, 2, 1, out, in );
                print_result( 1, in );

                out[0] = 0x03;
                out[1] = 0x0F;
                print_request( 2, out );
                execute_raw_command( 3, 0xD, 2, 1, out, in );
                print_result( 1, in );

                // Some C's and B's
                out[0] = 0x00;
                out[1] = 0x00;
                out[2] = 0x00;
                out[3] = 0x00;
                out[4] = 0x01;
                out[5] = 0x00;
                print_request( 6, out );
                execute_raw_command( 3, 0xC, 6, 1, out, in );
                print_result( 1, in );

                out[0] = 0x00;
                out[1] = 0x00;
                print_request( 2, out );
                execute_raw_command( 3, 0xB, 2, 3, out, in );
                print_result( 3, in );
                
                out[0] = 0x00;
                out[1] = 0x00;
                out[2] = 0x02;
                out[3] = 0x00;
                out[4] = 0x3B;
                out[5] = 0x00;
                print_request( 6, out );
                execute_raw_command( 3, 0xC, 6, 1, out, in );
                print_result( 1, in );

                out[0] = 0x00;
                out[1] = 0x00;
                print_request( 2, out );
                execute_raw_command( 3, 0xB, 2, 3, out, in );
                print_result( 3, in );

                // Give that A a try
                memset( out, 0, sizeof( out ) );
                print_request( 22, out );
                execute_raw_command( 3, 0xA, 22, 1, out, in );
                print_result( 1, in );

                memset( out, 0, sizeof( out ) );
                print_request( 22, out );
                execute_raw_command( 3, 0xA, 22, 1, out, in );
                print_result( 1, in );

                memset( out, 0, sizeof( out ) );
                out[14] = 0x03;
                out[18] = 0x12;
                out[20] = 0x08;
                print_request( 22, out );
                execute_raw_command( 3, 0xA, 22, 1, out, in );
                print_result( 1, in );

                // Render it!
                console_render();
                while(1){;}
            }
            else
            {
                printf( "Please insert a VRU into slot 4.\n" );
            }
        }
        else
        {
            printf( "Please insert a standard\ncontroller into slot 1.\n" );
        }

        console_render();
    }
}
