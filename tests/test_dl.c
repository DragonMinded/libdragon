
#include <string.h>

#include "../src/dl/dl_internal.h"

const unsigned long dl_timeout = 100;

#define DL_LOG_STATUS(step) debugf("STATUS: %#010lx, PC: %#010lx (%s)\n", *SP_STATUS, *SP_PC, step)

void dump_mem(void* ptr, uint32_t size)
{
    for (uint32_t i = 0; i < size / sizeof(uint32_t); i += 4)
    {
        uint32_t *ints = ptr + i * sizeof(uint32_t);
        debugf("%08lX %08lX %08lX %08lX\n", ints[0], ints[1], ints[2], ints[3]);
    }
}

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

#define TEST_DL_EPILOG(s) \
    wait_for_sp_interrupt_and_halted(dl_timeout); \
    ASSERT(sp_intr_raised, "Interrupt was not raised!"); \
    ASSERT_EQUAL_HEX(*SP_STATUS, SP_STATUS_HALTED | SP_STATUS_BROKE | (s), "Unexpected SP status!"); \

void test_dl_queue_single(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    dl_start();
    dl_interrupt();

    TEST_DL_EPILOG(0);
}

void test_dl_queue_multiple(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    dl_start();
    dl_noop();
    dl_interrupt();

    TEST_DL_EPILOG(0);
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

    TEST_DL_EPILOG(0);
}

void test_dl_wrap(TestContext *ctx)
{
    TEST_DL_PROLOG();

    dl_start();

    // 1.5 times the size of the buffer
    uint32_t block_count = (DL_DRAM_BUFFER_SIZE * 3) / (DL_MAX_COMMAND_SIZE * 2);

    for (uint32_t i = 0; i < block_count; i++)
    {
        uint32_t *ptr = dl_write_begin(DL_MAX_COMMAND_SIZE);
        memset(ptr, 0, DL_MAX_COMMAND_SIZE);
        dl_write_end();
    }
    
    dl_interrupt();

    TEST_DL_EPILOG(0);
}

void test_dl_signal(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    dl_start();
    dl_signal(SP_WSTATUS_SET_SIG3 | SP_WSTATUS_SET_SIG6);
    dl_interrupt();

    TEST_DL_EPILOG(SP_STATUS_SIG3 | SP_STATUS_SIG6);
}

void test_dl_heterogeneous_sizes(TestContext *ctx)
{
    TEST_DL_PROLOG();

    ugfx_init();
    DEFER(ugfx_close());

    dl_start();

    for (uint32_t i = 0; i < 0x400; i++)
    {
        uint32_t x = RANDN(3);
        switch (x)
        {
            case 0:
                dl_signal(SP_WSTATUS_SET_SIG1);
                break;
            case 1:
                rdp_set_prim_color(0xFFFFFFFF);
                break;
            case 2:
                rdp_texture_rectangle(0, 0, 0, 32, 32, 0, 0, 1, 1);
                break;
        }
    }

    dl_signal(SP_WSTATUS_CLEAR_SIG1);
    dl_interrupt();

    TEST_DL_EPILOG(0);
}

void test_dl_load_overlay(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    ugfx_init();
    DEFER(ugfx_close());

    dl_start();
    rdp_set_env_color(0);
    dl_interrupt();

    TEST_DL_EPILOG(0);
    
    extern uint8_t rsp_ugfx_text_start[];
    extern uint8_t rsp_ugfx_text_end[0];

    uint32_t size = rsp_ugfx_text_end - rsp_ugfx_text_start;

    ASSERT_EQUAL_MEM((uint8_t*)SP_IMEM, rsp_ugfx_text_start, size, "ugfx overlay was not loaded into IMEM!");
}
