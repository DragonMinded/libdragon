#include <malloc.h>
#include <string.h>

#include <rspq.h>
#include <rspq_constants.h>
#include <rdp.h>
#include <rdpq_constants.h>
#include "test_rspq_constants.h"

#define ASSERT_GP_BACKWARD           0xF001   // Also defined in rsp_test.S
#define ASSERT_TOO_MANY_NOPS         0xF002

static void test_assert_handler(rsp_snapshot_t *state, uint16_t assert_code)
{
    switch (assert_code) {
        case ASSERT_GP_BACKWARD:
            printf("GP moved backward\n");
            break;
        case ASSERT_TOO_MANY_NOPS:
            printf("Trying to send too many NOPs (%ld)\n", state->gpr[4]);
            break;
        default:
            printf("Unknown assert\n");
            break;
    }
}

DEFINE_RSP_UCODE(rsp_test, 
    .assert_handler = test_assert_handler);

DEFINE_RSP_UCODE(rsp_test2);

static uint32_t test_ovl_id;
static uint32_t test2_ovl_id;

void test_ovl_init()
{   
    void *test_ovl_state = UncachedAddr(rspq_overlay_get_state(&rsp_test));
    memset(test_ovl_state, 0, sizeof(uint32_t) * 2);

    rspq_init();
    test_ovl_id = rspq_overlay_register(&rsp_test);
    test2_ovl_id = rspq_overlay_register(&rsp_test2);
}

void test_ovl_close()
{
    rspq_overlay_unregister(test2_ovl_id);
    rspq_overlay_unregister(test_ovl_id);
}

void rspq_test_4(uint32_t value)
{
    rspq_write(test_ovl_id, 0x0, value & 0x00FFFFFF);
}

void rspq_test_8(uint32_t value)
{
    rspq_write(test_ovl_id, 0x1, value & 0x00FFFFFF,
        0x02000000 | SP_WSTATUS_SET_SIG0);
}

void rspq_test_16(uint32_t value)
{
    rspq_write(test_ovl_id, 0x2, value & 0x00FFFFFF, 
        0x02000000 | SP_WSTATUS_SET_SIG0,
        0x02000000 | SP_WSTATUS_SET_SIG1,
        0x02000000 | SP_WSTATUS_SET_SIG0);
}

void rspq_test_wait(uint32_t length)
{
    rspq_write(test_ovl_id, 0x3, 0, length);
}

void rspq_test_output(uint64_t *dest)
{
    rspq_write(test_ovl_id, 0x4, 0, PhysicalAddr(dest));
}

void rspq_test_reset(void)
{
    rspq_write(test_ovl_id, 0x5);
}

void rspq_test_high(uint32_t value)
{
    rspq_write(test_ovl_id, 0x6, value & 0x00FFFFFF);
}

void rspq_test_reset_log(void)
{
    rspq_write(test_ovl_id, 0x7);
}

void rspq_test_send_rdp(uint32_t value)
{
    rdpq_write(1, test_ovl_id, 0xA, 0, value);
}

void rspq_test_send_rdp_nops(int num_nops)
{
    rdpq_write(num_nops, test_ovl_id, 0xB, num_nops);
}

void rspq_test_big_out(void *dest)
{
    rspq_write(test_ovl_id, 0x9, 0, PhysicalAddr(dest));
}

void rspq_test2(uint32_t v0, uint32_t v1)
{
    rspq_write(test2_ovl_id, 0x0, v0, v1);
}

#define RSPQ_LOG_STATUS(step) debugf("STATUS: %#010lx, PC: %#010lx (%s)\n", *SP_STATUS, *SP_PC, step)

void dump_mem(void* ptr, uint32_t size)
{
    for (uint32_t i = 0; i < size / sizeof(uint32_t); i += 8)
    {
        uint32_t *ints = ptr + i * sizeof(uint32_t);
        debugf("%08lX: %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX\n",
            (uint32_t)(ints) - (uint32_t)(ptr), ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], ints[6], ints[7]);
    }
}

bool wait_for_syncpoint(int sync_id, unsigned long timeout)
{
    unsigned long time_start = get_ticks_ms();

    while (get_ticks_ms() - time_start < timeout) {
        // Wait until the interrupt was raised and the SP is in idle mode
        if (rspq_syncpoint_check(sync_id) && (*SP_STATUS & SP_STATUS_HALTED)) {
            return true;
        }
        // Check if the RSP has hit an assert, and if so report it.
        __rsp_check_assert(__FILE__, __LINE__, __func__);
    }
    return false;
}

#define TEST_RSPQ_PROLOG() \
    rspq_init(); \
    DEFER(rspq_close());

const unsigned long rspq_timeout = 100;

#define ASSERT_RSPQ_EPILOG_SP_STATUS(s) \
    ASSERT_EQUAL_HEX(*SP_STATUS, SP_STATUS_HALTED | SP_STATUS_BROKE | SP_STATUS_SIG_BUFDONE_LOW | SP_STATUS_SIG_BUFDONE_HIGH | (s), "Unexpected SP status!")

#define TEST_RSPQ_EPILOG(s, t) ({ \
    int sync_id = rspq_syncpoint_new(); \
    rspq_flush(); \
    if (!wait_for_syncpoint(sync_id, t)) \
        ASSERT(0, "display list not completed: %d/%d", rspq_syncpoint_check(sync_id), (*SP_STATUS & SP_STATUS_HALTED) != 0); \
    ASSERT_RSPQ_EPILOG_SP_STATUS((s)); \
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

void test_rspq_high_load(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();

    test_ovl_init();
    DEFER(test_ovl_close());

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

void test_rspq_flush(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();

    test_ovl_init();
    DEFER(test_ovl_close());

    // This is meant to verify that the fix in rspq_flush actually
    // prevents the race condition (see the comment in that function).
    // If the race condition does happen, this test will fail very quickly.
    uint32_t t0 = TICKS_READ();
    while (TICKS_DISTANCE(t0, TICKS_READ()) < TICKS_FROM_MS(1000)) {
        rspq_test_wait(RANDN(50));
        rspq_flush();

        wait_ticks(80 + RANDN(20));

        rspq_syncpoint_t sp = rspq_syncpoint_new();
        rspq_flush();
        ASSERT(wait_for_syncpoint(sp, 100), "syncpoint was not flushed!, PC:%03lx, STATUS:%04lx", *SP_PC, *SP_STATUS);
    }

    TEST_RSPQ_EPILOG(0, rspq_timeout);
}

void test_rspq_rapid_flush(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    
    test_ovl_init();
    DEFER(test_ovl_close());

    // This test is meant to verify that a specific hardware bug
    // does not occur (see rsp_queue.inc). The exact conditions
    // for the bug to happen are not known and this test setup was
    // found by pure experimentation.

    uint64_t actual_sum[2] __attribute__((aligned(16))) = {0};
    data_cache_hit_writeback_invalidate(actual_sum, 16);

    uint32_t t0 = TICKS_READ();
    while (TICKS_DISTANCE(t0, TICKS_READ()) < TICKS_FROM_MS(10000)) {
        for (int wait=1;wait<0x100;wait++) {
            uint64_t expected_sum = 1*24 + 3*24 + 5*24 + 7*24;

            rspq_flush();
            rspq_test_reset_log();
            rspq_test_reset();
            for (uint32_t i = 0; i < 24; i++)
            {
                rspq_test_high(1);
                if ((i&3)==0) rspq_test_wait(RANDN(wait));
            }
            rspq_flush();

            rspq_flush();
            for (uint32_t i = 0; i < 24; i++)
            {
                rspq_test_high(3);
                if ((i&3)==0) rspq_test_wait(RANDN(wait));
            }
            rspq_flush();

            rspq_flush();
            for (uint32_t i = 0; i < 24; i++)
            {
                rspq_test_high(5);
                if ((i&3)==0) rspq_test_wait(RANDN(wait));
            }
            rspq_flush();

            rspq_flush();
            for (uint32_t i = 0; i < 24; i++)
            {
                rspq_test_high(7);
                if ((i&3)==0) rspq_test_wait(RANDN(wait));
            }
            rspq_flush();

            rspq_flush();
            rspq_test_output(actual_sum);
            rspq_wait();

            ASSERT_EQUAL_UNSIGNED(actual_sum[1], expected_sum, "Sum is incorrect! (diff: %lld)", expected_sum - actual_sum[1]);
            data_cache_hit_invalidate(actual_sum, 16);
        }
    }

    TEST_RSPQ_EPILOG(0, rspq_timeout);
}

void test_rspq_load_overlay(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    
    test_ovl_init();
    DEFER(test_ovl_close());

    rspq_test_4(0);

    TEST_RSPQ_EPILOG(0, rspq_timeout);

    uint32_t size = rsp_test_text_end - rsp_test_text_start;

    ASSERT_EQUAL_MEM((uint8_t*)SP_IMEM, rsp_test_text_start, size, "test overlay was not loaded into IMEM!");
}

void test_rspq_switch_overlay(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    
    test_ovl_init();
    DEFER(test_ovl_close());

    rspq_test2(0x123456, 0x87654321);
    rspq_test_16(0);

    TEST_RSPQ_EPILOG(0, rspq_timeout);

    uint8_t *test2_state = UncachedAddr(rspq_overlay_get_state(&rsp_test2));

    uint32_t expected_state[] = {
        test2_ovl_id | 0x123456,
        0x87654321
    };

    ASSERT_EQUAL_MEM(test2_state, (uint8_t*)expected_state, sizeof(expected_state), "State was not saved!");
}

void test_rspq_multiple_flush(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    test_ovl_init();
    DEFER(test_ovl_close());

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


void test_rspq_wait(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    
    test_ovl_init();
    DEFER(test_ovl_close());

    for (uint32_t i = 0; i < 100; i++)
    {
        rspq_test_8(1);
        rspq_test_wait(0x8000);
        rspq_wait();
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
        syncpoints[i] = rspq_syncpoint_new();
    }

    TEST_RSPQ_EPILOG(0, rspq_timeout);

    for (uint32_t i = 0; i < 100; i++)
    {
        ASSERT(rspq_syncpoint_check(syncpoints[i]), "Not all syncpoints have been reached!");
    }
}

void test_rspq_block(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    test_ovl_init();
    DEFER(test_ovl_close());

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
    rspq_wait();
    ASSERT_EQUAL_UNSIGNED(*actual_sum, 512, "sum #1 is not correct");
    data_cache_hit_invalidate(actual_sum, 16);

    rspq_block_run(b512);
    rspq_test_reset();
    rspq_block_run(b512);
    rspq_test_output(actual_sum);
    rspq_wait();
    ASSERT_EQUAL_UNSIGNED(*actual_sum, 512, "sum #2 is not correct");
    data_cache_hit_invalidate(actual_sum, 16);

    rspq_test_reset();
    rspq_block_run(b2048);
    rspq_test_output(actual_sum);
    rspq_wait();
    ASSERT_EQUAL_UNSIGNED(*actual_sum, 2048, "sum #3 is not correct");
    data_cache_hit_invalidate(actual_sum, 16);

    rspq_test_reset();
    rspq_block_run(b3072);
    rspq_test_output(actual_sum);
    rspq_wait();
    ASSERT_EQUAL_UNSIGNED(*actual_sum, 3072, "sum #4 is not correct");
    data_cache_hit_invalidate(actual_sum, 16);

    rspq_test_reset();
    rspq_test_8(1);
    rspq_block_run(b3072);
    rspq_test_8(1);
    rspq_block_run(b2048);
    rspq_test_8(1);
    rspq_test_output(actual_sum);
    rspq_wait();
    ASSERT_EQUAL_UNSIGNED(*actual_sum, 5123, "sum #5 is not correct");

    TEST_RSPQ_EPILOG(0, rspq_timeout);
}

void test_rspq_wait_sync_in_block(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();

    wait_ms(3);

    rspq_syncpoint_t syncpoint = rspq_syncpoint_new();

    rspq_block_begin();
    DEFER(rspq_block_end());

    rspq_syncpoint_wait(syncpoint);

    // Test will cause an RSP crash (timeout) if it fails.
}

// Test the basic working of highpri queue.
void test_rspq_highpri_basic(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    test_ovl_init();
    DEFER(test_ovl_close());

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
    rspq_wait();

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
    rspq_wait();

    // Verify result of both queues
    ASSERT_EQUAL_UNSIGNED(actual_sum[0], 4096, "lowpri sum is not correct");
    ASSERT_EQUAL_UNSIGNED(actual_sum[1], 323, "highpri sum is not correct");

    TEST_RSPQ_EPILOG(0, rspq_timeout);
}

void test_rspq_highpri_multiple(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    test_ovl_init();
    DEFER(test_ovl_close());

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

    uint32_t t0 = TICKS_READ();
    while (TICKS_DISTANCE(t0, TICKS_READ()) < TICKS_FROM_MS(2000)) {
        for (int wait=1;wait<0x100;wait++) {
            int partial = 0;
            rspq_highpri_begin();
                rspq_test_reset_log();
                rspq_test_reset();
                for (uint32_t i = 0; i < 24; i++) {
                    rspq_test_high(1);
                    if ((i&3)==0) rspq_test_wait(RANDN(wait));
                }
                rspq_flush();
            rspq_highpri_end();

            rspq_highpri_begin();
                for (uint32_t i = 0; i < 24; i++) {
                    rspq_test_high(3);
                    // if ((i&3)==0) rspq_test_wait(RANDN(wait));
                }
            rspq_highpri_end();

            rspq_highpri_begin();
                for (uint32_t i = 0; i < 24; i++) {
                    rspq_test_high(5);
                    // if ((i&3)==0) rspq_test_wait(RANDN(wait));
                }
            rspq_highpri_end();

            rspq_highpri_begin();
                for (uint32_t i = 0; i < 24; i++) {
                    rspq_test_high(7);
                    if ((i&3)==0) rspq_test_wait(RANDN(wait));
                }
            rspq_highpri_end();

            rspq_highpri_begin();
                rspq_test_output(actual_sum);
            rspq_highpri_end();

            rspq_highpri_sync();

            partial += 1*24 + 3*24 + 5*24 + 7*24;
            if (actual_sum[1] != partial) {
                *SP_STATUS = SP_WSTATUS_SET_HALT;
                MEMORY_BARRIER();
                wait_ms(10);
                for (int i=0;i<128;i++) {
                    debugf("%lx %lx %ld %ld\n", SP_DMEM[512+i*4+0], SP_DMEM[512+i*4+1], SP_DMEM[512+i*4+2], SP_DMEM[512+i*4+3]);
                }
                ASSERT_EQUAL_UNSIGNED(actual_sum[1], partial, "highpri sum is not correct (diff: %lld)", partial - actual_sum[1]);
            }

            ASSERT_EQUAL_UNSIGNED(actual_sum[1], partial, "highpri sum is not correct (diff: %lld)", partial - actual_sum[1]);
            data_cache_hit_invalidate(actual_sum, 16);
        }
    }

    rspq_test_output(actual_sum);
    TEST_RSPQ_EPILOG(0, rspq_timeout);

    // ASSERT_EQUAL_UNSIGNED(actual_sum[0], 4096*16, "lowpri sum is not correct");
    // ASSERT_EQUAL_UNSIGNED(actual_sum[1], partial, "highpri sum is not correct");
}

// Test that an overlay only used in highpri is correctly loaded
void test_rspq_highpri_overlay(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    test_ovl_init();
    DEFER(test_ovl_close());

    uint64_t actual_sum[2] __attribute__((aligned(16)));
    actual_sum[0] = actual_sum[1] = 0;
    data_cache_hit_writeback_invalidate(actual_sum, 16);

    rspq_highpri_begin();
        rspq_test_reset();
        rspq_test_high(123);
        rspq_test_output(actual_sum);
    rspq_highpri_end();
    rspq_wait();
        
    ASSERT_EQUAL_UNSIGNED(actual_sum[1], 123, "highpri sum is not correct");
    TEST_RSPQ_EPILOG(0, rspq_timeout);
}

void test_rspq_big_command(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    test_ovl_init();
    DEFER(test_ovl_close());

    uint32_t values[TEST_RSPQ_BIG_PAYLOAD_WORDS];
    for (uint32_t i = 0; i < TEST_RSPQ_BIG_PAYLOAD_WORDS; i++)
    {
        values[i] = RANDN(0xFFFFFFFF);
    }
    

    uint32_t output[TEST_RSPQ_BIG_PAYLOAD_WORDS] __attribute__((aligned(16)));
    data_cache_hit_writeback_invalidate(output, TEST_RSPQ_BIG_PAYLOAD_SIZE);

    rspq_write_t wptr = rspq_write_begin(test_ovl_id, 0x8, TEST_RSPQ_BIG_COMMAND_SIZE);
    rspq_write_arg(&wptr, 0);
    for (uint32_t i = 0; i < TEST_RSPQ_BIG_PAYLOAD_WORDS; i++)
    {
        rspq_write_arg(&wptr, i | i << 8 | i << 16 | i << 24);
    }
    rspq_write_end(&wptr);

    wptr = rspq_write_begin(test_ovl_id, 0x8, TEST_RSPQ_BIG_COMMAND_SIZE);
    rspq_write_arg(&wptr, 0);
    for (uint32_t i = 0; i < TEST_RSPQ_BIG_PAYLOAD_WORDS; i++)
    {
        rspq_write_arg(&wptr, values[i]);
    }
    rspq_write_end(&wptr);

    rspq_test_big_out(output);

    TEST_RSPQ_EPILOG(0, rspq_timeout);

    uint32_t expected[TEST_RSPQ_BIG_PAYLOAD_WORDS];
    for (uint32_t i = 0; i < TEST_RSPQ_BIG_PAYLOAD_WORDS; i++)
    {
        uint32_t x = i | i << 8 | i << 16 | i << 24;
        expected[i] = x ^ values[i];
    }
    
    ASSERT_EQUAL_MEM((uint8_t*)output, (uint8_t*)expected, TEST_RSPQ_BIG_PAYLOAD_SIZE, "Output does not match!");
}

void test_rspq_rdp_dynamic(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    test_ovl_init();

    const uint32_t count = 0x80;

    for (uint32_t i = 0; i < count; i++)
    {
        rspq_test_send_rdp(i);
    }

    TEST_RSPQ_EPILOG(0, rspq_timeout);

    extern void *rspq_rdp_dynamic_buffers[2];

    ASSERT_EQUAL_HEX(*DP_START, PhysicalAddr(rspq_rdp_dynamic_buffers[0]), "DP_START does not match!");
    ASSERT_EQUAL_HEX(*DP_END, PhysicalAddr(rspq_rdp_dynamic_buffers[0]) + count * 8, "DP_END does not match!");

    uint64_t *rdp_buf = (uint64_t*)rspq_rdp_dynamic_buffers[0];

    for (uint64_t i = 0; i < count; i++)
    {
        ASSERT_EQUAL_HEX(rdp_buf[i], i, "Wrong command at idx: %llx", i);
    }
}

void test_rspq_rdp_dynamic_switch(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    test_ovl_init();

    const uint32_t full_count = RDPQ_DYNAMIC_BUFFER_SIZE / 8;
    const uint32_t extra_count = 8;
    const uint32_t count = full_count + extra_count;

    for (uint32_t i = 0; i < count; i++)
    {
        rspq_test_send_rdp(i);
    }

    TEST_RSPQ_EPILOG(0, rspq_timeout);

    extern void *rspq_rdp_dynamic_buffers[2];

    ASSERT_EQUAL_HEX(*DP_START, PhysicalAddr(rspq_rdp_dynamic_buffers[1]), "DP_START does not match!");
    ASSERT_EQUAL_HEX(*DP_END, PhysicalAddr(rspq_rdp_dynamic_buffers[1]) + extra_count * 8, "DP_END does not match!");

    uint64_t *rdp_buf0 = (uint64_t*)rspq_rdp_dynamic_buffers[0];
    uint64_t *rdp_buf1 = (uint64_t*)rspq_rdp_dynamic_buffers[1];

    for (uint64_t i = 0; i < full_count; i++)
    {
        ASSERT_EQUAL_HEX(rdp_buf0[i], i, "Wrong command at idx: %llx", i);
    }

    for (uint64_t i = 0; i < extra_count; i++)
    {
        ASSERT_EQUAL_HEX(rdp_buf1[i], i + full_count, "Wrong command at idx: %llx", i);
    }
}


void test_rspq_deferred_call(TestContext *ctx)
{
    TEST_RSPQ_PROLOG();
    test_ovl_init();

    int num_call_expected = 0;
    int num_call_found = 0;

    uint64_t *actual_sum = malloc_uncached(2 * sizeof(uint64_t));
    DEFER(free_uncached(actual_sum));

    int value = 0;

    void cb1(void* expectedp) {
        ++num_call_found;
        int exp = (int)expectedp;
        volatile uint64_t cur_counter = actual_sum[0];
        ASSERT(cur_counter >= exp, "invalid sequence for deferred call (expected %d, got %d)", exp, (int)cur_counter);
    }

    rspq_test_reset();

    SRAND(123);
    for (int i=0;i<1000;i++) {
        switch (RANDN(8)) {
        case 0: case 1: case 2: {
            rspq_test_4(1); value+=1;
        }   break;
        case 3: {
            rspq_test_output(actual_sum);
            rspq_syncpoint_new_cb(cb1, (void*)value);
            num_call_expected++;
        }   break;
        case 4: case 5: {
            int count = RANDN(RSPQ_DRAM_LOWPRI_BUFFER_SIZE / 16);
            for (int j=0;j<count;j++)
                rspq_noop();
        }   break;
        case 6: case 7: {
            rspq_flush();
        }
        }
        if (ctx->result == TEST_FAILED)
            return;
    }

    rspq_wait();
    if (ctx->result == TEST_FAILED)
        return;

    ASSERT_EQUAL_UNSIGNED(num_call_found, num_call_expected, "invalid number of deferred calls");
}
