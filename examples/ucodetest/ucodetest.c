#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>

#include <libdragon.h>
#include <rsp.h>

extern const void __basic_ucode_data_start;
extern const void __basic_ucode_start;
extern const void __basic_ucode_end;

static resolution_t res = RESOLUTION_320x240;
static bitdepth_t bit = DEPTH_32_BPP;

static volatile bool broke = false;

static void sp_handler() {
    broke = true;
}

int main(void)
{
    /* enable interrupts (on the CPU) */
    init_interrupts();

    /* Initialize peripherals */
    display_init( res, bit, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );
    console_init();
    console_set_render_mode(RENDER_MANUAL);
    rsp_init();

    /* Attach SP handler and enable interrupt */
    register_SP_handler(&sp_handler);
    set_SP_interrupt(1);

    // Size must be multiple of 8 and start & end must be aligned to 8 bytes
    unsigned long data_size = (unsigned long) (&__basic_ucode_start - &__basic_ucode_data_start);
    unsigned long ucode_size = (unsigned long) (&__basic_ucode_end - &__basic_ucode_start);
    load_data((void*)&__basic_ucode_data_start, data_size);
    load_ucode((void*)&__basic_ucode_start, ucode_size);

    char* up = malloc(ucode_size);
    read_ucode((void*)up, ucode_size);

    console_clear();

    const char* orig = &__basic_ucode_start;

    unsigned long i = 0;
    while(i < 10)
    {
        printf("%02X ", orig[i]);
        if (i % 8 == 7) {
            printf("\n");
        }
        i++;
    }

    printf("\n");

    i = 0;
    while(i < 10)
    {
        printf("%02X ", up[i]);
        if (i % 8 == 7) {
            printf("\n");
        }
        i++;
    }

    printf("\nsize: %d\n", (int)ucode_size);

    run_ucode();

    while(1)
    {
        if (broke) {
            printf("broke");
            broke = false;
        }
        console_render();
    }
}
