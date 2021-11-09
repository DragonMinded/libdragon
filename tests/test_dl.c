
#include <string.h>

const unsigned long dl_timeout = 100;

#define DL_LOG_STATUS(step) debugf("STATUS: %#010lx, PC: %#010lx (%s)\n", *SP_STATUS, *SP_PC, step)

static volatile int sp_intr_raised;

void sp_interrupt_handler()
{
    sp_intr_raised = 1;
}

void wait_for_sp_interrupt_and_halted(unsigned long timeout)
{
    unsigned long time_start = get_ticks_ms();

    while (get_ticks_ms() - time_start < timeout) {
        // Wait until the interrupt was raised and the SP is in idle mode
        if (sp_intr_raised && (*SP_STATUS & SP_STATUS_HALTED)) {
            break;
        }
    }
}

#define TEST_DL_PROLOG() \
    sp_intr_raised = 0; \
    register_SP_handler(sp_interrupt_handler); \
    set_SP_interrupt(1); \
    dl_init(); \
    DEFER(dl_close(); set_SP_interrupt(0); unregister_SP_handler(sp_interrupt_handler));

#define TEST_DL_EPILOG() \
    wait_for_sp_interrupt_and_halted(dl_timeout); \
    ASSERT(sp_intr_raised, "Interrupt was not raised!"); \
    ASSERT_EQUAL_HEX(*SP_STATUS, SP_STATUS_HALTED | SP_STATUS_BROKE, "Unexpected SP status!"); \

void test_dl_queue_single(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    dl_start();
    dl_interrupt();

    TEST_DL_EPILOG();
}

void test_dl_queue_multiple(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    dl_start();
    dl_noop();
    dl_interrupt();

    TEST_DL_EPILOG();
}

void test_dl_queue_rapid(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    dl_start();
    dl_noop();
    dl_noop();
    dl_noop();
    dl_noop();
    dl_noop();
    dl_noop();
    dl_noop();
    dl_noop();
    dl_noop();
    dl_noop();
    dl_noop();
    dl_noop();
    dl_noop();
    dl_noop();
    dl_interrupt();

    TEST_DL_EPILOG();
}

void dl_queue_noop_block(uint32_t count)
{
    uint32_t *ptr = dl_write_begin(sizeof(uint32_t) * count);
    memset(ptr, 0, sizeof(uint32_t) * count);
    dl_write_end();
}

void test_dl_queue_big(TestContext *ctx)
{
    TEST_DL_PROLOG();

    dl_start();
    dl_queue_noop_block(345);
    dl_queue_noop_block(468);
    dl_queue_noop_block(25);
    dl_interrupt();
    dl_queue_noop_block(34);

    TEST_DL_EPILOG();
}

void test_dl_load_overlay(TestContext *ctx)
{

    TEST_DL_PROLOG();
    
    gfx_init();
    DEFER(gfx_close());

    dl_start();
    rdp_set_env_color(0);
    dl_interrupt();

    TEST_DL_EPILOG();
    
    extern uint8_t rsp_ovl_gfx_text_start[];
    extern uint8_t rsp_ovl_gfx_text_end[0];

    uint32_t size = rsp_ovl_gfx_text_end - rsp_ovl_gfx_text_start;

    ASSERT_EQUAL_MEM((uint8_t*)SP_IMEM, rsp_ovl_gfx_text_start, size, "gfx overlay was not loaded into IMEM!");
}
