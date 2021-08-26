#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>

#include <libdragon.h>
#include <rsp.h>

extern uint8_t rsp_basic_data_start[];
extern uint8_t rsp_basic_data_end[];
extern uint8_t rsp_basic_text_start[];
extern uint8_t rsp_basic_text_end[];

static volatile bool broke = false;

static void sp_handler() {
    broke = true;
}

int main(void)
{
    /* enable interrupts (on the CPU) */
    init_interrupts();

    /* Initialize peripherals */
    console_init();
    console_set_render_mode(RENDER_MANUAL);
    rsp_init();

    /* Attach SP handler and enable interrupt */
    register_SP_handler(&sp_handler);
    set_SP_interrupt(1);

    unsigned long data_size = rsp_basic_data_end - rsp_basic_data_start;
    unsigned long ucode_size = rsp_basic_text_end - rsp_basic_text_start;

    /* start & end must be aligned to 8 bytes.
     * This property has been guaranteed by the build system */
    assert(((uint32_t)rsp_basic_text_start % 8) == 0);
    assert(((uint32_t)rsp_basic_data_start % 8) == 0);
    
    load_ucode(rsp_basic_text_start, ucode_size);
    load_data(rsp_basic_data_start, data_size);

    const uint8_t* orig = rsp_basic_data_start;

    unsigned long i = 0;
    while(i < data_size)
    {
        printf("%02X ", orig[i]);
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
            printf("\nbroke");
            printf("\n");
            broke = false;

            unsigned char* up = malloc(data_size);
            read_data((void*)up, data_size);

            i = 0;
            while(i < data_size)
            {
                printf("%02X ", up[i]);
                if (i % 8 == 7) {
                    printf("\n");
                }
                i++;
            }
        }
        console_render();
    }
}
