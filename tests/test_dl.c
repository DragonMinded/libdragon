#include <malloc.h>
#include <string.h>

#include <dl.h>
#include <ugfx.h>

#include "../src/dl/dl_internal.h"
#include "../src/ugfx/ugfx_internal.h"

DEFINE_RSP_UCODE(rsp_test);

void test_ovl_init()
{
    void *test_ovl_state = dl_overlay_get_state(&rsp_test);
    memset(test_ovl_state, 0, sizeof(uint32_t) * 2);

    uint8_t ovl_index = dl_overlay_add(&rsp_test);
    dl_overlay_register_id(ovl_index, 0xF);
}

void dl_test_4(uint32_t value)
{
    uint32_t *ptr = dl_write_begin();
    *ptr++ = 0xf0000000 | value;
    dl_write_end(ptr);
}

void dl_test_8(uint32_t value)
{
    uint32_t *ptr = dl_write_begin();
    *ptr++ = 0xf1000000 | value;
    *ptr++ = 0x02000000 | SP_WSTATUS_SET_SIG0;
    dl_write_end(ptr);
}

void dl_test_16(uint32_t value)
{
    uint32_t *ptr = dl_write_begin();
    *ptr++ = 0xf2000000 | value;
    *ptr++ = 0x02000000 | SP_WSTATUS_SET_SIG1;
    *ptr++ = 0x02000000 | SP_WSTATUS_SET_SIG2;
    *ptr++ = 0x02000000 | SP_WSTATUS_SET_SIG3;
    dl_write_end(ptr);
}

void dl_test_wait(uint32_t length)
{
    uint32_t *ptr = dl_write_begin();
    *ptr++ = 0xf3000000;
    *ptr++ = length;
    dl_write_end(ptr);
}

void dl_test_output(uint64_t *dest)
{
    uint32_t *ptr = dl_write_begin();
    *ptr++ = 0xf4000000;
    *ptr++ = (uint32_t)PhysicalAddr(dest);
    dl_write_end(ptr);
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

bool wait_for_syncpoint(int sync_id, unsigned long timeout)
{
    unsigned long time_start = get_ticks_ms();

    while (get_ticks_ms() - time_start < timeout) {
        // Wait until the interrupt was raised and the SP is in idle mode
        if (dl_check_syncpoint(sync_id) && (*SP_STATUS & SP_STATUS_HALTED)) {
            return true;
        }
    }
    return false;
}

#define TEST_DL_PROLOG() \
    dl_init(); \
    DEFER(dl_close());

const unsigned long dl_timeout = 100;

#define TEST_DL_EPILOG(s, t) ({ \
    int sync_id = dl_syncpoint(); \
    dl_flush(); \
    if (!wait_for_syncpoint(sync_id, t)) \
        ASSERT(0, "display list not completed: %d/%d", dl_check_syncpoint(sync_id), (*SP_STATUS & SP_STATUS_HALTED) != 0); \
    ASSERT_EQUAL_HEX(*SP_STATUS, SP_STATUS_HALTED | SP_STATUS_BROKE | SP_STATUS_SIG5 | (s), "Unexpected SP status!"); \
})

void test_dl_queue_single(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    dl_start();

    TEST_DL_EPILOG(0, dl_timeout);
}

void test_dl_queue_multiple(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    dl_start();
    dl_noop();

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

    TEST_DL_EPILOG(0, dl_timeout);
}

void test_dl_wrap(TestContext *ctx)
{
    TEST_DL_PROLOG();

    dl_start();

    uint32_t block_count = DL_DRAM_BUFFER_SIZE * 8;
    for (uint32_t i = 0; i < block_count; i++)
        dl_noop();
    
    TEST_DL_EPILOG(0, dl_timeout);
}

void test_dl_signal(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    dl_start();
    dl_signal(SP_WSTATUS_SET_SIG1 | SP_WSTATUS_SET_SIG3);

    TEST_DL_EPILOG(SP_STATUS_SIG1 | SP_STATUS_SIG3, dl_timeout);
}

void test_dl_high_load(TestContext *ctx)
{
    TEST_DL_PROLOG();

    test_ovl_init();

    dl_start();

    uint64_t expected_sum = 0;

    for (uint32_t i = 0; i < 0x1000; i++)
    {
        uint32_t x = RANDN(3);

        switch (x)
        {
            case 0:
                dl_test_4(1);
                ++expected_sum;
                break;
            case 1:
                // Simulate computation heavy commands that take a long time to complete, so the ring buffer fills up
                dl_test_wait(0x10000);
                break;
            case 2:
                dl_test_16(1);
                ++expected_sum;
                break;
        }
    }

    uint64_t actual_sum;
    uint64_t *actual_sum_ptr = UncachedAddr(&actual_sum);

    dl_test_output(actual_sum_ptr);

    TEST_DL_EPILOG(0, 10000);

    ASSERT_EQUAL_UNSIGNED(*actual_sum_ptr, expected_sum, "Possibly not all commands have been executed!");
}

void test_dl_load_overlay(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    ugfx_init();
    DEFER(ugfx_close());

    dl_start();
    rdp_set_env_color(0);

    TEST_DL_EPILOG(0, dl_timeout);
    
    extern uint8_t rsp_ugfx_text_start[];
    extern uint8_t rsp_ugfx_text_end[0];

    uint32_t size = rsp_ugfx_text_end - rsp_ugfx_text_start;

    ASSERT_EQUAL_MEM((uint8_t*)SP_IMEM, rsp_ugfx_text_start, size, "ugfx overlay was not loaded into IMEM!");
}

void test_dl_switch_overlay(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    test_ovl_init();

    ugfx_init();
    DEFER(ugfx_close());

    dl_start();

    rdp_set_env_color(0);
    dl_test_16(0);

    TEST_DL_EPILOG(0, dl_timeout);

    extern rsp_ucode_t rsp_ugfx;
    extern void* dl_overlay_get_state(rsp_ucode_t *overlay_ucode);

    ugfx_state_t *ugfx_state = UncachedAddr(dl_overlay_get_state(&rsp_ugfx));

    uint64_t expected_commands[] = {
        RdpSetEnvColor(0)
    };

    ASSERT_EQUAL_MEM(ugfx_state->rdp_buffer, (uint8_t*)expected_commands, sizeof(expected_commands), "State was not saved!");
}

void test_dl_sync(TestContext *ctx)
{
    TEST_DL_PROLOG();
    
    test_ovl_init();
    dl_start();

    for (uint32_t i = 0; i < 1000; i++)
    {
        dl_test_8(1);
        dl_test_wait(0x8000);
        dl_sync();
    }

    uint64_t actual_sum;
    uint64_t *actual_sum_ptr = UncachedAddr(&actual_sum);

    dl_test_output(actual_sum_ptr);

    TEST_DL_EPILOG(0, dl_timeout);

    ASSERT_EQUAL_UNSIGNED(*actual_sum_ptr, 1000, "Sum is incorrect!");
}

// TODO: test syncing with overlay switching
