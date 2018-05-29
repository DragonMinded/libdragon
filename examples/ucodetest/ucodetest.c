#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>

#include <libdragon.h>
#include <rsp.h>
#include <exception.h>

extern const char basic_ucode_start;
extern const char basic_ucode_end;
extern const char basic_ucode_size;

static resolution_t res = RESOLUTION_320x240;
static bitdepth_t bit = DEPTH_32_BPP;

static volatile bool broke = false;

static void sp_handler(exception_t* e) {
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

    // unsigned long size = (unsigned long)&basic_ucode_size;
    unsigned long size = 8;
    load_ucode((void*)&basic_ucode_start, size);

    char* up = malloc(size);
    read_ucode((void*)up, size);

    console_clear();

    const char* orig = &basic_ucode_start;

    unsigned long i = 0;
    while(i < size)
    {
        printf("%02X ", orig[i]);
        if (i % 8 == 7) {
            printf("\n");
        }
        i++;
    }

    printf("\n");

    i = 0;
    while(i < size)
    {
        printf("%02X ", up[i]);
        if (i % 8 == 7) {
            printf("\n");
        }
        i++;
    }

    printf("\nsize: %d\n", (int)size);

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
