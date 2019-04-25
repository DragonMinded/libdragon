#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

static uint8_t mempak_data[128 * MEMPAK_BLOCK_SIZE];

int main(void)
{
    /* enable interrupts (on the CPU) */
    init_interrupts();

    /* Initialize peripherals */
    console_init();
    controller_init();

    console_set_render_mode(RENDER_MANUAL);

    console_clear();

    printf( "Press A on a controller\n"
                    "to read that controller's\n"
                    "mempak.\n\n"
                    "Press B to format mempak.\n\n"
                    "Press Z to corrupt mempak.\n\n"
                    "Press L to copy mempak.\n\n"
                    "Press R to paste mempak." );
    
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
                    struct controller_data output;
                    get_accessories_present( &output );

                    /* Make sure they don't have a rumble pak inserted instead */
                    switch( identify_accessory( i ) )
                    {
                        case ACCESSORY_NONE:
                            printf( "No accessory inserted!" );
                            break;
                        case ACCESSORY_MEMPAK:
                        {
                            int err;
                            if( (err = validate_mempak( i )) )
                            {
                                if( err == -3 )
                                {
                                    printf( "Mempak is not formatted!" );
                                }
                                else
                                {
                                    printf( "Mempak bad or removed during read!" );
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
                                        printf( "%s - %d blocks\n", entry.name, entry.blocks );
                                    }
                                    else
                                    {
                                        printf( "(EMPTY)\n" );
                                    }
                                }

                                printf( "\nFree space: %d blocks", get_mempak_free_space( i ) );
                            }

                            break;
                        }
                        case ACCESSORY_RUMBLEPAK:
                            printf( "Cannot read data off of rumblepak!" );
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
                            printf( "No accessory inserted!" );
                            break;
                        case ACCESSORY_MEMPAK:
                            if( format_mempak( i ) )
                            {
                                printf( "Error formatting mempak!" );
                            }
                            else
                            {
                                printf( "Memory card formatted!" );
                            }

                            break;
                        case ACCESSORY_RUMBLEPAK:
                            printf( "Cannot format rumblepak!" );
                            break;
                    }

                    console_render();
                }
                else if( keys.c[i].Z )
                {
                    console_clear();

                    /* Make sure they don't have a rumble pak inserted instead */
                    switch( identify_accessory( i ) )
                    {
                        case ACCESSORY_NONE:
                            printf( "No accessory inserted!" );
                            break;
                        case ACCESSORY_MEMPAK:
                        {
                            uint8_t sector[256];
                            int err = 0;

                            memset( sector, 0xFF, 256 );

                            err |= write_mempak_sector( i, 0, sector );
                            err |= write_mempak_sector( i, 1, sector );
                            err |= write_mempak_sector( i, 2, sector );
                            err |= write_mempak_sector( i, 3, sector );
                            err |= write_mempak_sector( i, 4, sector );

                            if( err )
                            {
                                printf( "Error corrupting data!" );
                            }
                            else
                            {
                                printf( "Data corrupted on memory card!" );
                            }

                            break;
                        }
                        case ACCESSORY_RUMBLEPAK:
                            printf( "Cannot erase data off of rumblepak!" );
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
                            printf( "No accessory inserted!" );
                            break;
                        case ACCESSORY_MEMPAK:
                        {
                            int err = 0;

                            for( int j = 0; j < 128; j++ )
                            {
                                err |= read_mempak_sector( i, j, &mempak_data[j * MEMPAK_BLOCK_SIZE]  );
                            }

                            if( err )
                            {
                                printf( "Error loading data!" );
                            }
                            else
                            {
                                printf( "Data loaded into RAM!" );
                            }

                            break;
                        }
                        case ACCESSORY_RUMBLEPAK:
                            printf( "Cannot erase data off of rumblepak!" );
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
                            printf( "No accessory inserted!" );
                            break;
                        case ACCESSORY_MEMPAK:
                        {
                            int err = 0;

                            for( int j = 0; j < 128; j++ )
                            {
                                err |= write_mempak_sector( i, j, &mempak_data[j * MEMPAK_BLOCK_SIZE]  );
                            }

                            if( err )
                            {
                                printf( "Error saving data!" );
                            }
                            else
                            {
                                printf( "Data saved into mempak!" );
                            }

                            break;
                        }
                        case ACCESSORY_RUMBLEPAK:
                            printf( "Cannot erase data off of rumblepak!" );
                            break;
                    }

                    console_render();
                }
            }
        }
    }
}
