#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

static uint8_t mempak_data[128 * MEMPAK_BLOCK_SIZE];

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
            "Press A to read Pak.\n\n"
            "Press B to format Pak.\n\n"
            "Press Z to corrupt Pak.\n\n"
            "Press L to copy Pak.\n\n" /* CPak to RAM */
            "Press R to paste Pak." /* RAM to CPak */
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
                console_clear();

                /* Make sure they don't have a Rumble Pak inserted instead */
                switch (joypad_get_accessory_type(port))
                {
                    case JOYPAD_ACCESSORY_TYPE_NONE:
                        printf("No accessory inserted!");
                        break;
                    case JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK:
                    {
                        int err;
                        if ( (err = validate_mempak(port)) )
                        {
                            if (err == -3)
                            {
                                printf("Pak is not formatted!");
                            }
                            else
                            {
                                printf("Pak bad or removed during read!");
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

                            printf("\nFree space: %d blocks", get_mempak_free_space(port));
                        }

                        break;
                    }
                    default:
                        printf("Cannot read data from this accessory!");
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
                            printf( "Error formatting Pak!" );
                        }
                        else
                        {
                            printf("Pak formatted!");
                        }

                        break;
                    default:
                        printf("Cannot format this accessory!");
                        break;
                }

                console_render();
            }
            else if (keys.z)
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
                        uint8_t sector[256];
                        int err = 0;

                        memset(sector, 0xFF, 256);

                        err |= write_mempak_sector(port, 0, sector);
                        err |= write_mempak_sector(port, 1, sector);
                        err |= write_mempak_sector(port, 2, sector);
                        err |= write_mempak_sector(port, 3, sector);
                        err |= write_mempak_sector(port, 4, sector);

                        if (err)
                        {
                            printf( "Error corrupting data!" );
                        }
                        else
                        {
                            printf( "Data corrupted on Pak!" );
                        }

                        break;
                    }
                    default:
                        printf("Cannot erase data from this accessory!");
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
                        int err = 0;

                        for (int j = 0; j < 128; j++)
                        {
                            err |= read_mempak_sector(port, j, &mempak_data[j * MEMPAK_BLOCK_SIZE]);
                        }

                        if (err)
                        {
                            printf("Error loading data!");
                        }
                        else
                        {
                            printf("Data loaded into RAM!");
                        }

                        break;
                    }
                    default:
                        printf("Cannot copy data from this accessory!");
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
                        int err = 0;

                        for (int j = 0; j < 128; j++)
                        {
                            err |= write_mempak_sector(port, j, &mempak_data[j * MEMPAK_BLOCK_SIZE]);
                        }

                        if (err)
                        {
                            printf("Error saving data!");
                        }
                        else
                        {
                            printf("Data saved onto Pak!");
                        }

                        break;
                    }
                    default:
                        printf("Cannot paste data to this accessory!");
                        break;
                }

                console_render();
            }
        }
    }
}
