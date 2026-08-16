/**
 * @file menudemo.c
 * @author Aftersol
 * @date 2026-05-20
 * @brief A simple menu example for libdragon.
 * 
 * Credits:
 * - bap.wav - Aftersol
 * - bop.wav - Aftersol
 * - Spooky Илюха and Cedar Branch: Libdragon logo
 *   https://github.com/DragonMinded/libdragon/wiki/Logos
 * - MiaFan2010: You Would Be Here (miafan2010_-_you_would_be_here.xm)
 *   https://modarchive.org/index.php?request=view_by_moduleid&query=172936
 * - madameberry: Public Domain Backgrounds - sunset.png
 *   https://opengameart.org/content/public-domain-backgrounds
 */

#include <libdragon.h>

#include <stdio.h>

bool play_sfx = true;
bool play_music = true;

char main_menu_items [3][256]  = {
    "Start Game",
    "Options",
    "Credits"
};

char options_menu_items [3][256] = {
    "Music - ON",
    "SFX - ON",
    "Back"
};

char credits_menu_items [3][256] = {
    "Programming and SFX by Aftersol",
    "madameberry - sunset.png, MiaFan2010 - you_would_be_here.xm",
    "Back"
};

inline void wav64_play_if_enabled(bool enabled, wav64_t *wav, int ch) {
    if (enabled)
        wav64_play(wav, ch);
}

int main() {
    int menuIndex = 0, menuID = 0;
    xm64player_t music; // Background Music
    wav64_t bop, bap;

    // For displaying different menu options; default is main menu
    char *menuText[3] = {
        main_menu_items[0],
        main_menu_items[1],
        main_menu_items[2]
    };

    // Start up debug subsystem
    debug_init_emulog();
    debug_init_usblog();

    asset_init_compression(3); // For compresseed XM file

    dfs_init(DFS_DEFAULT_LOCATION); // Start up ROM DFS for loading assets from ROM
    display_init(
        RESOLUTION_320x240,
        DEPTH_16_BPP,
        2,
        GAMMA_NONE,
        FILTERS_DISABLED
    );

    joypad_init();
    rdpq_init();

    // Start up audio subsystem
    audio_init(22050, 3);
    mixer_init(32);

    wav64_open(&bop, "rom:/bop.wav64"); // Load audio assets
    wav64_open(&bap, "rom:/bap.wav64");
    xm64player_open(&music, "rom:/miafan2010_-_you_would_be_here.xm64");
    xm64player_set_loop(&music, true);
    xm64player_play(&music, 0);

    sprite_t* background = sprite_load("rom:/background.sprite"); // Load sprites
    sprite_t* logo = sprite_load("rom:/logo.ci4.sprite");

    rdpq_font_t *font = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO);
    rdpq_text_register_font(1, font); // Load and register default font

    while (1) {
        surface_t* disp;

        char menuTextBuffer[3][256]; // For text displayed to screen

        while(!(disp = display_try_get())) {;}

        joypad_poll(); // Poll controls for button inputs
        mixer_try_play(); // Required for audio playback

        joypad_buttons_t button_port_1 = joypad_get_buttons_pressed(JOYPAD_PORT_1);

        if (button_port_1.d_up || button_port_1.c_up || joypad_get_axis_pressed(JOYPAD_PORT_1, JOYPAD_AXIS_STICK_Y) > 0) {
            wav64_play_if_enabled(play_sfx, &bap, 31); // play clack sound
            menuIndex = (menuIndex - 1 + 3) % 3; // Move up in the menu
        }

        if (button_port_1.d_down || button_port_1.c_down || joypad_get_axis_pressed(JOYPAD_PORT_1, JOYPAD_AXIS_STICK_Y) < 0) {
            wav64_play_if_enabled(play_sfx, &bap, 31); // play clack sound
            menuIndex = (menuIndex + 1) % 3; // Move down in the menu
        }

        if (button_port_1.b && menuID != 0) {
            wav64_play_if_enabled(play_sfx, &bop, 31); // play return sound
            // Back selected
            menuID = 0; // Return to main menu
            menuIndex = 0; // Reset menu index for main menu
            for (int i = 0; i < 3; i++)
                menuText[i] = main_menu_items[i];
        }

        else if (button_port_1.a) {
            switch (menuID) {
                case 0: // Main Menu
                {
                    if (menuIndex == 1) {
                        // Options selected
                        menuID = 1; // Switch to options menu
                        menuIndex = 0; // Reset menu index for options
                        for (int i = 0; i < 3; i++)
                            menuText[i] = options_menu_items[i];
                    }
                    if (menuIndex == 2) {
                        // Credits selected
                        menuID = 2; // Switch to credits menu
                        menuIndex = 0; // Reset menu index for credits
                        for (int i = 0; i < 3; i++)
                            menuText[i] = credits_menu_items[i];
                    }
                    break;
                }
                case 1: // Options Menu
                    {
                        if (menuIndex == 0) { // Toggle Music
                            sys_hw_memset(menuText[0], 0, 256);
                            play_music ^= 1; // Use XOR to toggle music
                            sprintf(options_menu_items[0], (play_music) ? "Music - ON" : "Music - OFF");
                            xm64player_set_vol(&music, (play_music) ? 1.0f : 0.0f);
                        }
                        if (menuIndex == 1) { // Toggle SFX
                            sys_hw_memset(options_menu_items[1], 0, 256);
                            play_sfx ^= 1; // Use XOR to toggle sound
                            sprintf(options_menu_items[1], (play_sfx) ? "SFX - ON" : "SFX - OFF");
                        } else if (menuIndex == 2) {
                            // Back selected
                            menuID = 0; // Return to main menu
                            menuIndex = 0; // Reset menu index for main menu
                            for (int i = 0; i < 3; i++)
                                menuText[i] = main_menu_items[i];
                        }
                        break;
                }
                case 2:
                {
                    if (menuIndex == 2) { // Back selected
                        menuID = 0; // Return to main menu
                        menuIndex = 0; // Reset menu index for main menu
                        for (int i = 0; i < 3; i++)
                            menuText[i] = main_menu_items[i];
                    }
                }
            }
            wav64_play_if_enabled(play_sfx, &bop, 31); // play confirm sound
        }

        rdpq_attach(disp, NULL);
        rdpq_set_mode_copy(true);
        rdpq_sprite_blit(background, 0, 0, NULL); // Draw 2D elements to screen
        rdpq_sprite_blit(logo, (320/2)-(114/2), 32, NULL);

        rdpq_set_mode_standard();
        for (int i = 0; i < 3; i++) // Write indicator to text buffer
            sprintf(menuTextBuffer[i], (i == menuIndex) ? "> %s <" : "  %s  ", menuText[i]);
        
        rdpq_text_printf(&(rdpq_textparms_t) { // Draw menu options to screen
            .width = 320-32,
            .align = ALIGN_CENTER,
            .wrap = WRAP_WORD,
            }, 1, 16, 128, "%s\n%s\n%s\n", // %s is replaced with menu options automatically
            menuTextBuffer[0], 
            menuTextBuffer[1], 
            menuTextBuffer[2]);

        for (int i = 0; i < 3; i++) // Clear text buffer for next frame
            sys_hw_memset(menuTextBuffer[i], 0, 64);

        rdpq_detach_show(); // Send the contents of the frame to the screen

    }
}
