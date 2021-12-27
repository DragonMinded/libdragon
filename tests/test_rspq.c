#include <malloc.h>
#include <string.h>

#include <rspq.h>
#include <ugfx.h>

#include "../src/rspq/rspq_internal.h"
#include "../src/ugfx/ugfx_internal.h"

DEFINE_RSP_UCODE(rsp_test);

void test_ovl_init()
{
    void *test_ovl_state = rspq_overlay_get_state(&rsp_test);
    memset(test_ovl_state, 0, sizeof(uint32_t) * 2);

    rspq_init();
    rspq_overlay_register(&rsp_test, 0xF);
    rspq_sync(); // make sure the overlay is fully registered before beginning
}

void rspq_test_4(uint32_t value)
{
    uint32_t *ptr = rspq_write_begin();
    *ptr++ = 0xf0000000 | value;
    rspq_write_end(ptr);
}

void rspq_test_8(uint32_t value)
{
    uint32_t *ptr = rspq_write_begin();
    *ptr++ = 0xf1000000 | value;
    *ptr++ = 0x02000000 | SP_WSTATUS_SET_SIG0;
    rspq_write_end(ptr);
}

void rspq_test_16(uint32_t value)
{
    uint32_t *ptr = rspq_write_begin();
    *ptr++ = 0xf2000000 | value;
    *ptr++ = 0x02000000 | SP_WSTATUS_SET_SIG0;
    *ptr++ = 0x02000000 | SP_WSTATUS_SET_SIG1;
    *ptr++ = 0x02000000 | SP_WSTATUS_SET_SIG2;
    rspq_write_end(ptr);
}

void rspq_test_wait(uint32_t length)
{
    uint32_t *ptr = rspq_write_begin();
    *ptr++ = 0xf3000000;
    *ptr++ = length;
    rspq_write_end(ptr);
}

void rspq_test_output(uint64_t *dest)
{
    uint32_t *ptr = rspq_write_begin();
    *ptr++ = 0xf4000000;
    *ptr++ = PhysicalAddr(dest);
    rspq_write_end(ptr);
}

void rspq_test_reset(void)
{
    uint32_t *ptr = rspq_write_begin();
    *ptr++ = 0xf5000000;
    rspq_write_end(ptr);
}

void rspq_test_high(uint32_t value)
{
    uint32_t *ptr = rspq_write_begin();
    *ptr++ = 0xf6000000 | value;
    rspq_write_end(ptr);
}


#define RSPQ_LOG_STATUS(step) debugf("STATUS: %#010lx, PC: %#010lx (%s)\n", *SP_STATUS, *SP_PC, step)

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
        if (rspq_check_syncpoint(sync_id) && (*SP_STATUS & SP_STATUS_HALTED)) {
            return true;
        }
    }
    return false;
}

#define TEST_RSPQ_PROLOG() \
    rspq_init(); \
    DEFER(rspq_close());

const unsigned long rspq_timeout = 100;

#define TEST_RSPQ_EPILOG(s, t) ({ \
    int sync_id = rspq_syncpoint(); \
    rspq_flush(); \
    if (!wait_for_syncpoint(sync_id, t)) \
        ASSERT(0, "display list not completed: %d/%d", rspq_check_syncpoint(sync_id), (*SP_STATUS & SP_STATUS_HALTED) != 0); \
    ASSERT_EQUAL_HEX(*SP_STATUS, SP_STATUS_HALTED | SP_STATUS_BROKE | SP_STATUS_SIG3 | SP_STATUS_SIG5 | (s), "Unexpected SP status!"); \
})

void test_rspq_queue_single(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();

    TEST_RSPQ_EPILOG(0, rspq_timeout);
}

void test_rspq_queue_multiple(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    
    rspq_noop();

    TEST_RSPQ_EPILOG(0, rspq_timeout);
}

void test_rspq_queue_rapid(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    
    rspq_noop();
    rspq_noop();
    rspq_noop();
    rspq_noop();
    rspq_noop();
    rspq_noop();
    rspq_noop();
    rspq_noop();
    rspq_noop();
    rspq_noop();
    rspq_noop();
    rspq_noop();
    rspq_noop();
    rspq_noop();

    TEST_RSPQ_EPILOG(0, rspq_timeout);
}

void test_rspq_wrap(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();

    uint32_t block_count = RSPQ_DRAM_LOWPRI_BUFFER_SIZE * 8;
    for (uint32_t i = 0; i < block_count; i++)
        rspq_noop();
    
    TEST_RSPQ_EPILOG(0, rspq_timeout);
}

void test_rspq_signal(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    
    rspq_signal(SP_WSTATUS_SET_SIG1 | SP_WSTATUS_SET_SIG2);

    TEST_RSPQ_EPILOG(SP_STATUS_SIG1 | SP_STATUS_SIG2, rspq_timeout);
}

void test_rspq_high_load(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();

    test_ovl_init();

    uint64_t expected_sum = 0;

    for (uint32_t i = 0; i < 0x1000; i++)
    {
        uint32_t x = RANDN(3);

        switch (x)
        {
            case 0:
                rspq_test_4(1);
                break;
            case 1:
                rspq_test_8(1);
                break;
            case 2:
                rspq_test_16(1);
                break;
        }

        ++expected_sum;
    }

    uint64_t actual_sum[2] __attribute__((aligned(16))) = {0};
    data_cache_hit_writeback_invalidate(actual_sum, 16);

    rspq_test_output(actual_sum);

    TEST_RSPQ_EPILOG(0, rspq_timeout);

    ASSERT_EQUAL_UNSIGNED(*actual_sum, expected_sum, "Possibly not all commands have been executed!");
}

void test_rspq_load_overlay(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    
    ugfx_init();
    DEFER(ugfx_close());

    rdp_set_env_color(0);

    TEST_RSPQ_EPILOG(0, rspq_timeout);
    
    extern uint8_t rsp_ugfx_text_start[];
    extern uint8_t rsp_ugfx_text_end[0];

    uint32_t size = rsp_ugfx_text_end - rsp_ugfx_text_start;

    ASSERT_EQUAL_MEM((uint8_t*)SP_IMEM, rsp_ugfx_text_start, size, "ugfx overlay was not loaded into IMEM!");
}

void test_rspq_switch_overlay(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    
    test_ovl_init();

    ugfx_init();
    DEFER(ugfx_close());

    rdp_set_env_color(0);
    rspq_test_16(0);

    TEST_RSPQ_EPILOG(0, rspq_timeout);

    extern rsp_ucode_t rsp_ugfx;
    extern void* rspq_overlay_get_state(rsp_ucode_t *overlay_ucode);

    ugfx_state_t *ugfx_state = UncachedAddr(rspq_overlay_get_state(&rsp_ugfx));

    uint64_t expected_commands[] = {
        RdpSetEnvColor(0)
    };

    ASSERT_EQUAL_MEM(ugfx_state->rdp_buffer, (uint8_t*)expected_commands, sizeof(expected_commands), "State was not saved!");
}

void test_rspq_multiple_flush(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    test_ovl_init();

    rspq_test_8(1);
    rspq_test_8(1);
    rspq_test_8(1);
    rspq_flush();
    wait_ms(3);
    rspq_test_8(1);
    rspq_test_8(1);
    rspq_test_8(1);
    rspq_flush();
    wait_ms(3);

    uint64_t actual_sum[2] __attribute__((aligned(16))) = {0};
    data_cache_hit_writeback_invalidate(actual_sum, 16);

    rspq_test_output(actual_sum);

    TEST_RSPQ_EPILOG(0, rspq_timeout);

    ASSERT_EQUAL_UNSIGNED(*actual_sum, 6, "Sum is incorrect!");
}


void test_rspq_sync(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    
    test_ovl_init();

    for (uint32_t i = 0; i < 100; i++)
    {
        rspq_test_8(1);
        rspq_test_wait(0x8000);
        rspq_sync();
    }

    uint64_t actual_sum[2] __attribute__((aligned(16))) = {0};
    data_cache_hit_writeback_invalidate(actual_sum, 16);

    rspq_test_output(actual_sum);

    TEST_RSPQ_EPILOG(0, rspq_timeout);

    ASSERT_EQUAL_UNSIGNED(*actual_sum, 100, "Sum is incorrect!");
}

void test_rspq_rapid_sync(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();

    rspq_syncpoint_t syncpoints[100];

    for (uint32_t i = 0; i < 100; i++)
    {
        syncpoints[i] = rspq_syncpoint();
    }

    TEST_RSPQ_EPILOG(0, rspq_timeout);

    for (uint32_t i = 0; i < 100; i++)
    {
        ASSERT(rspq_check_syncpoint(syncpoints[i]), "Not all syncpoints have been reached!");
    }
}

void test_rspq_block(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    test_ovl_init();

    rspq_block_begin();
    for (uint32_t i = 0; i < 512; i++)
        rspq_test_8(1);
    rspq_block_t *b512 = rspq_block_end();
    DEFER(rspq_block_free(b512));

    rspq_block_begin();
    for (uint32_t i = 0; i < 4; i++)
        rspq_block_run(b512);
    rspq_block_t *b2048 = rspq_block_end();
    DEFER(rspq_block_free(b2048));

    rspq_block_begin();
    rspq_block_run(b512);
    for (uint32_t i = 0; i < 512; i++)
        rspq_test_8(1);
    rspq_block_run(b2048);
    rspq_block_t *b3072 = rspq_block_end();
    DEFER(rspq_block_free(b3072));

    uint64_t actual_sum[2] __attribute__((aligned(16))) = {0};
    data_cache_hit_writeback_invalidate(actual_sum, 16);

    rspq_test_reset();
    rspq_block_run(b512);
    rspq_test_output(actual_sum);
    rspq_sync();
    ASSERT_EQUAL_UNSIGNED(*actual_sum, 512, "sum #1 is not correct");
    data_cache_hit_invalidate(actual_sum, 16);

    rspq_block_run(b512);
    rspq_test_reset();
    rspq_block_run(b512);
    rspq_test_output(actual_sum);
    rspq_sync();
    ASSERT_EQUAL_UNSIGNED(*actual_sum, 512, "sum #2 is not correct");
    data_cache_hit_invalidate(actual_sum, 16);

    rspq_test_reset();
    rspq_block_run(b2048);
    rspq_test_output(actual_sum);
    rspq_sync();
    ASSERT_EQUAL_UNSIGNED(*actual_sum, 2048, "sum #3 is not correct");
    data_cache_hit_invalidate(actual_sum, 16);

    rspq_test_reset();
    rspq_block_run(b3072);
    rspq_test_output(actual_sum);
    rspq_sync();
    ASSERT_EQUAL_UNSIGNED(*actual_sum, 3072, "sum #4 is not correct");
    data_cache_hit_invalidate(actual_sum, 16);

    rspq_test_reset();
    rspq_test_8(1);
    rspq_block_run(b3072);
    rspq_test_8(1);
    rspq_block_run(b2048);
    rspq_test_8(1);
    rspq_test_output(actual_sum);
    rspq_sync();
    ASSERT_EQUAL_UNSIGNED(*actual_sum, 5123, "sum #5 is not correct");

    TEST_RSPQ_EPILOG(0, rspq_timeout);
}

void test_rspq_wait_sync_in_block(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();

    wait_ms(3);

    rspq_syncpoint_t syncpoint = rspq_syncpoint();

    rspq_block_begin();
    DEFER(rspq_block_end());

    rspq_wait_syncpoint(syncpoint);

    // Test will block forever if it fails.
    // TODO: implement RSP exception handler that detects infinite stalls
}

void test_rspq_pause(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    
    test_ovl_init();

    for (uint32_t i = 0; i < 1000; i++)
    {
        rspq_test_4(1);
    }

    uint64_t actual_sum[2] __attribute__((aligned(16))) = {0};
    data_cache_hit_writeback_invalidate(actual_sum, 16);

    rspq_test_output(actual_sum);

    int sync_id = rspq_syncpoint();
    rspq_flush();

    unsigned long time_start = get_ticks_ms();

    bool completed = 0;
    while (get_ticks_ms() - time_start < 20000) {
        // Wait until the interrupt was raised and the SP is in idle mode
        if (rspq_check_syncpoint(sync_id) && (*SP_STATUS & SP_STATUS_HALTED)) {
            completed = 1;
            break;
        } else {
            wait_ticks(RANDN(10));
            rsp_pause(1);
            wait_ticks(100000);
            rsp_pause(0);
        }
    }

    ASSERT(completed, "display list not completed: %d/%d", rspq_check_syncpoint(sync_id), (*SP_STATUS & SP_STATUS_HALTED) != 0);
    ASSERT_EQUAL_HEX(*SP_STATUS, SP_STATUS_HALTED | SP_STATUS_BROKE | SP_STATUS_SIG3 | SP_STATUS_SIG5, "Unexpected SP status!"); \
    ASSERT_EQUAL_UNSIGNED(*actual_sum, 1000, "Sum is incorrect!");
}

// Test the basic working of highpri queue.
void test_rspq_highpri_basic(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    test_ovl_init();

    uint64_t actual_sum[2] __attribute__((aligned(16))) = {0};
    data_cache_hit_writeback_invalidate(actual_sum, 16);

    // Prepare a block of commands
    rspq_block_begin();
    for (uint32_t i = 0; i < 4096; i++) {
        rspq_test_8(1);
        if (i%256 == 0)
            rspq_test_wait(0x10);
    }
    rspq_block_t *b4096 = rspq_block_end();
    DEFER(rspq_block_free(b4096));

    // Initialize the test ucode
    rspq_test_reset();
    rspq_sync();

    // Run the block in standard queue
    rspq_block_run(b4096);
    rspq_test_output(actual_sum);
    rspq_flush();

    // Schedule a highpri queue
    rspq_highpri_begin();
        rspq_test_high(123);
        rspq_test_output(actual_sum);
    rspq_highpri_end();

    // Wait for highpri execution
    rspq_highpri_sync();

    // Verify that highpri was executed correctly and before lowpri is finished
    ASSERT(actual_sum[0] < 4096, "lowpri sum is not correct");
    ASSERT_EQUAL_UNSIGNED(actual_sum[1], 123, "highpri sum is not correct");
    data_cache_hit_invalidate(actual_sum, 16);

    // Schedule a second highpri queue
    rspq_highpri_begin();
        rspq_test_high(200);
        rspq_test_output(actual_sum);
    rspq_highpri_end();
    rspq_highpri_sync();

    // Verify that highpri was executed correctly and before lowpri is finished
    ASSERT(actual_sum[0] < 4096, "lowpri sum is not correct");
    ASSERT_EQUAL_UNSIGNED(actual_sum[1], 323, "highpri sum is not correct");
    data_cache_hit_invalidate(actual_sum, 16);

    // Wait for the end of lowpri
    rspq_sync();

    // Verify result of both queues
    ASSERT_EQUAL_UNSIGNED(actual_sum[0], 4096, "lowpri sum is not correct");
    ASSERT_EQUAL_UNSIGNED(actual_sum[1], 323, "highpri sum is not correct");

    TEST_RSPQ_EPILOG(0, rspq_timeout);
}

void test_rspq_highpri_only(TestContext *ctx)
{

}


void test_rspq_highpri_multiple(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    test_ovl_init();

    uint64_t actual_sum[2] __attribute__((aligned(16)));
    actual_sum[0] = actual_sum[1] = 0;
    data_cache_hit_writeback_invalidate(actual_sum, 16);

    rspq_block_begin();
    for (uint32_t i = 0; i < 4096; i++) {
        rspq_test_8(1);
        if (i%256 == 0)
            rspq_test_wait(0x10);
    }
    rspq_block_t *b4096 = rspq_block_end();
    DEFER(rspq_block_free(b4096));

    rspq_test_reset();
    for (int i=0;i<16;i++)
        rspq_block_run(b4096);
    rspq_flush();

    int partial = 0;
    for (int wait=1;wait<0x100;wait++) {
        debugf("wait: %x\n", wait);
        rspq_highpri_begin();
            for (uint32_t i = 0; i < 32; i++) {
                rspq_test_high(1);
                if ((i&3)==0) rspq_test_wait(wait);
            }
        rspq_highpri_end();

        rspq_highpri_begin();
            for (uint32_t i = 0; i < 32; i++) {
                rspq_test_high(3);
                if ((i&3)==0) rspq_test_wait(wait);
            }
        rspq_highpri_end();

        rspq_highpri_begin();
            for (uint32_t i = 0; i < 32; i++) {
                rspq_test_high(5);
                if ((i&3)==0) rspq_test_wait(wait);
            }
        rspq_highpri_end();

        rspq_highpri_begin();
            for (uint32_t i = 0; i < 32; i++) {
                rspq_test_high(7);
                if ((i&3)==0) rspq_test_wait(wait);
            }
        rspq_highpri_end();

        rspq_highpri_begin();
            rspq_test_output(actual_sum);
        rspq_highpri_end();

        rspq_highpri_sync();

        partial += 1*32 + 3*32 + 5*32 + 7*32;
        // ASSERT(actual_sum_ptr[0] < 4096*16, "lowpri sum is not correct");
        debugf("lowsum: %lld\n", actual_sum[0]);
        ASSERT_EQUAL_UNSIGNED(actual_sum[1], partial, "highpri sum is not correct (diff: %lld)", partial - actual_sum[1]);
        data_cache_hit_invalidate(actual_sum, 16);
    }

    rspq_test_output(actual_sum);
    rspq_sync();

    ASSERT_EQUAL_UNSIGNED(actual_sum[0], 4096*16, "lowpri sum is not correct");
    ASSERT_EQUAL_UNSIGNED(actual_sum[1], partial, "highpri sum is not correct");
}

// TODO: test syncing with overlay switching
