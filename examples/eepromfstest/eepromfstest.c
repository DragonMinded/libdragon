#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

typedef struct
{
    char initials[3];
    uint32_t score;
} game_high_score_t;

#define NUM_HIGH_SCORES 10

static game_high_score_t game_high_scores[NUM_HIGH_SCORES];

typedef struct
{
    bool initialized;
    uint8_t language;
    uint8_t sfx_volume;
    uint8_t music_volume;
} game_settings_t;

typedef struct
{
    bool initialized;
    char name[16];
    uint32_t score;
    uint8_t map_index;
    int16_t map_pos_x;
    int16_t map_pos_y;
    uint16_t max_hp;
    uint16_t current_hp;
    uint8_t inventory[256];
} game_save_state_t;

static void press_a_to_continue(void)
{
    printf( "Press A to continue\n" );
    struct controller_data keys;
    while (1)
    {
        controller_scan();
        keys = get_keys_down();
        if (keys.c[0].A)
        {
            console_clear();
            break;
        }
    }
}

static void keep_or_erase_data(void)
{
    printf( "Press A to keep EEPROM data\n" );
    printf( "Press B to erase EEPROM data\n" );
    struct controller_data keys;
    while (1)
    {
        controller_scan();
        keys = get_keys_down();
        if (keys.c[0].A)
        {
            console_clear();
            break;
        }
        if (keys.c[0].B)
        {   
            printf( "Wiping EEPROM...\n" );
            eepfs_wipe();
            printf( "EEPROM has been erased...\n" );
            press_a_to_continue();
            break;
        }
    }
}

static void print_game_high_scores(void)
{
    game_high_score_t *entry;
    printf( "(game_high_score_t[%d]){\n", NUM_HIGH_SCORES );
    for (size_t i = 0; i < NUM_HIGH_SCORES; ++i)
    {
        entry = &game_high_scores[i];
        printf( "  { \"%.3s\", %lu }\n", entry->initials, entry->score );
    }
    printf( "};\n" );
}

static int read_game_high_scores(char * path)
{
    printf( "Reading '%s'\n", path );
    const int result = eepfs_read(path, &game_high_scores, sizeof(game_high_scores));

    if ( result != EEPFS_ESUCCESS )
    {
        printf( "Read failed; error %d\n", result );
        return 1;
    }

    print_game_high_scores();
    printf( "\n" );
 
    return 0;
}

static int write_game_high_scores(char * path)
{
    game_high_scores[0] = (game_high_score_t){ "CDB", 4294967295 };
    game_high_scores[1] = (game_high_score_t){ "AAA", 16777215 };
    game_high_scores[2] = (game_high_score_t){ "XYZ", 65535 };
    game_high_scores[3] = (game_high_score_t){ "ME", 255 };
    game_high_scores[4] = (game_high_score_t){ "Q", 1 };

    printf( "Writing '%s'\n", path );
    const int result = eepfs_write(path, &game_high_scores, sizeof(game_high_scores));

    if ( result != EEPFS_ESUCCESS )
    {
        printf( "Write failed; error %d\n", result );
        return 1;
    }

    print_game_high_scores();
    printf( "\n" );

    return 0;
}

static int validate_game_high_scores(char * path)
{
    int result;

    result = read_game_high_scores(path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    result = write_game_high_scores(path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    result = read_game_high_scores(path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    return 0;
}

static void print_game_settings(const game_settings_t *gs)
{
    printf( "(game_settings_t){\n" );
    printf( "  .initialized = %s,\n", gs->initialized ? "true" : "false" );
    printf( "  .language = %u,\n", gs->language );
    printf( "  .sfx_volume = %u,\n", gs->sfx_volume );
    printf( "  .music_volume = %u,\n", gs->music_volume );
    printf( "};\n" );
}

static int read_game_settings(char * path)
{
    game_settings_t game_settings;

    printf( "Reading '%s'\n", path );
    const int result = eepfs_read(path, &game_settings, sizeof(game_settings));

    if ( result != EEPFS_ESUCCESS )
    {
        printf( "Read failed; error %d\n", result );
        return 1;
    }

    print_game_settings(&game_settings);
    printf( "\n" );
 
    return 0;
}

static int write_game_settings(char * path)
{
    const game_settings_t game_settings = { true, 2, 255, 128 };

    printf( "Writing '%s'\n", path );
    const int result = eepfs_write(path, &game_settings, sizeof(game_settings));

    if ( result != EEPFS_ESUCCESS )
    {
        printf( "Write failed; error %d\n", result );
        return 1;
    }

    print_game_settings(&game_settings);
    printf( "\n" );

    return 0;
}

static int validate_game_settings(char * path)
{
    int result;

    result = read_game_settings(path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    result = write_game_settings(path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    result = read_game_settings(path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    return 0;
}

static void print_game_save_state(const game_save_state_t *save)
{
    uint8_t item;
    printf( "(game_save_state_t){\n" );
    printf( "  .initialized = %s,\n", save->initialized ? "true" : "false" );
    printf( "  .name = \"%.16s\",\n", save->name );
    printf( "  .map_index = %u,\n", save->map_index );
    printf( "  .map_pos_x = %d,\n", save->map_pos_x );
    printf( "  .map_pos_y = %d,\n", save->map_pos_y );
    printf( "  .max_hp = %u,\n", save->max_hp );
    printf( "  .current_hp = %u,\n", save->current_hp );
    printf( "  .inventory = {\n" );
    for (size_t i = 0; i < 256; ++i)
    {
        item = save->inventory[i];
        if (item)
        {
            printf( "    [%u] = %u,\n", i, item );
        }
    }
    printf( "  }\n" );
    printf( "};\n" );
}

static int read_game_save_state(char * path)
{
    game_save_state_t game_save_state;

    printf( "Reading '%s'\n", path );
    const int result = eepfs_read(path, &game_save_state, sizeof(game_save_state));

    if ( result != EEPFS_ESUCCESS )
    {
        printf( "Read failed; error %d\n", result );
        return 1;
    }

    print_game_save_state(&game_save_state);
    printf( "\n" );
 
    return 0;
}

static int write_game_save_state(char * path)
{
    game_save_state_t game_save_state = { 
        1,
        "Dragon",
        500,
        1,
        87,
        -120,
        40,
        36,
        { 1, 22, [254] = 52 }
    };

    printf( "Writing '%s'\n", path );
    const int result = eepfs_write(path, &game_save_state, sizeof(game_save_state));

    if ( result != EEPFS_ESUCCESS )
    {
        printf( "Write failed; error %d\n", result );
        return 1;
    }

    print_game_save_state(&game_save_state);
    printf( "\n" );

    return 0;
}

static int validate_game_save_state(char * path)
{
    int result;

    result = read_game_save_state(path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    result = write_game_save_state(path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    result = read_game_save_state(path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    return 0;
}

static int validate_eeprom_4k(void)
{
    int result;
    const eepfs_entry_t eeprom_4k_files[] = {
        { "/high_scores.dat", sizeof(game_high_scores)  },
        { "/settings.dat",    sizeof(game_settings_t)   },
        { "/player.sav",      sizeof(game_save_state_t) }
    };

    printf( "EEPROM Detected: 4 Kibit (64 blocks)\n" );
    printf( "Initializing EEPROM Filesystem...\n" );
    result = eepfs_init(eeprom_4k_files, 3);
    switch ( result )
    {
        case EEPFS_ESUCCESS:
            printf( "Success!\n" );
            break;
        case EEPFS_EBADFS:
            printf( "Failed with error: bad filesystem\n" );
            return 1;
        case EEPFS_ENOMEM: 
            printf( "Failed with error: not enough memory\n" );
            return 1;
        default:
            printf( "Failed in an unexpected manner\n" );
            return 1;
    }

    /* Check if the EEPROM is roughly compatible with the filesystem */
    if ( !eepfs_verify_signature() )
    {
        /* If not, erase it and start from scratch */
        printf( "Filesystem signature is invalid!\n" );
        printf( "Wiping EEPROM...\n" );
        eepfs_wipe();
        press_a_to_continue();
    }
    else
    {
        /* Offer to erase EEPROM before running the tests */
        printf( "Filesystem signature OK!\n" );
        keep_or_erase_data();
    }

    result = validate_game_high_scores("/high_scores.dat");
    if ( result != 0 ) return result;

    result = validate_game_settings("/settings.dat");
    if (result != 0) return result;

    result = validate_game_save_state("player.sav");
    if (result != 0) return result;

    eepfs_close();

    return 0;
}

static int validate_eeprom_16k(void)
{
    int result;
    const eepfs_entry_t eeprom_16k_files[] = {
        { "high_scores.dat", sizeof(game_high_scores)  },
        { "settings.dat",    sizeof(game_settings_t)   },
        { "saves/slot1.sav", sizeof(game_save_state_t) },
        { "saves/slot2.sav", sizeof(game_save_state_t) },
        { "saves/slot3.sav", sizeof(game_save_state_t) },
        { "saves/slot4.sav", sizeof(game_save_state_t) }
    };

    printf( "EEPROM Detected: 16 Kibit (256 blocks)\n" );
    printf( "Initializing EEPROM Filesystem...\n" );
    result = eepfs_init(eeprom_16k_files, 6);
    switch ( result )
    {
        case EEPFS_ESUCCESS:
            printf( "Success!\n" );
            break;
        case EEPFS_EBADFS:
            printf( "Failed with error: bad filesystem\n" );
            return 1;
        case EEPFS_ENOMEM: 
            printf( "Failed with error: not enough memory\n" );
            return 1;
        default:
            printf( "Failed in an unexpected manner\n" );
            return 1;
    }

    /* Check if the EEPROM is roughly compatible with the filesystem */
    if ( !eepfs_verify_signature() )
    {
        /* If not, erase it and start from scratch */
        printf( "Filesystem signature is invalid!\n" );
        printf( "Wiping EEPROM...\n" );
        eepfs_wipe();
        press_a_to_continue();
    }
    else
    {
        /* Offer to erase EEPROM before running the tests */
        printf( "Filesystem signature OK!\n" );
        keep_or_erase_data();
    }

    result = validate_game_high_scores("high_scores.dat");
    if ( result != 0 ) return result;

    result = validate_game_settings("/settings.dat");
    if (result != 0) return result;

    result = validate_game_save_state("/saves/slot1.sav");
    if (result != 0) return result;

    result = validate_game_save_state("saves/slot2.sav");
    if (result != 0) return result;

    result = validate_game_save_state("saves/slot3.sav");
    if (result != 0) return result;

    result = validate_game_save_state("saves/slot4.sav");
    if (result != 0) return result;

    eepfs_close();

    return 0;
}

static int validate_eeprom(eeprom_type_t eeprom_type)
{
    switch ( eeprom_type )
    {
        case EEPROM_4K:
            return validate_eeprom_4k();
        case EEPROM_16K:
            return validate_eeprom_16k();
        default:
            printf( "No EEPROM detected!\n\n" );
            printf( "Make sure the save type is\n" );
            printf( "configured correctly in your\n" );
            printf( "emulator or flashcart.\n\n" );
            return 1;
    }
}

int main(void)
{
    /* Initialize peripherals */
    console_init();
    controller_init();

    console_set_render_mode(RENDER_AUTOMATIC);
    console_clear();

    const eeprom_type_t eeprom_type = eeprom_present();

    int result = 0;
    while (1)
    {
        result = validate_eeprom(eeprom_type);
        if ( result != 0 ) break;
        printf( "EEPROM Filesystem test complete!\n" );
        press_a_to_continue();
    }
}
