#include <malloc.h>
#include <string.h>

#include <ugfx.h>

#include "../src/dl/dl_internal.h"

static uint32_t test_ovl_data[2];

void test_ovl_init()
{
    uint8_t ovl_index = DL_OVERLAY_ADD(rsp_test, test_ovl_data);
    dl_overlay_register_id(ovl_index, 0xF);
}

void dl_test_4()
{
    uint32_t *ptr = dl_write_begin(4);
    ptr[0] = 0xf0000000;
    dl_write_end();
}

void dl_test_8()
{
    uint32_t *ptr = dl_write_begin(8);
    ptr[0] = 0xf1000000;
    ptr[1] = 0x02000200;
    dl_write_end();
}

void dl_test_16()
{
    uint32_t *ptr = dl_write_begin(16);
    ptr[0] = 0xf2000000;
    ptr[1] = 0x02000800;
    ptr[2] = 0x02002000;
    ptr[3] = 0x02008000;
    dl_write_end();
}

void dl_test_wait(uint32_t length)
{
    uint32_t *ptr = dl_write_begin(8);
    ptr[0] = 0xf3000000;
    ptr[1] = length;
    dl_write_end();
}

#define DL_LOG_STATUS(step) debugf("STATUS: %#010lx, PC: %#010lx (%s)\n", *SP_STATUS, *SP_PC, step)

void dump_mem(void* ptr, uint32_t size)
{
    for (uint32_t i = 0; i < size / sizeof(uint32_t); i += 8)
    {
        uint32_t *ints = ptr + i * sizeof(uint32_t);
        debugf("%#010lX: %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX\n", (uint32_t)ints, ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], ints[6], ints[7]);
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

const unsigned long dl_timeout = 100;

#define TEST_DL_EPILOG(s, t) \
    wait_for_sp_interrupt_and_halted(t); \
    ASSERT(sp_intr_raised, "Interrupt was not raised!"); \
    ASSERT_EQUAL_HEX(*SP_STATUS, SP_STATUS_HALTED | SP_STATUS_BROKE | (s), "Unexpected SP status!");

void test_dl_queue_single(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    dl_start();
    dl_interrupt();

    TEST_DL_EPILOG(0, dl_timeout);
}

void test_dl_queue_multiple(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    dl_start();
    dl_noop();
    dl_interrupt();

    TEST_DL_EPILOG(0, dl_timeout);
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

    TEST_DL_EPILOG(0, dl_timeout);
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

    TEST_DL_EPILOG(0, dl_timeout);
}

void test_dl_signal(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    dl_start();
    dl_signal(SP_WSTATUS_SET_SIG3 | SP_WSTATUS_SET_SIG6);
    dl_interrupt();

    TEST_DL_EPILOG(SP_STATUS_SIG3 | SP_STATUS_SIG6, dl_timeout);
}

void test_dl_high_load(TestContext *ctx)
{
    TEST_DL_PROLOG();

    test_ovl_init();

    dl_start();

    for (uint32_t i = 0; i < 0x800; i++)
    {
        uint32_t x = RANDN(3);

        switch (x)
        {
            case 0:
                dl_test_4();
                break;
            case 1:
                // Simulate computation heavy commands that take a long time to complete, so the ring buffer fills up
                dl_test_wait(0x10000);
                break;
            case 2:
                dl_test_16();
                break;
        }
    }

    dl_interrupt();

    TEST_DL_EPILOG(0, 5000);
}

void test_dl_load_overlay(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    ugfx_init();
    DEFER(ugfx_close());

    dl_start();
    rdp_set_env_color(0);
    dl_interrupt();

    TEST_DL_EPILOG(0, dl_timeout);
    
    extern uint8_t rsp_ugfx_text_start[];
    extern uint8_t rsp_ugfx_text_end[0];

    uint32_t size = rsp_ugfx_text_end - rsp_ugfx_text_start;

    ASSERT_EQUAL_MEM((uint8_t*)SP_IMEM, rsp_ugfx_text_start, size, "ugfx overlay was not loaded into IMEM!");
}
