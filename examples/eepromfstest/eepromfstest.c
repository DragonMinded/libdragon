#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

static resolution_t res = RESOLUTION_320x240;
static bitdepth_t bit = DEPTH_32_BPP;

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

static void press_a_to_continue()
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

static void keep_or_erase_data()
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

static void print_game_high_scores()
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

static int read_game_high_scores(const char *game_high_scores_path)
{
    FILE *game_high_scores_fp = NULL;
    const size_t entry_size = sizeof(game_high_score_t);

    printf( "Opening file for reading:\n%s\n", game_high_scores_path );
    game_high_scores_fp = fopen(game_high_scores_path, "rb");

    if ( game_high_scores_fp == NULL)
    {
        printf( "Could not open file.\n" );
        return 1;
    }

    printf( "Reading %d bytes...\n", NUM_HIGH_SCORES * entry_size );
    fread(game_high_scores, entry_size, NUM_HIGH_SCORES, game_high_scores_fp);
    fclose(game_high_scores_fp);

    printf( "Read array from EEPROM:\n\n" );
    print_game_high_scores();
    printf( "\n" );
 
    return 0;
}

static int write_game_high_scores(const char *game_high_scores_path)
{
    FILE *game_high_scores_fp = NULL;
    const size_t entry_size = sizeof(game_high_score_t);

    game_high_scores[0] = (game_high_score_t){ "CDB", 4294967295 };
    game_high_scores[1] = (game_high_score_t){ "AAA", 16777215 };
    game_high_scores[2] = (game_high_score_t){ "XYZ", 65535 };
    game_high_scores[3] = (game_high_score_t){ "ME", 255 };
    game_high_scores[4] = (game_high_score_t){ "Q", 1 };

    printf( "Opening file for writing:\n%s\n", game_high_scores_path );
    game_high_scores_fp = fopen(game_high_scores_path, "wb");

    if ( game_high_scores_fp == NULL)
    {
        printf( "Could not open file.\n" );
        return 1;
    }

    printf( "Writing %d bytes...\n", NUM_HIGH_SCORES * entry_size );
    fwrite(game_high_scores, entry_size, NUM_HIGH_SCORES, game_high_scores_fp);
    fclose(game_high_scores_fp);

    printf( "Wrote array to EEPROM:\n\n" );
    print_game_high_scores();
    printf( "\n" );

    return 0;
}

static int validate_game_high_scores(const char * game_high_scores_path)
{
    int result;

    result = read_game_high_scores(game_high_scores_path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    result = write_game_high_scores(game_high_scores_path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    result = read_game_high_scores(game_high_scores_path);
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

static int read_game_settings(const char *game_settings_path)
{
    FILE *game_settings_fp = NULL;
    game_settings_t game_settings_read;
    const size_t size = sizeof(game_settings_t);

    printf( "Opening file for reading:\n%s\n", game_settings_path );
    game_settings_fp = fopen(game_settings_path, "rb");

    if ( game_settings_fp == NULL)
    {
        printf( "Could not open file.\n" );
        return 1;
    }

    printf( "Reading %d bytes from file...\n", size );
    fread(&game_settings_read, size, 1, game_settings_fp);
    fclose(game_settings_fp);

    printf( "Read struct from EEPROM:\n\n" );
    print_game_settings(&game_settings_read);
    printf( "\n" );
 
    return 0;
}

static int write_game_settings(const char *game_settings_path)
{
    FILE *game_settings_fp = NULL;
    const game_settings_t game_settings_write = { true, 2, 255, 128 };
    const size_t size = sizeof(game_settings_t);

    printf( "Opening file for writing:\n%s\n", game_settings_path );
    game_settings_fp = fopen(game_settings_path, "wb");

    if ( game_settings_fp == NULL)
    {
        printf( "Could not open file.\n" );
        return 1;
    }

    printf( "Writing %d bytes to file...\n", size );
    fwrite(&game_settings_write, size, 1, game_settings_fp);
    fclose(game_settings_fp);

    printf( "Wrote struct to EEPROM:\n\n" );
    print_game_settings(&game_settings_write);
    printf( "\n" );

    return 0;
}

static int validate_game_settings(const char *game_settings_path)
{
    int result;

    result = read_game_settings(game_settings_path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    result = write_game_settings(game_settings_path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    result = read_game_settings(game_settings_path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    return 0;
}

static void print_game_save_state(const game_save_state_t *gs)
{
    uint8_t item;
    printf( "(game_save_state_t){\n" );
    printf( "  .initialized = %s,\n", gs->initialized ? "true" : "false" );
    printf( "  .name = \"%.16s\",\n", gs->name );
    printf( "  .map_index = %u,\n", gs->map_index );
    printf( "  .map_pos_x = %d,\n", gs->map_pos_x );
    printf( "  .map_pos_y = %d,\n", gs->map_pos_y );
    printf( "  .max_hp = %u,\n", gs->max_hp );
    printf( "  .current_hp = %u,\n", gs->current_hp );
    printf( "  .inventory = {\n" );
    for (size_t i = 0; i < 256; ++i)
    {
        item = gs->inventory[i];
        if (item)
        {
            printf( "    [%u] = %u,\n", i, item );
        }
    }
    printf( "  }\n" );
    printf( "};\n" );
}

static int read_game_save_state(const char *game_save_state_path)
{
    FILE *game_save_state_fp = NULL;
    game_save_state_t game_save_state_read;
    const size_t size = sizeof(game_save_state_t);

    printf( "Opening file for reading:\n%s\n", game_save_state_path );
    game_save_state_fp = fopen(game_save_state_path, "rb");

    if ( game_save_state_fp == NULL)
    {
        printf( "Could not open file.\n" );
        return 1;
    }

    printf( "Reading %d bytes from file...\n", size );
    fread(&game_save_state_read, size, 1, game_save_state_fp);
    fclose(game_save_state_fp);

    printf( "Read struct from EEPROM:\n\n" );
    print_game_save_state(&game_save_state_read);
    printf( "\n" );
 
    return 0;
}

static int write_game_save_state(const char *game_save_state_path)
{
    FILE *game_save_state_fp = NULL;
    const game_save_state_t game_save_state_write = { 
        1,
        "Dragon",
        500,
        1,
        87,
        -120,
        40,
        36,
        { 1, 0, 0, 52 }
    };
    const size_t size = sizeof(game_save_state_t);

    printf( "Opening file for writing:\n%s\n", game_save_state_path );
    game_save_state_fp = fopen(game_save_state_path, "wb");

    if ( game_save_state_fp == NULL)
    {
        printf( "Could not open file.\n" );
        return 1;
    }

    printf( "Writing %d bytes to file...\n", size );
    fwrite(&game_save_state_write, size, 1, game_save_state_fp);
    fclose(game_save_state_fp);

    printf( "Wrote struct to EEPROM:\n\n" );
    print_game_save_state(&game_save_state_write);
    printf( "\n" );

    return 0;
}

static int validate_game_save_state(const char *game_save_state_path)
{
    int result;

    result = read_game_save_state(game_save_state_path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    result = write_game_save_state(game_save_state_path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    result = read_game_save_state(game_save_state_path);
    if ( result != 0 ) return result;
    press_a_to_continue();

    return 0;
}

static int validate_eeprom_4k()
{
    int result;
    const eepfs_entry_t eeprom_4k_files[] = {
        { "/high_scores.dat", sizeof(game_high_scores)  },
        { "/settings.dat",    sizeof(game_settings_t)   },
        { "/player.sav",      sizeof(game_save_state_t) }
    };
    const eepfs_config_t eeprom_4k_config = {
        .files = eeprom_4k_files,
        .num_files = EEPFS_NUM_FILES(eeprom_4k_files),
        .stdio_prefix = "eeprom:/",
        .skip_signature_check = false,
    };

    printf( "EEPROM Detected: 4Kb (64 blocks)\n" );
    printf( "Initializing EEPROM Filesystem...\n" );
    result = eepfs_init(eeprom_4k_config);
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

    keep_or_erase_data();

    result = validate_game_high_scores("eeprom://high_scores.dat");
    if ( result != 0 ) return result;

    result = validate_game_settings("eeprom://settings.dat");
    if (result != 0) return result;

    result = validate_game_save_state("eeprom://player.sav");
    if (result != 0) return result;

    eepfs_deinit();

    return 0;
}

static int validate_eeprom_16k()
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
    eepfs_config_t eeprom_16k_config = {
        .files = eeprom_16k_files,
        .num_files = EEPFS_NUM_FILES(eeprom_16k_files),
        .stdio_prefix = "eeprom:/",
        .skip_signature_check = false
    };

    printf( "EEPROM Detected: 16Kb (256 blocks)\n" );
    printf( "Initializing EEPROM Filesystem...\n" );
    result = eepfs_init(eeprom_16k_config);
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

    keep_or_erase_data();

    result = validate_game_high_scores("eeprom://high_scores.dat");
    if ( result != 0 ) return result;

    result = validate_game_settings("eeprom://settings.dat");
    if (result != 0) return result;

    result = validate_game_save_state("eeprom://saves/slot1.sav");
    if (result != 0) return result;

    result = validate_game_save_state("eeprom://saves/slot2.sav");
    if (result != 0) return result;

    result = validate_game_save_state("eeprom://saves/slot3.sav");
    if (result != 0) return result;

    result = validate_game_save_state("eeprom://saves/slot4.sav");
    if (result != 0) return result;

    eepfs_deinit();

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
    /* enable interrupts (on the CPU) */
    init_interrupts();

    /* Initialize peripherals */
    display_init( res, bit, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );
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
