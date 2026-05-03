/**
 * \file sdfsdemo.c
 * \author Aftersol
 * \date 2026
 * \brief An example project that demonstrates how to
 *        set up saving and reading a file from an SD card with libdragon.
 * 
 * Requires a Real N64 Game Console. Don't run this on emulators, as they
 * don't support SD cards
 * 
 * Press A or B to write or read random numbers to the SD card. Hold Start
 * and press A or B to write or read example text file. Press Z to take a
 * screenshot (RGBA5551 .raw file)
 * 
 */

/* Set this to 0 for testing the GUI on emulators */
#define ENABLE_SD_CARD_EMULATOR_CHECK 1

#include <libdragon.h>

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <string.h>

/* Enum for holding type of files read, not written */
typedef enum file_read_t {
    FP_NUL_FILE,
    FP_BIN_FILE,
    FP_TXT_FILE
} file_read_t;

/* Threshold in milliseconds for switching to next index */
const float threshold_ms = 250.0f;

int numScreenshots;
const char default_screenshot_name[] = "sd:/IMG_%04d.raw";
#define MAX_SCREENSHOTS 10000

/**
 * @brief Initalizes the screenshot subsystem for this program
 * @return Whether the screenshot system initalization was successful
 */
bool screenshot_init() {
    bool successful = false;

    /* Number of screenshots ever taken */
    numScreenshots = 0;

    /* Check if SD card exists */
    if (debug_init_sdfs("sd:/", -1)) {

        successful = true;

        /* Count number of screenshots */
        while (numScreenshots < MAX_SCREENSHOTS) {
            char path[64] = {0};

            sprintf(
                path,
                default_screenshot_name,
                numScreenshots
            );

            FILE *file = fopen(path, "rb");

            /* Record the amount of screenshots found in root of the SD card */
            if (file == NULL) {
                break;
            }

            fclose(file);

            numScreenshots++;
        }
    }

    debug_close_sdfs();
    
    return successful;
}

/**
 * @brief Initalizes the screenshot subsystem for this program
 * @param surf Screenshot to be saved
 * @return Whether the screenshot was successfully saved
 */
bool screenshot_save(surface_t *surf) {
    uint16_t* framebuffer;
    
    if (surf == NULL) return false;

    /* Get the framebuffer data from surface */
    framebuffer = (uint16_t*)surf->buffer;

    /* Keep checking for empty spots until we find one */
    while (numScreenshots < MAX_SCREENSHOTS) {
        char path[64] = {0};

        /* Prepare invidualized numeric names for unique screenshots */
        sprintf(
            path,
            default_screenshot_name,
            numScreenshots
        );

        FILE *file = fopen(path, "rb");
        /* Empty spot found */
        if (file == NULL) {
            FILE* wfp = fopen(path, "wb");

            if (wfp == NULL) {
                return false;
            }
            
            /* Write the raw pixel data to the file */
            fwrite(
                framebuffer,
                sizeof(uint16_t),
                320 * 240,
                wfp
            );
            
            fclose(wfp);

            numScreenshots++;

            return true;
            
        }

        fclose(file);

        numScreenshots++;
    }

    /* Too many screenshots found, save SD space and time by returning false */
    return false;
}

/**
 * @brief The entry point for this ROM
 * @return Nothing, because it will never exit
 */
int main(void) {

    /* To hold text data */
    char text_buffer[1024];

    /* To hold binary data read from the SD card */
    uint32_t bin_buffer[128];

    /* To hold binary data to save to the SD card */
    uint32_t sav_bin[128];

    /* To hold the font */
    rdpq_font_t *font;

    /* Start and End timer */
    uint64_t start_ticks = 0, end_ticks = 0;

    /* For binary file read from SD card */
    uint32_t file_size = 0;
    uint32_t file_index = 0;

    /* To hold the random seed */
    uint32_t seed;

    float accumulator = 0.0f;

    /* Flags for SD card reading */
    file_read_t file_read = FP_NUL_FILE;

    /* Init logging */
    debug_init_emulog();
    debug_init_usblog();

    #if ENABLE_SD_CARD_EMULATOR_CHECK
    /* Don't run this on emulators, as they don't support SD cards. */
    assertf(
        debug_init_sdfs("sd:/", -1),
        "Failed to initialize SD card. Run this"
        " program on a real N64 with a flashcart."
        " Don't run this program on emulators"
        " as they don't support SD cards."
    );
    debug_close_sdfs();
    #endif

    /* Init display and peripherals */
    display_init(
        RESOLUTION_320x240,
        DEPTH_16_BPP,
        3,
        GAMMA_NONE,
        FILTERS_DISABLED
    );

    /* Initialize the controllers */
    joypad_init();

    /* Initialize the RDP for rendering */
    rdpq_init();

    /* Initialize the timer */

    /* Initialize the random number generator, then call rand() every
     * frame so to get random behavior also in emulators.
     */
    getentropy(&seed, sizeof(seed));
    srand(seed);
    register_VI_handler((void(*)(void))rand);

    /* Loads font and register it to slot 1 */
    font = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO);
    rdpq_text_register_font(1, font);

    screenshot_init();

    /* Main loop */
    while (1) {
        
        /* Pressed buttons */

        joypad_buttons_t button_port_1;
        joypad_buttons_t button_port_2;
        joypad_buttons_t button_port_3;
        joypad_buttons_t button_port_4;

        /* Held buttons */

        joypad_buttons_t button_port_1_held;
        joypad_buttons_t button_port_2_held;
        joypad_buttons_t button_port_3_held;
        joypad_buttons_t button_port_4_held;

        /* Framebuffer */
        surface_t *disp;

        /* Loop counter for storing array of random numbers */
        uint8_t i;

        /* When any buttons press the screenshot button, it is set to true */
        bool screenshot_flag = false;

        /* Measuring frame time */
        end_ticks = timer_ticks();
        if (file_read == FP_BIN_FILE) {
            accumulator += TIMER_MICROS_LL(end_ticks - start_ticks) / 1000.0f;

            /*
             * Prevent huge lag spikes when acculumated frame time gets too big
             */
            if (accumulator >= 1.0f) {
                accumulator = 1.0f;
            }

            /* 
             * Makes it so it works regardless of lagginess which
             *might never happen
             */
            if (accumulator >= threshold_ms) {
                while (threshold_ms < accumulator) {
                    file_index = (file_index + 1) % \
                        (file_size / sizeof(uint32_t));
                    accumulator -= threshold_ms;
                }
            }
        }

        /* Begin measuring new frame time */
        start_ticks = end_ticks;

        /* 
         * Insert number and index of the binary file into text buffer
         * for display
         */
        if (file_read == FP_BIN_FILE) {
            uint8_t scratch[4];
            uint32_t num;

            /* Workaround for strict alignment error */
            scratch[0] = (sav_bin[file_index] >> 24) & 0xFF;
            scratch[1] = (sav_bin[file_index] >> 16) & 0xFF;
            scratch[2] = (sav_bin[file_index] >> 8) & 0xFF;
            scratch[3] = sav_bin[file_index] & 0xFF;

            num = (scratch[0] << 24) |
            (scratch[1] << 16) |
            (scratch[2] << 8) |
            scratch[3];

            sprintf(text_buffer,
                "sav.bin[%lu] = %lu\n",
                file_index,
                num
            );
        }

        /* Wait for a framebuffer to become available */
        while(!(disp = display_try_get())) {;}

        /* Attach the RDP to the framebuffer */
        rdpq_attach(disp, NULL);

        /* Clear the framebuffer with black */
        rdpq_set_mode_fill(RGBA32(0, 0, 0, 255));
        rdpq_fill_rectangle(0, 0, 320, 240);

        /* Set the RDP to standard mode for rendering text */
        rdpq_set_mode_standard();

        rdpq_text_printf(
            &(rdpq_textparms_t) {
                .width = 320-32,
                .height = 240-32,
                .align = ALIGN_LEFT,
                .wrap = WRAP_WORD,
            }, 
            1, 
            16, 
            16, 
            "Requires a Real N64 Game Console & a flashcart\n"
            "Don\'t run this program on emulators\n"
            "Press A or B to write or read random numbers to the SD card\n"
            "Hold Start and press A or B to write or read example text file\n"
            "Press Z to take a RGBA5551 screenshot\n"
            "Current text file content: %s", 
            text_buffer
        );
        
        /* Poll the controllers to get the latest button states
         *
         * It does not matter which controller you use
         * to save stuff to the SD card
         * 
         */
        joypad_poll();

        button_port_1 = joypad_get_buttons_pressed(JOYPAD_PORT_1);
        button_port_2 = joypad_get_buttons_pressed(JOYPAD_PORT_2);
        button_port_3 = joypad_get_buttons_pressed(JOYPAD_PORT_3);
        button_port_4 = joypad_get_buttons_pressed(JOYPAD_PORT_4);

        button_port_1_held = joypad_get_buttons_held(JOYPAD_PORT_1);
        button_port_2_held = joypad_get_buttons_held(JOYPAD_PORT_2);
        button_port_3_held = joypad_get_buttons_held(JOYPAD_PORT_3);
        button_port_4_held = joypad_get_buttons_held(JOYPAD_PORT_4);

        /* 
         * If screenshot button from any port is pressed, take screenshot later
         */
        screenshot_flag = 
            button_port_1.z ||
            button_port_2.z ||
            button_port_3.z ||
            button_port_4.z;

        /* Store random numbers to save buffer */
        for (i = 0; i < 128; i++) {
            bin_buffer[i] = rand();
        }

        /*
         * Write example text file to SD card (START+A)
         */
        if (
            (button_port_1_held.start && button_port_1.a) ||
            (button_port_2_held.start && button_port_2.a) ||
            (button_port_3_held.start && button_port_3.a) ||
            (button_port_4_held.start && button_port_4.a)
        ) {
            /* What if SD card was unmounted while the program is running? */
            bool sd_mounted = debug_init_sdfs("sd:/", -1);

            /* Save the text file to the SD card */
            if (sd_mounted) {
                FILE* txt_file = fopen("sd:/sav.txt", "w");

                if (txt_file) {
                    char txt[512];

                    file_read = FP_TXT_FILE;

                    sys_hw_memset(txt, 0, sizeof(txt));

                    /* 
                     * Random number to prove that we can write a new file
                     * and have the program read that file when they press
                     * read text button combination
                     */
                    sprintf(txt,
                        "Hello, N64brew community!\n"
                        "Example text file.\n"
                        "The quick brown fox jumps over the lazy dog.\n"
                        "Random number: %u", rand()
                    );

                    fwrite(txt, sizeof(char), 512, txt_file);

                    sprintf(text_buffer,
                        "Wrote sav.txt to SD card\n%s",
                        txt
                    );

                    fclose(txt_file);
                } else {
                    file_read = FP_NUL_FILE;
                    sys_hw_memset(text_buffer, 0, sizeof(text_buffer));
                    sprintf(
                        text_buffer,
                        "Failed to open sav.txt for writing."
                    );
                }
            }
            else {
                file_read = FP_NUL_FILE;
                sys_hw_memset(text_buffer, 0, sizeof(text_buffer));
                sprintf(
                    text_buffer, 
                    "Failed to mount SD card "
                    "for writing text file."
                );
            }

            debug_close_sdfs();
            
        } else if ( 
            /*
            * Write read text file from SD card (START+B)
            */
            (button_port_1_held.start && button_port_1.b) ||
            (button_port_2_held.start && button_port_2.b) ||
            (button_port_3_held.start && button_port_3.b) ||
            (button_port_4_held.start && button_port_4.b)
        ) {
            /* 
             * What if SD card was unmounted while the program is running?
             */
            bool sd_mounted = debug_init_sdfs("sd:/", -1);
            
            /* Read the text file from the SD card */
            if (sd_mounted) {
                FILE* txt_file = fopen("sd:/sav.txt", "r");
                if (txt_file) {
                    file_read = FP_TXT_FILE;

                    fread(text_buffer, sizeof(char), 511, txt_file);

                    text_buffer[511] = '\0'; /* Ensure null termination */

                    fclose(txt_file);
                } else {
                    file_read = FP_NUL_FILE;

                    sys_hw_memset(text_buffer, 0, sizeof(text_buffer));

                    sprintf(
                        text_buffer,
                        "Failed to open sav.txt for reading."
                    );
                }
            }
            else {
                file_read = FP_NUL_FILE;

                sys_hw_memset(text_buffer, 0, sizeof(text_buffer));

                sprintf(
                    text_buffer, 
                    "Failed to mount SD card for reading text file."
                );
            }


            debug_close_sdfs();
        }
        else {
            /* If A is pressed, write random numbers to the SD card */
            if (
                button_port_1.a ||
                button_port_2.a ||
                button_port_3.a ||
                button_port_4.a
            ) {
                /* 
                 * What if SD card was unmounted while the program is running?
                 */
                bool sd_mounted = debug_init_sdfs("sd:/", -1);

                /* Save random numbers to the SD card */
                if (sd_mounted) {
                    FILE* bin_file = fopen("sd:/sav.bin", "wb");

                    if (bin_file) {
                        uint8_t scratch[4];
                        uint32_t num;

                        /* To let user know writing sav.bin is successful */
                        file_read = FP_TXT_FILE;

                        /* Workaround for strict alignment error */
                        scratch[0] = (bin_buffer[0] >> 24) & 0xFF;
                        scratch[1] = (bin_buffer[0] >> 16) & 0xFF;
                        scratch[2] = (bin_buffer[0] >> 8) & 0xFF;
                        scratch[3] = bin_buffer[0] & 0xFF;

                        num = (scratch[0] << 24) |
                        (scratch[1] << 16) |
                        (scratch[2] << 8) |
                        scratch[3];

                        /* Write random numbers to SD card */
                        fwrite(bin_buffer, sizeof(uint32_t), 128, bin_file);

                        sys_hw_memset(bin_buffer, 0, sizeof(bin_buffer));

                        sys_hw_memset(text_buffer, 0, sizeof(text_buffer));

                        sprintf(
                            text_buffer, 
                            "Wrote random numbers to SD card.\n"
                            "First number: %lu",
                            num
                        );
                        
                        fclose(bin_file);
                    } else {
                        file_read = FP_NUL_FILE;

                        sys_hw_memset(text_buffer, 0, sizeof(text_buffer));

                        sprintf(
                            text_buffer,
                            "Failed to open sav.bin for writing."
                        );
                    }
                } else {
                    file_read = FP_NUL_FILE;

                    sys_hw_memset(text_buffer, 0, sizeof(text_buffer));

                    sprintf(
                        text_buffer,
                        "Failed to mount SD card for writing binary file."
                    );
                }

                debug_close_sdfs();
            }

            /* 
             * If B is pressed, read the numbers back from the SD card and
             * store them in sav_bin buffer 
             */
            if (
                button_port_1.b ||
                button_port_2.b ||
                button_port_3.b ||
                button_port_4.b
            ) {
                /* 
                 * What if SD card was unmounted while the program is running?
                 */
                bool sd_mounted = debug_init_sdfs("sd:/", -1);

                /* Read the random numbers from the SD card */
                if (sd_mounted) {
                    FILE* bin_file = fopen("sd:/sav.bin", "rb");

                    if (bin_file) {
                        /* 
                         *Reset indexing because we don't know size of buffer
                         */
                        file_read = FP_BIN_FILE;
                        file_index = 0;

                        accumulator = 0.0f;

                        sys_hw_memset(sav_bin, 0, sizeof(sav_bin));

                        file_size = fread(
                            sav_bin,
                            sizeof(uint32_t),
                            128,
                            bin_file
                        );

                        fclose(bin_file);

                    } else {
                        file_read = FP_NUL_FILE;
                        sys_hw_memset(text_buffer, 0, sizeof(text_buffer));
                        sprintf(
                            text_buffer,
                            "Failed to open sav.bin for reading."
                        );
                    }
                } else {
                    file_read = FP_NUL_FILE;
                    sys_hw_memset(text_buffer, 0, sizeof(text_buffer));
                    sprintf(
                        text_buffer,
                        "Failed to mount SD card for reading binary file."
                    );
                }

                debug_close_sdfs();
            }
        }

        /*
         * Save a screenshot when the screenshot button on any port was pressed
         * raising a screenshot flag
         */
        if (screenshot_flag) {
            /* 
             * What if SD card was unmounted while the program is running?
             */
            bool sd_mounted = debug_init_sdfs("sd:/", -1);

            /* Save RGBA5551 SD card */
            if (sd_mounted) {
                surface_t scr_surf = surface_alloc(FMT_RGBA16, 320, 240);

                /* Blit current framebuffer to surface */
                rdpq_attach(&scr_surf, NULL);
                rdpq_set_mode_copy(false);
                rdpq_tex_blit(disp, 0, 0, NULL);
                rdpq_detach();

                /* Try to write raw screenshot to SD card */
                if (!screenshot_save(&scr_surf)) {
                    file_read = FP_NUL_FILE;

                    sys_hw_memset(text_buffer, 0, sizeof(text_buffer));

                    sprintf(
                        text_buffer,
                        "Failed to save screenshot to SD card."
                    );
                }
                surface_free(&scr_surf);

            } else {
                file_read = FP_NUL_FILE;

                sys_hw_memset(text_buffer, 0, sizeof(text_buffer));

                sprintf(
                    text_buffer,
                    "Failed to mount SD card for reading taking screenshot."
                );
            }

            debug_close_sdfs();
        }
        rdpq_detach_show();

    }
    
    return 0;
}
