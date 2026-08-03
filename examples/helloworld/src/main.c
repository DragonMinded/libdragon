#include <libdragon.h>

int main(void) {
    // libdragon subsystems are typically initialized on demand by calling
    // *_init functions, like here we initialize the display and rdpq systems.
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE,
                 FILTERS_RESAMPLE);
    rdpq_init();

    // Register a built-in font as id 1
    rdpq_text_register_font(1, rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_VAR));

    while (1) {
        // Acquire a free framebuffer for rendering
        surface_t *disp = display_get();
        // Attach the framebuffer for use by rdpq
        rdpq_attach(disp, NULL);

        // Clear the framebuffer with black
        rdpq_clear((color_t){0, 0, 0, 0});

        // Print some text
        rdpq_text_print(NULL, 1, 50, 50, "Hello World!");

        // Detach the framebuffer and show it on screen when it's ready
        // (when previous rendering operations have completed)
        rdpq_detach_show();
    }
}
