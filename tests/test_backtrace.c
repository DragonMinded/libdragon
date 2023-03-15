#include "backtrace.h"
#include "../src/backtrace_internal.h"
#include <alloca.h>

#define NOINLINE static __attribute__((noinline,used))
#define STACK_FRAME(n)   volatile char __stackframe[n] = {0}; (void)__stackframe;

void* bt_buf[32];
int bt_buf_len;
int (*bt_null_func_ptr)(void);
int (*bt_invalid_func_ptr)(void) = (int(*)(void))0xECECECEC;
int (*bt_misaligned_func_ptr)(void) = (int(*)(void))0x80010002;

// Test functions defined in backtrace_test.S
int btt_end(void)
{
    memset(bt_buf, 0, sizeof(bt_buf));
    bt_buf_len = backtrace(bt_buf, 32);
    return 0;
}

NOINLINE int btt_fp(void) { STACK_FRAME(128); volatile char *buf = alloca(bt_buf_len+1); buf[0] = 2; return btt_end()+1+buf[0]; }
NOINLINE int btt_dummy(void) { return 1; }

void btt_crash_handler(exception_t *exc)
{
    btt_end();
    exc->regs->epc = (uint32_t)btt_dummy;
}

#define BT_SYSCALL()       asm volatile ("syscall 0x0F001")   // Syscall for the backtrace test
#define BT_SYSCALL_FP()    asm volatile ("syscall 0x0F002")   // Syscall for the backtrace test, clobbering the frame pointer

void btt_syscall_handler(exception_t *exc, uint32_t code)
{
    volatile int ret; 
    switch (code & 0xFF) {
    case 0x02:  ret = btt_fp(); break;
    default:    ret = btt_end(); break;
    }
    (void)ret;
}

void btt_register_syscall(void)
{
    static bool registered = false;
    if (!registered) {
        register_syscall_handler(btt_syscall_handler, 0x0F001, 0x0F002);
        registered = true;
    }
}

NOINLINE int btt_b3(void) { STACK_FRAME(128);  return btt_end()+1; }
NOINLINE int btt_b2(void) { STACK_FRAME(12);   return btt_b3()+1;  }
NOINLINE int btt_b1(void) { STACK_FRAME(1024); return btt_b2()+1;  }

NOINLINE int btt_c3(void) { STACK_FRAME(128); volatile char *buf = alloca(bt_buf_len+1); return btt_end()+1+buf[0]; }
NOINLINE int btt_c2(void) { STACK_FRAME(12);   return btt_c3()+1;  }
NOINLINE int btt_c1(void) { STACK_FRAME(1024); volatile char *buf = alloca(bt_buf_len+1); return btt_c2()+1+buf[0];  }

NOINLINE int btt_d2(void) { STACK_FRAME(12); return 0; }
NOINLINE int btt_d1(void) { STACK_FRAME(16); BT_SYSCALL(); return btt_d2()+1; }

NOINLINE int btt_e2(void) { BT_SYSCALL(); return 1;  } // this is a leaf function (no stack frame)
NOINLINE int btt_e1(void) { STACK_FRAME(1024); return btt_e2()+1;  }

NOINLINE int btt_f3(void) { BT_SYSCALL_FP(); return 1;  }
NOINLINE int btt_f2(void) { STACK_FRAME(128); volatile char *buf = alloca(bt_buf_len+1); return btt_f3()+1+buf[0]; }
NOINLINE int btt_f1(void) { STACK_FRAME(1024); return btt_f2()+1;  }

NOINLINE int btt_g2(void) { STACK_FRAME(1024); return bt_null_func_ptr() + 1; }
NOINLINE int btt_g1(void) { STACK_FRAME(1024); return btt_g2()+1;  }

NOINLINE int btt_h2(void) { STACK_FRAME(1024); return bt_invalid_func_ptr() + 1; }
NOINLINE int btt_h1(void) { STACK_FRAME(1024); return btt_h2()+1;  }

NOINLINE int btt_i2(void) { STACK_FRAME(1024); return bt_misaligned_func_ptr() + 1; }
NOINLINE int btt_i1(void) { STACK_FRAME(1024); return btt_i2()+1;  }

void btt_start(TestContext *ctx, int (*func)(void), const char *expected[])
{
    bt_buf_len = 0;
    func();
    ASSERT(bt_buf_len > 0, "backtrace not called");

    int i = 0;
    void cb(void *user, backtrace_frame_t *frame)
    {
        //backtrace_frame_print(frame, stderr); debugf("\n");
        if (ctx->result == TEST_FAILED) return;
        if (expected[i] == NULL) return;
        ASSERT_EQUAL_STR(expected[i], frame->func, "invalid backtrace entry");
        i++;
    }
    backtrace_symbols_cb(bt_buf, bt_buf_len, 0, cb, NULL);
    if (expected[i] != NULL) ASSERT(0, "backtrace too short");
}

void test_backtrace_basic(TestContext *ctx)
{
    // A standard call stack
    btt_start(ctx, btt_b1, (const char*[]) {
        "btt_end", "btt_b3", "btt_b2", "btt_b1", "btt_start", NULL
    });
}

void test_backtrace_fp(TestContext *ctx)
{
    // A standard call stack where one of the function uses the frame pointer (eg: alloca)
    btt_start(ctx, btt_c1, (const char*[]) {
        "btt_end", "btt_c3", "btt_c2", "btt_c1", "btt_start", NULL
    });
}

void test_backtrace_exception(TestContext *ctx)
{
    // A call stack including an exception
    btt_register_syscall();
    btt_start(ctx, btt_d1, (const char*[]) {
        "btt_end", "btt_syscall_handler", "__onSyscallException", "<EXCEPTION HANDLER>", "btt_d1", "btt_start", NULL
    });
}

void test_backtrace_exception_leaf(TestContext *ctx)
{
    // A call stack including an exception, interrupting a leaf function
    btt_register_syscall();
    btt_start(ctx, btt_e1, (const char*[]) {
        "btt_end", "btt_syscall_handler", "__onSyscallException", "<EXCEPTION HANDLER>", "btt_e2", "btt_e1", "btt_start", NULL
    });
}

void test_backtrace_exception_fp(TestContext *ctx)
{
    // A call stack including an exception, with frame pointer being used before and after the exception
    btt_register_syscall();
    btt_start(ctx, btt_f1, (const char*[]) {
        "btt_end", "btt_fp", "btt_syscall_handler", "__onSyscallException", "<EXCEPTION HANDLER>", "btt_f3", "btt_f2", "btt_f1", "btt_start", NULL
    });
}

void test_backtrace_invalidptr(TestContext *ctx)
{
    // A call stack including an exception due to a call to invalid pointers
    exception_handler_t prev = register_exception_handler(btt_crash_handler);
    DEFER(register_exception_handler(prev));
    
    btt_start(ctx, btt_g1, (const char*[]) {
        "btt_end", "btt_crash_handler", "__onCriticalException", "<EXCEPTION HANDLER>", "<NULL POINTER>", "btt_g2", "btt_g1", "btt_start", NULL
    });
    if (ctx->result == TEST_FAILED) return;

    btt_start(ctx, btt_h1, (const char*[]) {
        "btt_end", "btt_crash_handler", "__onCriticalException", "<EXCEPTION HANDLER>", "<INVALID ADDRESS>", "btt_h2", "btt_h1", "btt_start", NULL
    });
    if (ctx->result == TEST_FAILED) return;

    btt_start(ctx, btt_i1, (const char*[]) {
        "btt_end", "btt_crash_handler", "__onCriticalException", "<EXCEPTION HANDLER>", "<INVALID ADDRESS>", "btt_i2", "btt_i1", "btt_start", NULL
    });
    if (ctx->result == TEST_FAILED) return;
}

void test_backtrace_analyze(TestContext *ctx)
{
    bt_func_t func; bool ret;

    extern uint32_t test_bt_1_start[];
    ret = __bt_analyze_func(&func, test_bt_1_start, 0, false);
    ASSERT(ret, "bt_analyze failed");
    ASSERT_EQUAL_UNSIGNED(func.type, BT_FUNCTION, "invalid function type");
    ASSERT_EQUAL_UNSIGNED(func.stack_size, 112, "invalid stack size");
    ASSERT_EQUAL_UNSIGNED(func.ra_offset, 104+4, "invalid RA offset");
    ASSERT_EQUAL_UNSIGNED(func.fp_offset, 96+4, "invalid FP offset");

    extern uint32_t test_bt_2_start[];
    ret = __bt_analyze_func(&func, test_bt_2_start, 0, false);
    ASSERT(ret, "bt_analyze failed");
    ASSERT_EQUAL_UNSIGNED(func.type, BT_FUNCTION_FRAMEPOINTER, "invalid function type");
    ASSERT_EQUAL_UNSIGNED(func.stack_size, 128, "invalid stack size");
    ASSERT_EQUAL_UNSIGNED(func.ra_offset, 120+4, "invalid RA offset");
    ASSERT_EQUAL_UNSIGNED(func.fp_offset, 112+4, "invalid FP offset");

    extern uint32_t test_bt_3_start[];
    ret = __bt_analyze_func(&func, test_bt_3_start, 0, false);
    ASSERT(ret, "bt_analyze failed");
    ASSERT_EQUAL_UNSIGNED(func.type, BT_FUNCTION, "invalid function type");
    ASSERT_EQUAL_UNSIGNED(func.stack_size, 80, "invalid stack size");
    ASSERT_EQUAL_UNSIGNED(func.ra_offset, 20+4, "invalid RA offset");
    ASSERT_EQUAL_UNSIGNED(func.fp_offset, 16+4, "invalid FP offset");

    extern uint32_t test_bt_4_start[];
    ret = __bt_analyze_func(&func, test_bt_4_start, 0, true);
    ASSERT(ret, "bt_analyze failed");
    ASSERT_EQUAL_UNSIGNED(func.type, BT_LEAF, "invalid function type");
    ASSERT_EQUAL_UNSIGNED(func.stack_size, 0, "invalid stack size");
    ASSERT_EQUAL_UNSIGNED(func.ra_offset, 0, "invalid RA offset");
    ASSERT_EQUAL_UNSIGNED(func.fp_offset, 0, "invalid FP offset");

    extern uint32_t test_bt_5_start[], test_bt_5[];
    ret = __bt_analyze_func(&func, test_bt_5_start, (uint32_t)test_bt_5, true);
    ASSERT(ret, "bt_analyze failed");
    ASSERT_EQUAL_UNSIGNED(func.type, BT_LEAF, "invalid function type");
    ASSERT_EQUAL_UNSIGNED(func.stack_size, 0, "invalid stack size");
    ASSERT_EQUAL_UNSIGNED(func.ra_offset, 0, "invalid RA offset");
    ASSERT_EQUAL_UNSIGNED(func.fp_offset, 0, "invalid FP offset");
}
