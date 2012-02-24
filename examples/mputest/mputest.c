#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

int main(void)
{
    /* enable interrupts (on the CPU) */
    init_interrupts();

    /* Initialize peripherals */
    console_init();
    controller_init();

    console_set_render_mode(RENDER_MANUAL);

    console_clear();

    console_printf( "Press A on a controller\n"
                    "to read that controller's\n"
                    "mempak.\n\n"
                    "Press B to format mempak.\n\n"
                    "Press L to read first valid entry.\n\n"
                    "Press R to write a new entry.\n\n"
                    "Press S to delete first valid entry.\n\n" );
    
    console_render();

    /* Main loop test */
    while(1) 
    {
        /* To do initialize routines */
        controller_scan();

        struct controller_data keys = get_keys_down();

        for( int i = 0; i < 4; i++ )
        {
            if( keys.c[i].err == ERROR_NONE )
            {
                if( keys.c[i].A )
                {
                    console_clear();

                    /* Read accessories present, throwing away return.  If we don't do this, then
                       initialization routines in the identify_accessory() call will fail once we
                       remove and insert a new accessory while running */
                    get_accessories_present();

                    /* Make sure they don't have a rumble pak inserted instead */
                    switch( identify_accessory( i ) )
                    {
                        case ACCESSORY_NONE:
                            console_printf( "No accessory inserted!" );
                            break;
                        case ACCESSORY_MEMPAK:
                        {
                            int err;
                            if( (err = validate_mempak( i )) )
                            {
                                if( err == -3 )
                                {
                                    console_printf( "Mempak is not formatted!" );
                                }
                                else
                                {
                                    console_printf( "Mempak bad or removed during read!" );
                                }
                            }
                            else
                            {
                                for( int j = 0; j < 16; j++ )
                                {
                                    entry_structure_t entry;

                                    get_mempak_entry( i, j, &entry );

                                    if( entry.valid )
                                    {
                                        console_printf( "%s - %d blocks\n", entry.name, entry.blocks );
                                    }
                                    else
                                    {
                                        console_printf( "(EMPTY)\n" );
                                    }
                                }

                                console_printf( "\nFree space: %d blocks", get_mempak_free_space( i ) );
                            }

                            break;
                        }
                        case ACCESSORY_RUMBLEPAK:
                            console_printf( "Cannot read data off of rumblepak!" );
                            break;
                    }

                    console_render();
                }
                else if( keys.c[i].B )
                {
                    console_clear();

                    /* Make sure they don't have a rumble pak inserted instead */
                    switch( identify_accessory( i ) )
                    {
                        case ACCESSORY_NONE:
                            console_printf( "No accessory inserted!" );
                            break;
                        case ACCESSORY_MEMPAK:
                            if( format_mempak( i ) )
                            {
                                console_printf( "Error formatting mempak!" );
                            }
                            else
                            {
                                console_printf( "Memory card formatted!" );
                            }

                            break;
                        case ACCESSORY_RUMBLEPAK:
                            console_printf( "Cannot format rumblepak!" );
                            break;
                    }

                    console_render();
                }
                else if( keys.c[i].L )
                {
                    console_clear();

                    /* Make sure they don't have a rumble pak inserted instead */
                    switch( identify_accessory( i ) )
                    {
                        case ACCESSORY_NONE:
                            console_printf( "No accessory inserted!" );
                            break;
                        case ACCESSORY_MEMPAK:
                        {
                            int err;
                            if( (err = validate_mempak( i )) )
                            {
                                if( err == -3 )
                                {
                                    console_printf( "Mempak is not formatted!" );
                                }
                                else
                                {
                                    console_printf( "Mempak bad or removed during read!" );
                                }
                            }
                            else
                            {
                                for( int j = 0; j < 16; j++ )
                                {
                                    entry_structure_t entry;

                                    get_mempak_entry( i, j, &entry );

                                    if( entry.valid )
                                    {
                                        uint8_t *data = malloc( entry.blocks * MEMPAK_BLOCK_SIZE );

                                        console_printf( "Reading %s - %d blocks\n", entry.name, entry.blocks );
                                        console_printf( "Return: %d\n", read_mempak_entry_data( i, &entry, data ) );

                                        int new = 0;
                                        for( int k = 0; k < (12 * 12); k++ )
                                        {
                                            console_printf( "%02X", data[k] );

                                            new++;
                                            if( new == 12 ) 
                                            { 
                                                console_printf( "\n" ); new = 0; 
                                            }
                                        }

                                        free( data );
                                        break;
                                    }
                                }
                            }

                            break;
                        }
                        case ACCESSORY_RUMBLEPAK:
                            console_printf( "Cannot erase data off of rumblepak!" );
                            break;
                    }

                    console_render();
                }
                else if( keys.c[i].R )
                {
                    console_clear();

                    /* Make sure they don't have a rumble pak inserted instead */
                    switch( identify_accessory( i ) )
                    {
                        case ACCESSORY_NONE:
                            console_printf( "No accessory inserted!" );
                            break;
                        case ACCESSORY_MEMPAK:
                        {
                            int err;
                            if( (err = validate_mempak( i )) )
                            {
                                if( err == -3 )
                                {
                                    console_printf( "Mempak is not formatted!" );
                                }
                                else
                                {
                                    console_printf( "Mempak bad or removed during write!" );
                                }
                            }
                            else
                            {
                                for( int j = 0; j < 16; j++ )
                                {
                                    entry_structure_t entry;

                                    get_mempak_entry( i, j, &entry );

                                    if( !entry.valid )
                                    {
                                        uint8_t *data = malloc( MEMPAK_BLOCK_SIZE );
                                        for( int k = 0; k < MEMPAK_BLOCK_SIZE; k++ )
                                        {
                                            data[k] = k & 0xFF;
                                        }

                                        strcpy( entry.name, "TEST ENTRY.Z" );
                                        entry.blocks = 1;
                                        entry.region = 0x45;

                                        console_printf( "Writing %s - %d blocks\n", entry.name, entry.blocks );
                                        console_printf( "Return: %d\n", write_mempak_entry_data( i, &entry, data ) );

                                        free( data );
                                        break;
                                    }
                                }
                            }

                            break;
                        }
                        case ACCESSORY_RUMBLEPAK:
                            console_printf( "Cannot erase data off of rumblepak!" );
                            break;
                    }

                    console_render();
                }
                else if( keys.c[i].start )
                {
                    console_clear();

                    /* Make sure they don't have a rumble pak inserted instead */
                    switch( identify_accessory( i ) )
                    {
                        case ACCESSORY_NONE:
                            console_printf( "No accessory inserted!" );
                            break;
                        case ACCESSORY_MEMPAK:
                        {
                            int err;
                            if( (err = validate_mempak( i )) )
                            {
                                if( err == -3 )
                                {
                                    console_printf( "Mempak is not formatted!" );
                                }
                                else
                                {
                                    console_printf( "Mempak bad or removed during erase!" );
                                }
                            }
                            else
                            {
                                for( int j = 0; j < 16; j++ )
                                {
                                    entry_structure_t entry;

                                    get_mempak_entry( i, j, &entry );

                                    if( entry.valid )
                                    {
                                        console_printf( "Deleting %s - %d blocks\n", entry.name, entry.blocks );
                                        console_printf( "Return: %d\n", delete_mempak_entry( i, &entry ) );

                                        break;
                                    }
                                }
                            }

                            break;
                        }
                        case ACCESSORY_RUMBLEPAK:
                            console_printf( "Cannot erase data off of rumblepak!" );
                            break;
                    }

                    console_render();
                }
            }
        }
    }
}
