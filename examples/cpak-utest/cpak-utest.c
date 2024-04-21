#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

int main(void)
{
    /* Initialize peripherals */
    console_init();
    joypad_init();

    console_set_render_mode(RENDER_MANUAL);

    console_clear();

    printf(
            "To test an inserted\n"
            "ControllerPak (mempak):\n\n"
            "Press A to validate Pak.\n\n"
            "Press B to format Pak.\n\n"
            "Press R to create entry.\n\n"
            "Press L to get entries.\n\n"
            "Press START to delete entry."
            );
    
    console_render();

    /* Main loop test */
    while (1) 
    {
        /* To do initialize routines */
        joypad_poll();

        JOYPAD_PORT_FOREACH(port)
        {
            joypad_buttons_t keys = joypad_get_buttons_pressed(port);

            if (keys.a)
            {
                /* Make sure they don't have a Rumble Pak inserted instead */
                switch (joypad_get_accessory_type(port))
                {
                    case JOYPAD_ACCESSORY_TYPE_NONE:
                        printf("No accessory inserted!");
                        break;
                    case JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK:
                    {
                        int err;
                        if ((err = validate_mempak(port)))
                        {
                            if (err == -3)
                            {
                                printf( "Controller Pak is not formatted!" );
                            }
                            else
                            {
                                printf( "Controller Pak bad or removed during read!" );
                            }
                        }
                        else
                        {
                            for (int j = 0; j < 16; j++)
                            {
                                entry_structure_t entry;

                                get_mempak_entry(port, j, &entry);

                                if (entry.valid)
                                {
                                    printf("%s - %d blocks\n", entry.name, entry.blocks);
                                }
                                else
                                {
                                    printf("(EMPTY)\n");
                                }
                            }

                            printf( "\nFree space: %d blocks", get_mempak_free_space(port ) );
                        }

                        break;
                    }
                    default:
                        printf( "Cannot read data from this accessory!" );
                        break;
                }

                console_render();
            }
            else if (keys.b)
            {
                console_clear();

                /* Make sure they don't have a Rumble Pak inserted instead */
                switch (joypad_get_accessory_type(port))
                {
                    case JOYPAD_ACCESSORY_TYPE_NONE:
                        printf("No accessory inserted!");
                        break;
                    case JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK:
                        if (format_mempak(port))
                        {
                            printf( "Error formatting Controller Pak!" );
                        }
                        else
                        {
                            printf( "Controller Pak formatted!" );
                        }

                        break;
                    default:
                        printf("Cannot format this accessory!");
                        break;
                }

                console_render();
            }
            else if (keys.l)
            {
                console_clear();

                /* Make sure they don't have a rumble pak inserted instead */
                switch (joypad_get_accessory_type(port))
                {
                    case JOYPAD_ACCESSORY_TYPE_NONE:
                        printf("No accessory inserted!");
                        break;
                    case JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK:
                    {
                        int err;
                        if ((err = validate_mempak(port)))
                        {
                            if (err == -3)
                            {
                                printf( "Controller Pak is not formatted!" );
                            }
                            else
                            {
                                printf( "Controller Pak bad or removed during read!" );
                            }
                        }
                        else
                        {
                            for (int j = 0; j < 16; j++ )
                            {
                                entry_structure_t entry;

                                get_mempak_entry(port, j, &entry);

                                if (entry.valid)
                                {
                                    uint8_t *data = malloc( entry.blocks * MEMPAK_BLOCK_SIZE );

                                    printf( "Reading %s - %d blocks\n", entry.name, entry.blocks);
                                    printf( "Return: %d\n", read_mempak_entry_data(port, &entry, data));

                                    int new = 0;
                                    for (int k = 0; k < (12 * 12); k++)
                                    {
                                        printf( "%02X", data[k] );

                                        new++;
                                        if (new == 12) 
                                        { 
                                            printf( "\n" ); new = 0; 
                                        }
                                    }

                                    free(data);
                                    break;
                                }
                            }
                        }

                        break;
                    }
                    default:
                        printf("Cannot read data from this accessory!");
                        break;
                }

                console_render();
            }
            else if (keys.r)
            {
                console_clear();

                /* Make sure they don't have a rumble pak inserted instead */
                switch (joypad_get_accessory_type(port))
                {
                    case JOYPAD_ACCESSORY_TYPE_NONE:
                        printf("No accessory inserted!");
                        break;
                    case JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK:
                    {
                        int err;
                        if ((err = validate_mempak(port)))
                        {
                            if (err == -3)
                            {
                                printf( "Controller Pak is not formatted!" );
                            }
                            else
                            {
                                printf( "Controller Pak bad or removed during write!" );
                            }
                        }
                        else
                        {
                            for (int j = 0; j < 16; j++)
                            {
                                entry_structure_t entry;

                                get_mempak_entry(port, j, &entry);

                                if (!entry.valid)
                                {
                                    uint8_t *data = malloc( MEMPAK_BLOCK_SIZE );
                                    for (int k = 0; k < MEMPAK_BLOCK_SIZE; k++)
                                    {
                                        data[k] = k & 0xFF;
                                    }

                                    strcpy( entry.name, "TEST ENTRY.Z" );
                                    entry.blocks = 1;
                                    entry.region = 0x45;

                                    printf( "Writing %s - %d blocks\n", entry.name, entry.blocks);
                                    printf( "Return: %d\n", write_mempak_entry_data(port, &entry, data));

                                    free(data);
                                    break;
                                }
                            }
                        }

                        break;
                    }
                    default:
                        printf("Cannot write data to this accessory!");
                        break;
                }

                console_render();
            }
            else if (keys.start)
            {
                console_clear();

                /* Make sure they don't have a rumble pak inserted instead */
                switch (joypad_get_accessory_type(port))
                {
                    case JOYPAD_ACCESSORY_TYPE_NONE:
                        printf("No accessory inserted!");
                        break;
                    case JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK:
                    {
                        int err;
                        if ((err = validate_mempak(port)))
                        {
                            if (err == -3)
                            {
                                printf( "Controller Pak is not formatted!" );
                            }
                            else
                            {
                                printf( "Controller Pak bad or removed during erase!" );
                            }
                        }
                        else
                        {
                            for (int j = 0; j < 16; j++)
                            {
                                entry_structure_t entry;

                                get_mempak_entry(port, j, &entry);

                                if (entry.valid)
                                {
                                    printf( "Deleting %s - %d blocks\n", entry.name, entry.blocks);
                                    printf( "Return: %d\n", delete_mempak_entry(port, &entry));

                                    break;
                                }
                            }
                        }

                        break;
                    }
                    default:
                        printf("Cannot erase data from this accessory!");
                        break;
                }

                console_render();
            }
        }
    }
}
