#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>

#include <libdragon.h>
#include <rsp.h>

DEFINE_RSP_UCODE(rsp_basic);

static volatile bool broke = false;

static void sp_handler() {
    broke = true;
}

int main(void)
{
    /* Initialize peripherals */
    console_init();
    console_set_render_mode(RENDER_MANUAL);
    rsp_init();

    /* Attach SP handler and enable interrupt */
    register_SP_handler(&sp_handler);
    set_SP_interrupt(1);

    rsp_load(&rsp_basic);

    unsigned char* orig = malloc(16);
    rsp_read_data(orig, 16, 0);

    unsigned long i = 0;
    while(i < 16)
    {
        printf("%02X ", orig[i]);
        if (i % 8 == 7) {
            printf("\n");
        }
        i++;
    }

    printf("\n");
    console_render();

    rsp_run_async();

    RSP_WAIT_LOOP(2000) {
        if (broke) {
            break;
        }
    }

    printf("\nbroke");
    printf("\n");

    unsigned char* up = malloc(16);
    rsp_read_data((void*)up, 16, 0);

    i = 0;
    while(i < 16)
    {
        printf("%02X ", up[i]);
        if (i % 8 == 7) {
            printf("\n");
        }
        i++;
    }
    console_render();
}
